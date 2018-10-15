/*
 * Usage:
 *
 *   ./vtime_spin <human_id> <granularity> <time_dilation_factor>
 *
 * Upon receiving a signal prints the process name, human_id if given or PID
 * otherwise, and the number of work cycles completed at the specified
 * granularity. Granularity has exponential effect - increase by 1 should lead
 * to two times more cycles completed.
 *
 * Combine with timeout for precise duration:
 *
 *   timeout -s SIGINT <duration> ./vtime_spin <human_id> <gran> <tdf>
 *
 * Output:
 *
 *   Tab-separated process_name, human_id, granularity, tdf, cycles
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include "../tense.h"

static volatile sig_atomic_t exiting = 0;

static void
signal_handler(int signo)
{
    exiting = 1;
}

static void
spin(int granularity)
{
    for (int i = 0; i < INT_MAX / granularity; i++) {
        int j = i * i - i;
        // int k = j - i + j * i;
        // k = j - i;
    }
}

static int
set_time_dilation_factor(int vtime_spd)
{
    if (tense_init() == -1)
        return -1;

    if (tense_scale_percent(vtime_spd * 100) == -1)
        return -1;

    return 0;
}

int
main(int argc, char ** argv)
{
    int count = 0;
    int human_id = atoi(argv[1]);
    int granularity = atoi(argv[2]);
    int gran_power = (int)pow(10, granularity);
    int vtime_spd = atoi(argv[3]);

    // Set time dilation
    if (set_time_dilation_factor(vtime_spd) == -1) {
        fprintf(stderr, "Cannot open vtime_spd for writing\n");
        return EXIT_FAILURE;
    }

    // Register interrupt signal handler
    struct sigaction sa = { .sa_handler = signal_handler };
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "Failed to register signal handler\n");
        return EXIT_FAILURE;
    }

    struct timespec start_ts, end_ts;
    tense_time(&start_ts);

    long long start = start_ts.tv_sec * 1000000000 + start_ts.tv_nsec;

    printf("%d start %llu\n", human_id, start);

    // Do work until signal
    while (!exiting) {
        spin(gran_power);
        count++;
    }

    tense_time(&end_ts);
    long long end = end_ts.tv_sec * 1000000000 * end_ts.tv_nsec;
    printf("%s\t%d\t%d\t%d\t%d\t%lli\n", argv[0], human_id, granularity,
           vtime_spd, count, end - start);

    tense_destroy();
    return EXIT_SUCCESS;
}
