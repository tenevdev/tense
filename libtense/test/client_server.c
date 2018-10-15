#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include "../tense.h"

#define u_sample (random() / (RAND_MAX + 1.0))
#define exp_sample(rate) (-log(u_sample) / (rate))
static inline int geo_sample(double p) {
    double u = u_sample;
    return u < p ? 0 : (int)ceil(log(1 - u) / log(1 - p) - 1);
}

#ifndef MAX_REQUESTS
#define MAX_REQUESTS 10000
#endif

#define EMPTY (-1)
#define FULL (-1)
#define ONE_BILLION  1000000000L

struct request_queue {
    volatile size_t in, out;
    volatile unsigned long long buf[MAX_REQUESTS];
};

int request_queue_get(struct request_queue * rq, unsigned long long * val) {
    if (rq->out == rq->in)
        return EMPTY;

    *val = rq->buf[rq->out];
    rq->out = ++rq->out % MAX_REQUESTS;
    return 0;
}

int request_queue_put(struct request_queue * rq, unsigned long long val) {
    size_t next = (rq->in + 1) % MAX_REQUESTS;
    if (next == rq->out)
        return FULL;

    rq->buf[rq->in] = val;
    rq->in = next;
    return 0;
}

unsigned long request_queue_size(struct request_queue *rq) {
    unsigned long  size = MAX_REQUESTS + rq->in - rq->out;

    return size < MAX_REQUESTS ? size : (size - MAX_REQUESTS);
}

struct client_args {
    size_t requests_per_ms;
    size_t n_requests;
    size_t faster;
    size_t slower;
    struct request_queue * rq;
};

struct timespec client_request(size_t requests_per_ms, size_t faster, size_t slower) {
    double ms = (exp_sample(requests_per_ms) * faster) / slower;
    time_t s = (time_t) (ms / 1000);
    long ns = ((long)(ms * 1000000)) - s * 1000000000;

    struct timespec tv = {
            .tv_sec = s,
            .tv_nsec = ns
    };

    return tv;
}

#define timespec_delta(s, e) ((e.tv_sec - s.tv_sec) * ONE_BILLION + (e.tv_nsec - s.tv_nsec))

void * client(void * argv) {
    struct client_args * args = (struct client_args *) argv;
    unsigned long long prev_rt_raw = 0;
    unsigned long long prev_rt = 0;

    for (int i = 0; i < args->n_requests; ++i) {
        struct timespec next_request = client_request(args->requests_per_ms, args->faster, args->slower);

        nanosleep(&next_request, NULL);

        struct timespec time_in;

//        if (clock_gettime(CLOCK_MONOTONIC_RAW, &time_in) == -1)
//            fprintf(stderr, "failed to get time\n");
//
//        unsigned long long request_time = (unsigned long long)time_in.tv_sec * ONE_BILLION + time_in.tv_nsec;
//        printf("raw %llu,\n", request_time - prev_rt_raw);
//        prev_rt_raw = request_time;

        if (clock_gettime(CLOCK_MONOTONIC, &time_in) == -1) {
            fprintf(stderr, "failed to get time\n");
        }

        unsigned long long request_time = (unsigned long long)time_in.tv_sec * ONE_BILLION + time_in.tv_nsec;
//        printf("ten %llu,\n", request_time - prev_rt);
//        prev_rt = request_time;

        while (request_queue_put(args->rq, request_time) == FULL) {
            fprintf(stderr, "warning: queue is full\n");
        }
    }

    return NULL;
}

struct server_args {
    size_t n_requests;
    struct request_queue * rq;
    size_t  loop_iterations;
    double tense_speedup;
};

unsigned long do_some_work(struct request_queue * rq, size_t iter) {
    unsigned long size = 0;
    size_t when = rand() % iter;

    for (int i = 1; i < iter; ++i) {
        // Waste some cycles
        int j = i * i - i;
        int k = j + 2 * i - i * i; // k == i

        // Sample queue size at some random point
        if (k == when)
            size = request_queue_size(rq);
    }
    return size;
}

void * server(void * argv) {
    struct server_args * args = (struct server_args *) argv;
    unsigned long long response_time_total = 0;
    unsigned long long rq_size_total = 0;
    unsigned long long service_time_total = 0;
    unsigned long long time_total;
    unsigned long long request_time;

    tense_scale_percent((int) (100 * args->tense_speedup));

    struct timespec start;
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        fprintf(stderr, "failed to get time\n");
    }

    for (int i = 0; i < args->n_requests; ++i) {
        while (request_queue_get(args->rq, &request_time) == EMPTY);

        struct timespec time_out;
        if (clock_gettime(CLOCK_MONOTONIC, &time_out) == -1) {
            fprintf(stderr, "failed to get time\n");
        }


//        tense_scale_percent((int) (100 * args->tense_speedup));
        rq_size_total += do_some_work(args->rq, args->loop_iterations);
//        tense_clear();

        unsigned long long response_time = time_out.tv_sec * ONE_BILLION + time_out.tv_nsec - request_time;

        response_time_total += response_time;

        struct timespec end;
        if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
            fprintf(stderr, "failed to get time\n");

        service_time_total += end.tv_sec * ONE_BILLION + end.tv_nsec - time_out.tv_sec * ONE_BILLION - time_out.tv_nsec;
    }

    struct timespec end;
    if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
        fprintf(stderr, "failed to get time\n");

    time_total = (unsigned long long) (end.tv_sec * ONE_BILLION + end.tv_nsec - start.tv_sec * ONE_BILLION - start.tv_nsec);
//
//    printf("Avg wait in queue: %lf ns\n", response_time_total / (double) args->n_requests);
//    printf("Avg queue size: %lf\n", rq_size_total / (double) args->n_requests);
//    printf("Avg queue size (timed): %lf\n", response_time_total / (double) time_total);
//    printf("Avg service time: %lf ns\n", service_time_total / (double) args->n_requests);

    printf("%lf\t%lf\t%lf\n", response_time_total / (double) args->n_requests,service_time_total / (double) args->n_requests, response_time_total / (double) time_total);

    return NULL;
}

int main(int argc, char **argv) {


    struct request_queue rq = {
            .in = 0,
            .out = 0
    };

    struct client_args client_config = {
            .n_requests = (size_t) atoi(argv[1]),
            .requests_per_ms = (size_t) atoi(argv[2]),
            .faster = (size_t) atoi(argv[4]),
            .slower = (size_t) atoi(argv[5]),
            .rq = &rq
    };

    struct server_args server_config = {
            .n_requests = client_config.n_requests,
            .rq = &rq,
            .loop_iterations = (size_t) atoi(argv[3]),
            .tense_speedup = atof(argv[6]),
    };

    pthread_t server_t;
    if (pthread_create(&server_t, NULL, server, &server_config)) {
        fprintf(stderr, "failed to create server\n");
        return -1;
    }

    pthread_t client_t;
    if (pthread_create(&client_t, NULL, client, &client_config)) {
        fprintf(stderr, "failed to create client\n");
        return -1;
    }

    if(pthread_join(server_t, NULL)) {
        fprintf(stderr, "failed to join server\n");
        return -1;
    }

    if(pthread_join(client_t, NULL)) {
        fprintf(stderr, "failed to join client\n");
        return -1;
    }

    return 0;
}