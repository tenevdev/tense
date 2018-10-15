#include <stdlib.h>
#include <stdio.h>
#include "../tense.h"

#define NSEC_IN_SEC 1000000000L

void unimplemented(unsigned arg_one, void * arg_two) {
    tense_move_ns(100 * arg_one * arg_one);
}

int main(int argc, char ** argv) {
    struct timespec tense_start, tense_end;

    if (argc != 2)
        return -1;

    tense_init();
    tense_time(&tense_start);


    unsigned input = (unsigned int) atoi(argv[1]);

    unimplemented(input, argv);

    tense_time(&tense_end);
    long long delta_tense = tense_end.tv_sec * NSEC_IN_SEC + tense_end.tv_nsec - tense_start.tv_sec * NSEC_IN_SEC - tense_start.tv_nsec;
    fprintf(stderr,"Tense time %lli ms\n", delta_tense / 1000000);

    tense_destroy();
    return 0;
}