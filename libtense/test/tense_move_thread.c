#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "../tense.h"

#define NSEC_IN_SEC 1000000000L

void unimplemented(unsigned arg_one, void * arg_two) {
    tense_move_ns(100 * arg_one * arg_one);
}

void * implemented(void * data) {
    int n = *(int *) data;
    n *= n * 100;

    for (int i = 0; i < n; ++i) {
        int j = i + n * i;
    }

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

    if (pthread_create(&other_thread, NULL, implemented, &input)) {
        return -1;
    }

    unimplemented(input, argv);

    tense_time(&tense_end);

    if (pthread_join(other_thread, NULL)) {
        return -1;
    }

    long long delta_tense = tense_end.tv_sec * NSEC_IN_SEC + tense_end.tv_nsec - tense_start.tv_sec * NSEC_IN_SEC - tense_start.tv_nsec;
    fprintf(stderr,"Tense time %lli ms\n", delta_tense / 1000000);

    tense_destroy();
    return 0;
}