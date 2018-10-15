#ifndef TENSE_H
#define TENSE_H

#include <time.h>

int tense_init(void);

int tense_destroy(void);

void tense_health_check(void);

unsigned long long tense_rdtscp(void);

unsigned long long tense_vruntime(void);

int tense_scale_percent(int percent);

int tense_clear(void);

int tense_time(struct timespec *);
long long tense_time_ms(void);
int tense_time_point(const char * point_name);

int tense_sleep(const struct timespec * sleep);
int tense_sleep_ns(unsigned long long sleep_ns);

int tense_move(const struct timespec * delta);
int tense_move_ns(unsigned long long delta_ns);

//void tense_blink(unsigned int nanos);
//
//void tense_blink_abs(unsigned int nanos);
//
//void tense_warp_set(int scale);
//
//void tense_warp_push(int scale);
//
//int tense_warp_pop(void);

#endif