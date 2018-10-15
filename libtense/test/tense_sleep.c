#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "../tense.h"

#define NSEC_IN_SEC 1000000000L

pthread_mutex_t mutex;

struct timespec start, ten_ms = {
        .tv_sec = 0,
        .tv_nsec = 10000000
};

void work(unsigned input_size) {
    unsigned n = input_size;
    n *= n * 100;

    for (int i = 0; i < n; ++i) {
        int j = i + n * i;
    }
}

void critical(unsigned input_size, const char * caller_name) {
    int fail = pthread_mutex_trylock(&mutex);
    if (fail) {
        printf("%s was about to block", caller_name);
    } else {
        work(10 * input_size);
        pthread_mutex_unlock(&mutex);
    }
}

void * thread_routine(void * data) {
    unsigned n = *(unsigned int *) data;

    tense_scale_percent(200);

    work(n);
    work(n);
    work(n);
    critical(n, "thread");

    tense_clear();

    return NULL;
}

int main(int argc, char ** argv) {
    struct timespec tense_start, tense_end;
    pthread_t other_thread;

    if (argc != 2)
        return -1;

    tense_init();
    tense_time(&tense_start);


    unsigned input = (unsigned int) atoi(argv[1]);

    if (pthread_create(&other_thread, NULL, thread_routine, &input)) {
        return -1;
    }

    work(input);
    nanosleep(&ten_ms, NULL);
    work(input);
    critical(input, "main");

    tense_time(&tense_end);

    if (pthread_join(other_thread, NULL)) {
        return -1;
    }

    long long delta_tense = tense_end.tv_sec * NSEC_IN_SEC + tense_end.tv_nsec - tense_start.tv_sec * NSEC_IN_SEC - tense_start.tv_nsec;
    fprintf(stderr,"Tense time %lli ms\n", delta_tense / 1000000);

    tense_destroy();
    return 0;
}