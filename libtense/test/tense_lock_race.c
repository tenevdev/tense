#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "../tense.h"

#define NSEC_IN_SEC 1000000000L

pthread_mutex_t mutex;

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
        tense_time_point("block point");
        printf("%s was about to block\n", caller_name);
        pthread_mutex_lock(&mutex);
    }

    work(10 * input_size);
    pthread_mutex_unlock(&mutex);
}

void * thread_routine(void * data) {
    unsigned n = *(unsigned int *) data;

    work(n);
    critical(n, "thread");

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

    tense_scale_percent(900);

    work(3 * input);
    critical(input, "main");
    
    tense_clear();

    tense_time_point("main done");

    if (pthread_join(other_thread, NULL)) {
        return -1;
    }

    tense_time_point("thread joined");

    tense_destroy();
    return 0;
}
