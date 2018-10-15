#include <time.h>
#include <stdio.h>

#define ONE_BILLION 1000000000

#define timespec_delta(s, e) ((e.tv_sec - s.tv_sec) * ONE_BILLION + (e.tv_nsec - s.tv_nsec))


int main(void) {
    int type = CLOCK_MONOTONIC_RAW;
    struct timespec start, end;

    for(int i = 0; i < 10; ++i) {
        if (clock_gettime(type, &start) == -1) {
            fprintf(stderr, "failed to get time\n");
            return -1;
        }

        for (int j = 0; j < 3 * 500000; ++j)
        {
            asm("");
        }

        if (clock_gettime(type, &end) == -1) {
            fprintf(stderr, "failed to get time\n");
            return -1;
        }

        long delta = timespec_delta(start, end);

        printf("Nanos for nops %li\n", delta);
    }
    return 0;
}