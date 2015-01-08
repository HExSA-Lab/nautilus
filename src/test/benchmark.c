
#ifdef __USER
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/mman.h>
typedef unsigned long ulong_t;
#else
#include <nautilus/nautilus.h>
#include <nautilus/libccompat.h>
#include <lib/liballoc.h>
#endif

#define SIZE 4096

void ubenchmark (void);

void
ubenchmark (void) {

    int i;
    int j;
    struct timespec ts, te;

    unsigned count = 10000;
    volatile ulong_t sum = 0;

    uint8_t  * page = malloc(SIZE);
    if (!page) {
#ifdef __USER
        fprintf(stderr, "Could not alloc\n");
#else
        ERROR_PRINT("Could not alloc\n");
#endif
    }

    clock_gettime(CLOCK_MONOTONIC, &ts);
    for (j = 0; j < count; j++) {
        for ( i = 0; i < SIZE; i++) {
            sum += *(volatile uint8_t*)(page+i);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &te);

#ifdef __USER
    printf("loop: start: %lu sec %lu nsec, end: %lu sec %lu nsec\n", 
            ts.tv_sec, ts.tv_nsec,
            te.tv_sec, te.tv_nsec);
#else

    printk("loop: start: %lu sec %lu nsec, end: %lu sec %lu nsec\n", 
            ts.tv_sec, ts.tv_nsec,
            te.tv_sec, te.tv_nsec);

#endif
}

#ifdef __USER 

int main () {
    ubenchmark();
}
#endif
