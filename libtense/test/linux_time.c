#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <syscall.h>
#include <unistd.h>
#include "../tense.h"
#include <sched.h>

#define ONE_BILLION 1000000000
// Execution modes
#define NORMAL 1
#define SCALED 2
#define CHILD 3
#define CHILDREN 4
#define SLEEP 5
#define SLEEP_SCALED 6

#define handle(code) do { \
    if(code == -1) {\
        fprintf(stderr, #code); \
        return -1; \
    } \
} while(0)
#define timespec_delta(s, e) ((e.tv_sec - s.tv_sec) * ONE_BILLION + (e.tv_nsec - s.tv_nsec))

#define gettime(mode, type, code) do \
{ \
    struct timespec start, end; \
    if (clock_gettime(CLOCK_ ## type ## _CPUTIME_ID, &start) == -1) { \
        fprintf(stderr, "failed to get time\n"); \
        return -1; \
    } \
    {code}; \
    if (clock_gettime(CLOCK_ ## type ## _CPUTIME_ID, &end) == -1) { \
        fprintf(stderr, "failed to get time\n"); \
        return -1; \
    } \
    printf(# mode " %li\n", timespec_delta(start, end)); \
} while(0)

int normal(void) {
    gettime(Normal, THREAD,
            for (int i = 0; i < 10000000; ++i)
                if (i % 100000 == 0) {
                    printf("Normal %d\n", i);
//                    sched_yield();
                }
    );
    return 0;
}

int normal_tense(void) {
    return normal();
}

int scaled(void) {
    handle(tense_scale_percent(200));
    gettime(Scaled, THREAD,
            for (int i = 0; i < 10000000; ++i)
                if (i % 100000 == 0) {
                    printf("Scaled %d\n", i);
//                    sched_yield();
                }
    );
    return 0;
}

void * __child_normal(void *arg)
{
    printf("Normal tid %li\n", syscall(__NR_gettid));
    *(int *)arg = normal_tense();
    return NULL;
}

void * __child_scaled(void *arg)
{
    printf("Scaled tid %li\n", syscall(__NR_gettid));
    *(int *)arg = scaled();
    return NULL;
}

void * __child_sleepy(void *arg) {

    struct timespec start, ten_ms = {
            .tv_sec = 0,
            .tv_nsec = 10000000
    };

    printf("Sleepy tid %li\n", syscall(__NR_gettid));


    if (clock_gettime(CLOCK_MONOTONIC_RAW, &start) == -1) {
        fprintf(stderr, "failed to get time\n");
    }

    nanosleep(&ten_ms, NULL);

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ten_ms) == -1) {
        fprintf(stderr, "failed to get time\n");
    }

    printf("Slept for %li ns during 10 ms sleep\n", timespec_delta(start, ten_ms));

    return NULL;
}

int child(void) {
    pthread_t child;
    int ret;

    gettime(Parent, PROCESS, {
        if(pthread_create(&child, NULL, __child_normal, &ret)) {
            return -1;
        }

        if(pthread_join(child, NULL)) {
            return -1;
        }
    });

    return ret;
}

int children(void) {
    pthread_t child_normal, child_scaled;
    int ret;

    gettime(Parent, PROCESS, {
        if(pthread_create(&child_scaled, NULL, __child_scaled, &ret)) {
            return -1;
        }

        if(pthread_create(&child_normal, NULL, __child_normal, &ret)) {
            return -1;
        }



        if(pthread_join(child_normal, NULL)) {
            return -1;
        }

        if(pthread_join(child_scaled, NULL)) {
            return -1;
        }
    });

    return ret;
}

int sleep_child(void * (*child_func) (void *)) {
    pthread_t child, child_sleepy;
    int ret;

    gettime(Parent, PROCESS, {
        if (pthread_create(&child, NULL, child_func, &ret)) {
            return -1;
        }

        if (pthread_create(&child_sleepy, NULL, __child_sleepy, &ret)) {
            return -1;
        }

        if (pthread_join(child, NULL)) {
            return -1;
        }

        if (pthread_join(child_sleepy, NULL)) {
            return -1;
        }
    });
    return ret;
}

int main(int argc, char **argv)
{
    int mode = NORMAL;
    if (argc == 2)
        mode = atoi(argv[1]);

    switch (mode) {
        case SLEEP_SCALED: return sleep_child(__child_scaled);
        case SLEEP: return sleep_child(__child_normal);
        case CHILDREN: return children();
        case CHILD: return child();
        case SCALED: return scaled();
        case NORMAL:
        default: return normal();
    }
}