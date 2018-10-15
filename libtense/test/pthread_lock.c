#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <syscall.h>
#include <dirent.h>
#include <fcntl.h>
#include "../tense.h"


void * red(void * data) {

}

void * blue(void * data) {

}

int main()
{
    pthread_t red_t;
    pthread_t blue_t;

    if(pthread_create(&red_t, NULL, red, NULL)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

    if(pthread_create(&blue_t, NULL, blue, NULL)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

    if(pthread_join(red_t, NULL)) {
        fprintf(stderr, "Error joining thread\n");
        return 1;
    }

    if(pthread_join(blue_t, NULL)) {
        fprintf(stderr, "Error joining thread\n");
        return 1;
    }

    return 0;
}