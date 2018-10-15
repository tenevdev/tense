#define _GNU_SOURCE

#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syscall.h>
#include <stdlib.h>
#include <stdint.h>
#include "tense.h"

#define TENSE_FILE "/sys/kernel/debug/tense"
#define PAGE_SIZE 4096

#define NS_IN_SECOND 1000000000
#define MS_IN_SECOND 1000

static __thread int vtime_fd;
static __thread int tense_fd;

#define FASTER 0
#define SLOWER 1

static __thread uint32_t tense[2];

static __thread void * tense_page = NULL;

int tense_init(void) {
//    vtime_fd = open(TENSE_FILE, O_RDWR);
//    if (vtime_fd < 0)
//        return -1;

//    tense_page = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, vtime_fd, 0);
//    if (tense_page == MAP_FAILED)
//        return -1;
//

    tense[FASTER] = 1;
    tense[SLOWER] = 1;

    tense_fd = open(TENSE_FILE, O_RDWR);
    if (tense_fd < 0)
        goto bad_tense_open;

    if (write(tense_fd, (const void *) tense, 2 * sizeof(uint32_t)) == -1)
        goto bad_tense_write;

    goto success;

bad_tense_write:
    close(tense_fd);

bad_tense_open:
    return -1;

success:
    return 0;
}

int tense_destroy(void) {
    if(close(tense_fd) == -1)
        return -1;

    return 0;
}

void tense_health_check(void) {
    printf("Tense page at %lu\n", (unsigned long)tense_page);
    unsigned long long * addr = tense_page;
    while (*addr) {
        printf("Init data %llu\n", *addr);
        addr++;
    }

    addr = tense_page;
    for(unsigned long long i = 1; i < 20; i++) {
        memcpy(addr++, &i, sizeof(unsigned long long));
    }

    addr = tense_page;
    while (*addr) {
        printf("Written data %llu\n", *addr);
        addr++;
    }
}

inline unsigned long long tense_rdtscp(void)
{
    unsigned int low, high;

    asm volatile(
    "rdtscp"
    : "=a" (low), "=d" (high)
    : "a"(0)
    : "%ebx", "%ecx");

    return ((unsigned long long) low) | (((unsigned long long) high) << 32); // NOLINT
}

int tense_time(struct timespec *tp)
{
    return read(tense_fd, tp, 0);
//
//    char buf[VRUNTIME_LEN];
//    ssize_t bytes_read = read(tense_fd, buf, VRUNTIME_LEN);
//    buf[bytes_read] = '\0';
//    printf("TENSE time %s", buf);
}

long long tense_time_ms(void)
{
    struct timespec now;
    if (tense_time(&now) == -1)
        return -1;

    return (now.tv_sec * MS_IN_SECOND + (now.tv_nsec >> 20));
}

int
tense_time_point(const char * point_name)
{
    long long time = tense_time_ms();
    if (time == -1)
        return -1;

    fprintf(stderr, "tense: %llu ms %s \n", time, point_name);
}

int
tense_write(void)
{
    if(write(tense_fd, (const void *)tense, 2 * sizeof(uint32_t)) == -1)
        return -1;
    return 0;
}

int
tense_scale_percent(int percent)
{
    tense[FASTER] *= percent;
    tense[SLOWER] *= 100;

    return tense_write();
}

int
tense_clear(void)
{
    tense[FASTER] = 1;
    tense[SLOWER] = 1;

    return tense_write();
}

int
tense_sleep_ns(unsigned long long sleep_ns)
{
    lseek(tense_fd, (off_t)sleep_ns, SEEK_HOLE);
}

int tense_sleep(const struct timespec * sleep)
{
    tense_sleep_ns((unsigned long long int) (sleep->tv_sec * NS_IN_SECOND + sleep->tv_nsec));
}


int
tense_move(const struct timespec * delta)
{
    off_t now = lseek(tense_fd, (off_t)(delta->tv_nsec + delta->tv_sec * NS_IN_SECOND), SEEK_CUR);
    return (now == (off_t) -1) ? -1 : 0;
}

int
tense_move_ns(unsigned long long delta_ns)
{
    off_t now = lseek(tense_fd, (off_t)delta_ns, SEEK_CUR);
    return (now == (off_t) -1) ? -1 : 0;
}