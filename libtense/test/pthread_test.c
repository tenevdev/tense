#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <syscall.h>
#include <dirent.h>
#include <fcntl.h>
#include "../tense.h"

int ls(int pid) {
    char proc_dir_buff[20];
    DIR *proc_dir;
    struct dirent *walk;
    sprintf(proc_dir_buff, "/proc/%d/task", pid);
    proc_dir = opendir(proc_dir_buff);

    if (!proc_dir)
        return -1;

    while ((walk = readdir(proc_dir)) != NULL) {
        printf("%s\n", walk->d_name);
    }

    closedir(proc_dir);
    return 0;
}

int get_vruntime(long tid) {
    char path[20];
    sprintf(path, "/proc/%li/sched", tid);
    int fd_sched = open(path, O_RDONLY);
    if (fd_sched < 0)
        return -1;

    size_t content_len = 450;
    char content[content_len];
    char * vruntime_line = content;

    read(fd_sched, content, content_len);
    for (int line = 0; line < 3; ++line)
        while (*vruntime_line++ != '\n');

    printf("%.200s\n", vruntime_line);
    close(fd_sched);
}

#define do_work for(int i = 0; i < 10000; i++)

void *thread_stat(void * i)
{
    if (tense_init() == -1)
        printf("Error in tense\n");
    printf("Thread %d > PID: %d | TID %li \n",
           *(int *) i, getpid(), syscall(__NR_gettid));
    get_vruntime(syscall(__NR_gettid));

    tense_scale_percent(15);
    do_work;
    tense_scale_percent(45);
    do_work;
    tense_clear();
    do_work;
    return NULL;

}

int main()
{
    printf("Main > PID: %d | TID %li \n", getpid(), syscall(__NR_gettid));

    pthread_t child;

    for (int i = 0; i < 3; ++i) {
        if(pthread_create(&child, NULL, thread_stat, &i)) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }

        ls(getpid());

        if(pthread_join(child, NULL)) {

            fprintf(stderr, "Error joining thread\n");
            return 2;

        }
    }

    get_vruntime(syscall(__NR_gettid));

    return 0;
}