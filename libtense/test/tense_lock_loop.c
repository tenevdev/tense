#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
//#include "../tense.h"

#define NSEC_IN_SEC 1000000000L
#define N_WORKERS 8
#define N_LOOPS 50

pthread_mutex_t mutex;

void work(unsigned input_size) {
    unsigned n = input_size;
    n *= n * 100;

    for (int i = 0; i < n; ++i) {
        int j = i + n * i;
    }
}

void critical(unsigned input_size) {
    unsigned n = input_size;
    n *= n * 100;
    for (int i = 0; i < n; ++i) {
        int j = i + n * i;
    }
}

void * thread_routine(void * data) {
    unsigned n = *(unsigned int *) data;

    work(n);

    return NULL;
}

void * critical_thread_routine(void * data) {
    unsigned n = *(unsigned int *) data;

    critical(n);

    return NULL;
}


int main(int argc, char ** argv) {
    struct timespec tense_start, tense_end;
    pthread_t workers[10];

    if (argc != 2)
        return -1;

//    tense_init();
//    tense_time(&tense_start);

    unsigned input = (unsigned int) atoi(argv[1]);

    if (pthread_create(&workers[0], NULL, critical_thread_routine, &input)) {
        return -1;
    }

    for (int i = 1; i < N_WORKERS; ++i) {
        if (pthread_create(&workers[i], NULL, thread_routine, &input)) {
            return -1;
        }
    }

//    tense_scale_percent(800);
//    work(3 * input);
//    critical(input, "main");
//    tense_clear();

    for(int i = 0; i < N_WORKERS; ++i) {
        if (pthread_join(workers[i], NULL)) {
            return -1;
        }
    }

//    tense_time(&tense_end);

//    long long delta_tense = tense_end.tv_sec * NSEC_IN_SEC + tense_end.tv_nsec - tense_start.tv_sec * NSEC_IN_SEC - tense_start.tv_nsec;
//    fprintf(stderr,"Tense time %lli ms\n", delta_tense / 1000000);

//    tense_destroy();
    return 0;
}