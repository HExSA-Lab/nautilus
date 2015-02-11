#ifdef __USER
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <sys/mman.h>
#include <pthread.h>
#define PRINT printf
#define THREAD_T        pthread_t
#define FUNC_HDR        (void * in)
#define FUNC_TYPE       void*
#define JOIN_FUNC(x, y) pthread_join(x, y)
#define LOCK_T          pthread_spinlock_t
#define LOCK_INIT(x)    pthread_spin_init(x, 0)
#define LOCK(x)         pthread_spin_lock(x)
#define UNLOCK(x)       pthread_spin_unlock(x)
#define MUTEX_T         pthread_mutex_t 
#define MUTEX_INIT(x)   pthread_mutex_init(x, NULL)
#define MUTEX_LOCK(x)   pthread_mutex_lock(x)
#define MUTEX_UNLOCK(x) pthread_mutex_unlock(x)
#define COND_T          pthread_cond_t
#define COND_INIT(x)    pthread_cond_init(x, NULL)
#define COND_WAIT(x, m) pthread_cond_wait(x, m)
#define COND_SIG(x)     pthread_cond_signal(x)
typedef unsigned long ulong_t;

static inline uint64_t __attribute__((always_inline))
rdtscp (void) 
{
    uint32_t lo, hi;
    asm volatile("rdtscp" : "=a" (lo), "=d" (hi));
    return lo | ((uint64_t)hi << 32);
}

static inline uint64_t __attribute__((always_inline))
rdtsc (void) 
{
    uint32_t lo, hi;
    asm volatile("cpuid\n\t"
                 "rdtscp" : 
                 "=a" (lo), "=d" (hi));
    return lo | ((uint64_t)hi << 32);
}

#else
#include <nautilus/nautilus.h>
#include <nautilus/libccompat.h>
#include <nautilus/mwait.h>
#include <nautilus/thread.h>
#include <nautilus/condvar.h>
#include <nautilus/spinlock.h>
#include <lib/liballoc.h>
#define PRINT           printk
#define THREAD_T        nk_thread_id_t
#define LOCK_T          spinlock_t
#define FUNC_HDR        (void * in, void ** out)
#define FUNC_TYPE       void
#define JOIN_FUNC(x, y) nk_join(x, y)
#define LOCK_INIT(x)    spinlock_init(x)
#define LOCK(x)         spin_lock(x)
#define UNLOCK(x)       spin_unlock(x)
#define MUTEX_T         spinlock_t
#define MUTEX_INIT(x)   spinlock_init(x)
#define MUTEX_LOCK(x)   spin_lock(x)
#define MUTEX_UNLOCK(x) spin_unlock(x)
#define COND_T          nk_condvar_t
#define COND_INIT(x)    nk_condvar_init(x)
#define COND_WAIT(x, m) nk_condvar_wait(x, m)
#define COND_SIG(x)     nk_condvar_signal(x)
#endif

#define SIZE                  4096
#define THR_CREATE_LOOPS      1000
#define THR_LONG_CREATE_LOOPS 10
#define SPINLOCK_LOOPS        1000
#define CONDVAR_LOOPS         100
#define MWAIT_LOOPS           100
#define BIL                   1000000000
#define MIL                   1000000
#define LONG_LOCK_COUNT       (500*MIL)
#define LOCK_COUNT            1


static struct cv {
    uint64_t start;
    uint64_t end;
    uint64_t sum;
    uint64_t max;
    uint64_t min;
} cond_time;

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

void time_spinlock_long (void);
void time_spinlock_long (void)
{
    struct timespec ts, te;
    LOCK_T l;
    LOCK_INIT(&l);
    int i, j;
    uint64_t avg = 0;
    uint64_t diff = 0;
    uint64_t min = ULLONG_MAX;
    uint64_t max = 0;
    uint64_t nsec = 0;
    uint64_t tsc_overhead = 0;
    uint64_t nano_overhead = 0;
    uint64_t total = 0;

    for (i = 0; i < SPINLOCK_LOOPS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        clock_gettime(CLOCK_MONOTONIC, &te);
        nsec += ((te.tv_sec * BIL) + te.tv_nsec) - ((ts.tv_sec * BIL) + ts.tv_nsec);
    }
    avg = nsec / SPINLOCK_LOOPS; 

    PRINT("clock gettime overhead: %lu.%lu us\n", 
            avg / 1000, 
            avg % 1000);

    nano_overhead = avg;

    avg = 0;
    nsec = 0;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    for (j = 0; j < LONG_LOCK_COUNT; j++) {
        LOCK(&l);
        UNLOCK(&l);
    }
    clock_gettime(CLOCK_MONOTONIC, &te);

    diff = (te.tv_sec * BIL + te.tv_nsec) - 
        (ts.tv_sec * BIL + ts.tv_nsec);

    diff = (nano_overhead > diff) ? 0 : diff - nano_overhead;

    PRINT("\nSpinlock long clock_gettime (sec) (overhead subtracted): %lu.%lu seconds\n",
            diff / BIL,
            diff % BIL);

    avg = 0;
    total = 0;

    i = 0;
    j = 0;
    for (i = 0; i < SPINLOCK_LOOPS; i++) {
        uint64_t start, end;
        start = rdtscp();
        end = rdtscp();
        total += end - start;
    }
    avg = total / SPINLOCK_LOOPS; 

    PRINT("RDTSC overhead: %lu cycles\n", avg);
    tsc_overhead = avg;

    uint64_t start, end;
    start = rdtscp();
    for (j = 0; j < LONG_LOCK_COUNT; j++) {
        LOCK(&l);
        asm volatile ("":::"memory");
        UNLOCK(&l);
    }
    end = rdtscp();

    diff = end - start;
    diff = (tsc_overhead > diff) ? 0 : diff - tsc_overhead;
            

    PRINT("\nSpinlock long TSC in cycles (overhead subtracted) - %lu cycles\n",
            diff);
}

#ifndef __USER

struct mwait_trigger {
    uint8_t flag;
    uint8_t pad[63]; 
} __packed;

static FUNC_TYPE
mwait_wakeme FUNC_HDR
{
    struct mwait_trigger * trig = (struct mwait_trigger*)in;
    udelay(100000);
    trig->flag = 1;
    cond_time.start = rdtscp();
}

extern void nk_monitor(unsigned long,
        uint32_t, 
        uint32_t);

extern void nk_mwait (uint32_t, uint32_t);

void time_mwait (void);

void 
time_mwait (void) {

    THREAD_T t;
    int i;

    struct mwait_trigger trigger __attribute__((aligned (64)));
    memset(&trigger, 0, sizeof(trigger));
    memset(&cond_time, 0, sizeof(cond_time));

    if (sizeof(trigger) != 64) {
        ERROR_PRINT("Trigger is not 64 bytes!\n");
    }

    cond_time.min = ULLONG_MAX;

    nk_monitor((addr_t)&trigger, 0, 0);

    //printk("core %u going to monitor on addr %p\n", my_cpu_id(), addr);

    for (i = 0; i < MWAIT_LOOPS; i++) {

        nk_thread_start(mwait_wakeme, &trigger, NULL, 0, TSTACK_DEFAULT, &t,nk_get_cpu_by_lapicid(2) );

        //nk_monitor((addr_t)&trigger, 0, 0);
        nk_mwait(0, 0);
        cond_time.end = rdtscp();
        printk("core %u woke up! %lu cycles\n", my_cpu_id(), cond_time.end - cond_time.start);
        uint64_t diff = cond_time.end - cond_time.start;
        trigger.flag = 0;
        JOIN_FUNC(t, NULL);

        if (i != 0) {
            if (diff < cond_time.min) {
                cond_time.min = diff;
            }

            if (diff > cond_time.max) {
                cond_time.max = diff;
            }

            cond_time.sum += diff;
        }
    }

    uint64_t avg = cond_time.sum / (MWAIT_LOOPS-1);

    PRINT("MWAIT WAKEUP: (loops=%u) Avg: %llu Min: %llu Max: %llu\n",
            MWAIT_LOOPS,
            avg,
            cond_time.min,
            cond_time.max);

}

#endif

static FUNC_TYPE
wakemeupbeforeyougogo FUNC_HDR
{
    COND_T * c = (COND_T*)in;
#ifdef __USER
    usleep(100000);
#else
    udelay(100000);
#endif
    COND_SIG(c);
    cond_time.start = rdtscp();
}

void time_condvar (void);
void time_condvar (void) {

    COND_T c;
    THREAD_T t;
    MUTEX_T l;
    int i;


    MUTEX_INIT(&l);
    COND_INIT(&c);

    memset(&cond_time, 0, sizeof(cond_time));
    cond_time.min = ULLONG_MAX;

        for (i = 0; i < CONDVAR_LOOPS; i++) {

#ifdef __USER
            pthread_create(&t, NULL, wakemeupbeforeyougogo, &c);
#else
            nk_thread_start(wakemeupbeforeyougogo, &c, NULL, 0, TSTACK_DEFAULT, &t, 1);
#endif

            MUTEX_LOCK(&l);

            COND_WAIT(&c, &l);

            cond_time.end = rdtscp();

            MUTEX_UNLOCK(&l);

            JOIN_FUNC(t, NULL);

            uint64_t diff = cond_time.end - cond_time.start;

            if (diff < cond_time.min) {
                cond_time.min = diff;
            }

            if (diff > cond_time.max) {
                cond_time.max = diff;
            }

            cond_time.sum += diff;
        }

        uint64_t avg = cond_time.sum / CONDVAR_LOOPS;

        PRINT("CONDVAR: (loops=%u) Avg: %lu Min: %lu Max: %lu\n",
                CONDVAR_LOOPS,
                avg,
                cond_time.min,
                cond_time.max);

}

void time_spinlock (void);
void time_spinlock (void)
{
    struct timespec ts, te;
    LOCK_T l;
    LOCK_INIT(&l);
    int i, j;
    uint64_t avg = 0;
    uint64_t min = ULLONG_MAX;
    uint64_t max = 0;
    uint64_t nsec = 0;
    uint64_t tsc_overhead = 0;
    uint64_t nano_overhead = 0;

    for (i = 0; i < SPINLOCK_LOOPS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        clock_gettime(CLOCK_MONOTONIC, &te);
        nsec += ((te.tv_sec * BIL) + te.tv_nsec) - ((ts.tv_sec * BIL) + ts.tv_nsec);
    }
    avg = nsec / SPINLOCK_LOOPS; 

    PRINT("clock gettime overhead: %lu.%lu us\n", 
            avg / 1000, 
            avg % 1000);

    nano_overhead = avg;

    avg = 0;
    nsec = 0;

    for (i = 0; i < SPINLOCK_LOOPS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &ts);

        for (j = 0; j < LOCK_COUNT; j++) {
            LOCK(&l);
            UNLOCK(&l);
        }
        clock_gettime(CLOCK_MONOTONIC, &te);

        uint64_t diff = (te.tv_sec * BIL + te.tv_nsec) - 
            (ts.tv_sec * BIL + ts.tv_nsec);

        diff = (nano_overhead > diff) ? 0 : diff - nano_overhead;

        if (diff < min) {
            min = diff;
        }

        if (diff > max) {
            max = diff;
        }

        nsec += diff;
    }

    avg = nsec / SPINLOCK_LOOPS;

    PRINT("\nSpinlock clock_gettime (msec) (overhead subtracted) - Avg: %lu.%lu, Min: %lu.%lu, Max: %lu.%lu\n",
            avg / 1000000, 
            avg % 1000000,
            min / 1000000,
            min % 1000000,
            max / 1000000,
            max % 1000000);
    avg = 0;
    min = ULLONG_MAX;
    max = 0;
    uint64_t total = 0;

    for (i = 0; i < SPINLOCK_LOOPS; i++) {
        uint64_t start, end;
        start = rdtscp();
        end = rdtscp();
        total += end - start;
    }
    avg = total / SPINLOCK_LOOPS; 

    PRINT("RDTSC overhead: %lu cycles\n", avg);
    tsc_overhead = avg;
    avg = 0;
    total = 0;

    for (i = 0; i < SPINLOCK_LOOPS; i++) {
        uint64_t start, end;
        start = rdtscp();
        //for (j = 0; j < LOCK_COUNT; j++) {
            LOCK(&l);
            UNLOCK(&l);
        //}
        end = rdtscp();

        uint64_t diff = end - start;
        diff = (tsc_overhead > diff) ? 0 : diff - tsc_overhead;
            
        if (diff < min) {
            min = diff;
        }

        if (diff > max) {
            max = diff;
        }

        total += diff;
    }

    avg = total / SPINLOCK_LOOPS;

    PRINT("\nSpinlock TSC in cycles (overhead subtracted) - Avg: %lu, Min: %lu, Max: %lu\n",
            avg,
            min,
            max);
}

static FUNC_TYPE
start_test FUNC_HDR
{
    return;
}


void time_threads_long(void);
void time_threads_long(void)
{
    THREAD_T t[64];

    uint64_t total;
    uint64_t min;
    uint64_t max;
    uint64_t avg;

    int i, j, k;
    uint8_t thread_counts[6] = {2, 4, 8, 16, 32, 64};

    //for (k = 0; k < 6; k++) {

        total = 0;
        max = 0;
        min = ULLONG_MAX;
        avg = 0;

#define CUR 64
#ifndef __USER
        asm volatile ("cli");
#endif
        for (i = 0; i < THR_LONG_CREATE_LOOPS; i++) {

            uint64_t start = rdtscp();

            for (j = 0; j < CUR; j++) {
#ifdef __USER
                pthread_create(&t[j], NULL, start_test, NULL);
#else
                nk_thread_start(start_test, NULL, NULL, 0, TSTACK_DEFAULT, &t[j], 0);
#endif
            }

            uint64_t end = rdtscp();

            for (j = 0; j < CUR; j++) {
                JOIN_FUNC(t[j], NULL);
            }

            uint64_t diff = end - start;

            if (diff < min) { 
                min = diff;
            }

            if (diff > max) { 
                max = diff;
            }

            total += diff;

        }
#ifndef __USER
        asm volatile ("sti");
#endif

        avg = total / THR_LONG_CREATE_LOOPS;

        PRINT("Thread Create (%u threads): Avg: %lu cycles Min: %lu cycles Max: %lu cycles\n",
                CUR,
                avg,
                min,
                max);
    //}
}

void time_threads(void);
void
time_threads (void)
{
    struct timespec ts, te;
    THREAD_T t;

    uint64_t nsec = 0;
    uint64_t min = ULLONG_MAX;;
    uint64_t max = 0;

    int i;
#ifndef __USER
    asm volatile ("cli");
#endif
    for (i = 0; i < THR_CREATE_LOOPS; i++) {
        //clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t start = rdtscp();
    #ifdef __USER
        pthread_create(&t, NULL, start_test, NULL);
    #else
        nk_thread_start(start_test, NULL, NULL, 0, TSTACK_DEFAULT, &t, 0);
    #endif
        uint64_t end = rdtscp();
        //clock_gettime(CLOCK_MONOTONIC, &te);
        //asm volatile ("":::"memory");
        JOIN_FUNC(t, NULL);

#if 0
        uint64_t diff = (te.tv_sec * BIL + te.tv_nsec) - 
            (ts.tv_sec * BIL + ts.tv_nsec);
#endif
        uint64_t diff = end - start;

        if (diff < min) { 
            min = diff;
        }

        if (diff > max) { 
            max = diff;
        }

        nsec += diff;

    }
    asm volatile ("sti");

    uint64_t avg = nsec / THR_CREATE_LOOPS;
    //uint64_t leftover = nsec % THR_CREATE_LOOPS;

    PRINT("Thread Create: Avg: %lu cycles Min: %lu cycles Max: %lu cycles\n",
            avg,
            min,
            max);
#if 0
    PRINT("Thread Create: Avg: %lu.%lu us Min: %lu.%lu us Max: %lu.%lu us\n",
            avg / 1000,
            avg % 1000,
            min / 1000,
            min % 1000,
            max / 1000,
            min % 1000);
#endif
}

#ifdef __USER 

int main () {
    //ubenchmark();
    //time_spinlock();
    //time_threads();
    //time_threads_long();
    time_spinlock_long();
}

#define NUM_THREADS 32

#endif
