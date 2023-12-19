#include <dirent.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

<<<<<<< HEAD
    // gcc  multi-process-producer-consumer.c -lpthread
=======
    // gcc  multi-process-disk-use.c -lpthread
>>>>>>> ae7c601 (like du)

    typedef struct my_shared_st {
    sem_t *mutex; // 互斥信号量，一次只有一个进程更新共享内存
    off_t used_space; // 文件占用的空间大小，单位：字节
} my_shared_t;
#define SHM_SIZE sizeof(struct my_shared_st)

static void
child_do(const char *path, int shmid)
{
    struct dirent *entry;
    DIR           *dir;
    struct stat    stat_buf;
    off_t          used_space = 0;

    dir = opendir(path);
    if (dir == NULL) {
        perror("error opening directory");
        exit(EXIT_FAILURE);
    }

    // 计算目录下文件的使用大小
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0
            || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (lstat(full_path, &stat_buf) == 0) {
            if (S_ISREG(stat_buf.st_mode)) {
                used_space += stat_buf.st_size;
            } else if (S_ISDIR(stat_buf.st_mode)) {
                // used_space += calculate_used_space(full_path);
                pid_t pid = fork();
                if (pid == -1) {
                    perror("fork error");
                    exit(EXIT_FAILURE);
                }
                if (pid == 0) { // 子进程统计新的目录
                    child_do(full_path, shmid);
                }
                wait(NULL); // 父进程等子进程结束
            } else if (S_ISLNK(stat_buf.st_mode)) {
                // If it's a symbolic link, add the size of the link itself
                used_space += stat_buf.st_size;
            }
        } else {
            perror("Error getting file/directory information");
        }
    }
    closedir(dir);
    // printf("child path=%s: %lu\n", path, used_space);

    // 子进程获取共享内存空间地址
    struct my_shared_st *shared_spaces = shmat(shmid, NULL, 0);
    if (shared_spaces == (void *)-1) {
        perror("child attach share memory failed");
        exit(EXIT_FAILURE);
    }

    sem_wait(shared_spaces->mutex);
    shared_spaces->used_space += used_space;
    sem_post(shared_spaces->mutex);
    // 子进程放弃使用共享内存
    shmdt(shared_spaces);
    exit(EXIT_SUCCESS);
}

static void
parant_do(const char *path, int shmid)
{
    wait(NULL); // 等待所有子进程结束

    // 父进程获取共享内存空间地址
    struct my_shared_st *shared_spaces = shmat(shmid, NULL, 0);
    if (shared_spaces == (void *)-1) {
        perror("parant attach share memory failed");
        exit(EXIT_FAILURE);
    }

    if (shared_spaces->used_space > 0) {
        printf("Total disk space usage in %s: %lu bytes\n", path,
               shared_spaces->used_space);
    }

    // 父进程放弃使用共享内存
    shmdt(shared_spaces);
    // 销毁父子进程都不再使用的共享内存
    shmctl(shmid, IPC_RMID, NULL);
    exit(EXIT_SUCCESS);
}
int
main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *target_path = argv[1]; // 命令行指定目标目录

    // 申请一个 my_shared_t 大小的共享空间
    int shmid = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0600);
    if (shmid == -1) {
        perror("shamget failed");
        exit(EXIT_FAILURE);
    }

    struct my_shared_st *shared_spaces = shmat(shmid, NULL, 0);
    if (shared_spaces == (void *)-1) {
        perror("parant attach share memory failed");
        exit(EXIT_FAILURE);
    }
    shared_spaces->mutex = sem_open("/my_mutex_sem", O_CREAT, 0600, 1);
    if (shared_spaces->mutex == SEM_FAILED) {
        perror("semphore initialization failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork(); // 创建子进程
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        child_do(target_path, shmid);
    } else {
        parant_do(target_path, shmid);
    }

    return EXIT_SUCCESS;
}
