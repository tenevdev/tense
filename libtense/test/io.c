#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

// Modes of operation
#define STDIN 0 // For playing around with the period
#define SLEEP 1 // For constant period

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

int main(int argc, char **argv) {
    struct timespec t, m;

    printf("pid=%d", getpid());

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    clock_gettime(CLOCK_MONOTONIC_RAW, &m);

    switch (atoi(argv[1])) {
        case SLEEP: {
            struct timespec sleep_dur = { .tv_sec = 0, .tv_nsec = 1000000 };
            for (int i = 0; i < 10; ++i) {
                nanosleep(&sleep_dur, NULL);
                time_since(&t, CLOCK_THREAD_CPUTIME_ID);
                time_since(&m, CLOCK_MONOTONIC_RAW);
            }
            break;
        }
        case STDIN: default: {
            while (getchar() != 'q') {
                time_since(&t, CLOCK_THREAD_CPUTIME_ID);
                time_since(&m, CLOCK_MONOTONIC_RAW);
            }
            break;
        }
    }

    return 0;

}