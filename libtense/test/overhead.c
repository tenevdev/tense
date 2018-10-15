#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include "../tense.h"

#define ONE_BILLION 1000000000
#define timespec_delta(s, e) (((e).tv_sec - (s).tv_sec) * ONE_BILLION + ((e).tv_nsec - (s).tv_nsec))

void time_since(struct timespec *s, clockid_t type) {
    static struct timespec e;
    if (clock_gettime(type, &e) == -1) {
        fprintf(stderr, "failed to get time\n");
        return;
    }

    printf("%li\n", timespec_delta(*s, e));
    *s = e;
}


void do_some_work(size_t iter) {
    unsigned long size = 0;

    for (int i = 1; i < iter; ++i) {
        // Waste some cycles
        int j = i * i - i;
        int k = j + 2 * i - i * i; // k == i

    }
}


void * overhead(void *_) {
    struct timespec t, m;

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    clock_gettime(CLOCK_MONOTONIC_RAW, &m);

    for (int i = 0; i < ONE_BILLION / 10000; ++i) {
        tense_scale_percent(120);
        do_some_work(4000);
        tense_clear();
        do_some_work(4000);
    }

    time_since(&t, CLOCK_THREAD_CPUTIME_ID);
    time_since(&m, CLOCK_MONOTONIC_RAW);
}

int main(int argc, char **argv) {

    pthread_t thread;

    if(pthread_create(&thread, NULL, overhead, NULL)) {
        return -1;
    }

    if(pthread_join(thread, NULL)) {
        return -1;
    }

    return 0;
}
