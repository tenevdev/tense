#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

#include "../../tense.h"
#include "minunit.h"

// thread-related
#define spawn(x) \
	mu_error(pthread_create(&x ## _thread, NULL, x, NULL), \
		"pthread_create(" #x ")");

#define join(x) \
	mu_error(pthread_join(x ## _thread, NULL), \
		"pthread_join(" #x ")");

#define routine(name, args...) \
	pthread_t name ## _thread; \
	void * name(void * data) { \
		args \
		return NULL; \
	} \

// defines a test and runs it
#define main(args) \
	int main(int argc, char **argv) { \
		struct timespec tense_start; \
		tense_init(); \
		tense_time(&tense_start); \
		args \
		tense_destroy(); \
		return 0; \
	}

// easy lock
pthread_mutex_t mutex;
#define lock() pthread_mutex_lock(&mutex);
#define unlock() pthread_mutex_unlock(&mutex);

// busy loop
#define run(x) for (volatile int j = 0; j < x * 550000; ++j) { asm(""); }
// tense actions
#define MS << 20 
#define jump(x) tense_move_ns(x MS);
#define scale(x) tense_scale_percent(x);

// tense assertions
#define _time_now() \
	long long now = tense_time_ms(); \
	mu_error(now, "tense_time_ms")

#define time_lt(x) { \
	_time_now(); \
	mu_assert(now < x, "time_lt fail %u < %u", now, x); \
}

#define time_gt(x) { \
	_time_now(); \
	mu_assert(now > x, "time_gt fail %u > %u", now, x); \
}

#define time_range(min, max) { \
	_time_now(); \
	mu_assert(min <= now && now < max, "time_between fail %u <= %llu < %u", min, now, max); \
}

// todo: real time assertions
