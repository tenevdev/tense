#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>

#include "tense.h"

static int enabled;

/*
 * init_preload is added to the .init_array section (see below)
 *
 * Its purpose is to check if tense should be enabled for the given executable.
 * The idea is to read from a file containing whitelisted executable names or
 * maybe require "tense" to appear in argv[0].
 */
int init_preload(int argc, char **argv, char **env) {
    if (getenv("TENSE")) {
        fprintf(stderr, "Preload of %s\n", argv[0]);
        enabled = 1;
    }
    else {
        fprintf(stderr, "Linked against libtense but not enabled\n");
        enabled = 0;
    }
    return 0;
}

__attribute__((section(".init_array"))) static void *tense_preload_constructor = &init_preload;

int (*pthread_create_real) (pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg) = NULL;
int (*clock_gettime_real) (clockid_t clk_id, struct timespec *tp) = NULL;
int (*nanosleep_real) (const struct timespec *req, struct timespec *rem) = NULL;

struct start {
    void *(*start_routine) (void *);
    void *arg;
};

#define ONE_BILLION  1000000000L

void * start_routine_wrapper(void *arg) {
    struct start * start_struct = (struct start *)arg;

    if(tense_init() == -1) {
        fprintf(stderr, "TENSE failed to initialize tense\n");
        return NULL;
    }


    struct timespec t_start, t_end, tense_start, tense_end;


    tense_time(&tense_start);

    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_start) == -1) {
        fprintf(stderr, "failed to get time\n");
    }

    fprintf(stderr,"pid %d calling start_routine from wrapper\n", getpid());
    void *ret = start_struct->start_routine(start_struct->arg);


    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_end) == -1) {
        fprintf(stderr, "failed to get time\n");
    }

    tense_time(&tense_end);

    long long delta = t_end.tv_sec * ONE_BILLION + t_end.tv_nsec - t_start.tv_sec * ONE_BILLION - t_start.tv_nsec;
    long long delta_tense = tense_end.tv_sec * ONE_BILLION + tense_end.tv_nsec - tense_start.tv_sec * ONE_BILLION - tense_start.tv_nsec;
    fprintf(stderr,"T-CPU time %lli ms\n", delta / 1000000);
    fprintf(stderr,"Tense time %lli ms\n", delta_tense / 1000000);

    if(tense_destroy() == -1) {
        fprintf(stderr, "TENSE failed to destroy tense\n");
    }

    free(start_struct);

    return ret;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg) {
    if (!pthread_create_real) {
        pthread_create_real = dlsym(RTLD_NEXT, "pthread_create");
    }

    if (!enabled)
        return pthread_create_real(thread, attr, start_routine, arg);

    fprintf(stderr,"pid %d pthread_create\n", getpid());

    struct start * arg_wrapper = malloc (sizeof (*arg_wrapper));

    arg_wrapper->start_routine = start_routine;
    arg_wrapper->arg = arg;

    return pthread_create_real(thread, attr, start_routine_wrapper, arg_wrapper);
}

int clock_gettime(clockid_t clk_id, struct timespec *tp) {
    if (!clock_gettime_real) {
        clock_gettime_real = dlsym(RTLD_NEXT, "clock_gettime");
    }

    if (!enabled)
        return clock_gettime_real(clk_id, tp);

    if (clk_id == CLOCK_MONOTONIC) {
        // Hack to update virtual time before reading it
        clock_gettime_real(CLOCK_THREAD_CPUTIME_ID, tp);

        return tense_time(tp);
    }

    return clock_gettime_real(clk_id, tp);
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    if (! nanosleep_real)
        nanosleep_real = dlsym(RTLD_NEXT, "nanosleep");

    if (!enabled)
        return nanosleep_real(req, rem);

    unsigned long long req_ns = (unsigned long long)req->tv_sec * 1000000000 + req->tv_nsec;
    return tense_sleep_ns(req_ns);
}
