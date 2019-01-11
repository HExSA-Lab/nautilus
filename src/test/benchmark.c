/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifdef __USER

#define _GNU_SOURCE
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include <pthread.h>

#else

#include <nautilus/nautilus.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <nautilus/libccompat.h>
#include <nautilus/mwait.h>
#include <nautilus/thread.h>
#include <nautilus/condvar.h>
#include <nautilus/spinlock.h>
#include <nautilus/percpu.h>
#include <nautilus/numa.h>
#include <nautilus/nemo.h>
#include <nautilus/pmc.h>
#include <nautilus/shell.h>

#endif

#include "benchmark.h"

#define rdtscll(val)                    \
    do {                        \
    uint64_t tsc;                   \
    uint32_t a, d;                  \
    asm volatile("rdtsc" : "=a" (a), "=d" (d)); \
    *(uint32_t *)&(tsc) = a;            \
    *(uint32_t *)(((unsigned char *)&tsc) + 4) = d;   \
    val = tsc;                  \
    } while (0)


static struct cv {
    uint64_t start;
    uint64_t end;
    uint64_t sum;
    uint64_t max;
    uint64_t min;
} cond_time;


#define NUM_THREADS 64

#define SIZE                  4096
#define THR_CREATE_LOOPS      100
#define THR_LONG_CREATE_LOOPS 8
#define SPINLOCK_LOOPS        1000
#define CONDVAR_LOOPS         100
#define MWAIT_LOOPS           100
#define BIL                   1000000000
#define MIL                   1000000
#define LONG_LOCK_COUNT       (500*MIL)
#define LOCK_COUNT            1

static volatile uint64_t core_recvd[NUM_THREADS/64 + NUM_THREADS%64];
static volatile uint64_t core_counters[NUM_THREADS];


#ifndef __USER

#ifndef NAUT_CONFIG_XEON_PHI
#if 0
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


    for (i = 0; i < MWAIT_LOOPS; i++) {

        nk_thread_start(mwait_wakeme, &trigger, NULL, 0, TSTACK_DEFAULT, &t,nk_get_cpu_by_lapicid(2) );

        //nk_monitor((addr_t)&trigger, 0, 0);
        nk_mwait(0, 0);
        cond_time.end = rdtscp();
        PRINT("core %u woke up! %lu cycles\n", my_cpu_id(), cond_time.end - cond_time.start);
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
#endif /* !NAUT_CONFIG_XEON_PHI */

#endif

static FUNC_TYPE
wakeme FUNC_HDR
{
    COND_T * c = (COND_T*)in;
    DELAY(100);
    COND_SIG(c);
	rdtscll(cond_time.start);
	RETURN;
}

typedef struct container {
	COND_T * cvar;
	BARRIER_T * barrier;
	MUTEX_T lock;
} container_t;


static FUNC_TYPE
waitonit FUNC_HDR
{
	container_t * cont = (container_t*)in;
	COND_T * c = (COND_T*)cont->cvar;
	BARRIER_T * b = (BARRIER_T*)cont->barrier;
	uint32_t my_id;

/* BARRIER */
	my_id = GETCPU();

	/* wait at the barrier */
	BARRIER_WAIT(b);

	DELAY(10);

	MUTEX_LOCK(&(cont->lock));

	/* go to sleep */
	COND_WAIT(c, &(cont->lock));
	
	/* wakeup */
	rdtscll(core_counters[my_id]);

	bset(my_id, (unsigned long*)core_recvd);

	MUTEX_UNLOCK(&(cont->lock));
	RETURN;
}

/*
#ifdef __USER
#define BSP_CORE 0
#else
#define BSP_CORE 224
#endif
*/

#define BSP_CORE 0

static inline void
cores_wait(void)
{
    while (core_recvd[0] != 0xfffffffffffffffe);
}

#define REM_CORE 1

void time_cvar_bcast (void);
void time_cvar_bcast (void)
{
	int i, j;
	unsigned my_id;
	COND_T * c = malloc(sizeof(COND_T));
	BARRIER_T * b = malloc(sizeof(BARRIER_T));
	container_t * cont = malloc(sizeof(container_t));
	THREAD_T t[NUM_THREADS];
	
    udelay(100);

	my_id = GETCPU();

	/* setup arguments to threads */
	cont->cvar    = c;
	cont->barrier = b;

	/* clear timing info */
	memset((void*)core_counters, 0, sizeof(core_counters));
	memset((void*)core_recvd, 0, sizeof(core_recvd));


#ifdef __USER
	/* Make sure I'm pinned to the BSP core, already the case in Nautilus */
	cpu_set_t cp;
	CPU_ZERO(&cp);
	CPU_SET(BSP_CORE, &cp);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cp);
#endif

	for (i = 0; i < CONDVAR_LOOPS; i++) {
		uint64_t start = 0;

		COND_INIT(cont->cvar);
		MUTEX_INIT(&(cont->lock));

		BARRIER_INIT(cont->barrier, NUM_THREADS);

		for (j = 0; j < NUM_THREADS; j++) { 

			if (j == BSP_CORE) continue;

#ifdef __USER
			cpu_set_t cpuset;
			CPU_ZERO(&cpuset);
			CPU_SET(j, &cpuset);
			pthread_create(&t[j], NULL, waitonit, cont);
			pthread_setaffinity_np(t[j], sizeof(cpu_set_t), &cpuset);
#else
			nk_thread_start(waitonit, cont, NULL, 0, TSTACK_DEFAULT, &t[j], j);
#endif
		
		}

		/* wait for everyone to start up */
		BARRIER_WAIT(cont->barrier);

		DELAY(1000);

		/* set the timer */
		rdtscll(start);
		/* we can signal it now */
		COND_BCAST(cont->cvar);

		/* wait on everyone to finish waking up */
		cores_wait();

		/* join all the threads and print the trial */
		for (j = 0; j < NUM_THREADS; j++) {
			if (j == BSP_CORE) continue;

			if (start > core_counters[j]) {
				PRINT("STRANGENESS OCCURED IN CYCLE COUNT - start=%llu, end=%llu\n", start, core_counters[j]);
			}

			PRINT("TRIAL %u RC: %u %llu cycles\n", i, j, core_counters[j] - start);
			JOIN_FUNC(t[j], NULL);
		}

		/* clear timing info */
		memset((void*)core_counters, 0, sizeof(core_counters));
		memset((void*)core_recvd, 0, sizeof(core_recvd));
		
#ifdef __USER
		pthread_cond_destroy(cont->cvar);
		pthread_barrier_destroy(cont->barrier);
#else
		nk_condvar_destroy(cont->cvar);
		nk_barrier_destroy(cont->barrier);
#endif

		MUTEX_DEINIT(&(cont->lock));
	}

}


void time_condvar (void);
void time_condvar (void) {

    COND_T c;
    THREAD_T t;
    MUTEX_T l;
    int i,j;

    MUTEX_INIT(&l);
    COND_INIT(&c);

    memset(&cond_time, 0, sizeof(cond_time));

    for (j = 0; j < NUM_THREADS; j++) {

        if (j == 0) continue;

        for (i = 0; i < CONDVAR_LOOPS; i++) {

#ifdef __USER
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(j, &cpuset);
            pthread_create(&t, NULL, wakeme, &c);
            pthread_setaffinity_np(t, sizeof(cpu_set_t), &cpuset);
#else
            nk_thread_start(wakeme, &c, NULL, 0, TSTACK_DEFAULT, &t, j);
#endif

            MUTEX_LOCK(&l);

            COND_WAIT(&c, &l);

            rdtscll(cond_time.end);

            MUTEX_UNLOCK(&l);

            JOIN_FUNC(t, NULL);

            uint64_t diff = cond_time.end - cond_time.start;

            PRINT("TRIAL %u RC:%u %llu cycles\n", i, j, diff);
        }
    }

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
        rdtscll(start);
        rdtscll(end);
        total += end - start;
    }
    avg = total / SPINLOCK_LOOPS; 

    PRINT("RDTSC overhead: %lu cycles\n", avg);
    tsc_overhead = avg;
    avg = 0;
    total = 0;

    for (i = 0; i < SPINLOCK_LOOPS; i++) {
        uint64_t start, end;
        rdtscll(start);
        //for (j = 0; j < LOCK_COUNT; j++) {
            LOCK(&l);
            UNLOCK(&l);
        //}
        rdtscll(end);

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
create_test_func FUNC_HDR
{
    RETURN;
}


void time_threads_long(void);
void time_threads_long(void)
{
	THREAD_T t[NUM_THREADS];

	int i, j, k;
	uint64_t start,end;
	//int counts[] = {2,4,8,16,32,64,128,228};
    int counts[] = {2,4,8,16,32,64};

	for (k = 0; k < 6 ; k++) {

		for (i = 0; i < 100; i++) {

			rdtscll(start);

			/* TODO: each of these should go on a separate core */
			for (j = 0; j < counts[k]; j++) {
				if (j == BSP_CORE) continue;
#ifdef __USER
				cpu_set_t cpuset;
				CPU_ZERO(&cpuset);
				CPU_SET(j, &cpuset);
				pthread_create(&t[j], NULL, create_test_func, NULL);
				pthread_setaffinity_np(t[j], sizeof(cpu_set_t), &cpuset);
#else
				nk_thread_start(create_test_func, NULL, NULL, 0, TSTACK_DEFAULT, &t[j], j);
#endif
			}

			rdtscll(end);

			DELAY(10000);

			for (j = 0; j < counts[k]; j++) {

				if (j == BSP_CORE) continue;

				JOIN_FUNC(t[j], NULL);
			}

			DELAY(10000);

			memset(t, 0, sizeof(t));

			PRINT("TRIAL %u THREADS %u %llu\n", i, counts[k], end-start);

		}
	}
}


void time_thread_create(void);
void
time_thread_create (void)
{
    THREAD_T t;

    int i;
	uint64_t start,end;

    for (i = 0; i < THR_CREATE_LOOPS; i++) {
        rdtscll(start);
    #ifdef __USER
		pthread_attr_t attr;
		cpu_set_t cpus;
		pthread_attr_init(&attr);

		CPU_ZERO(&cpus);
		CPU_SET(1, &cpus);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
        pthread_create(&t, &attr, create_test_func, NULL);
    #else
        nk_thread_start(create_test_func, NULL, NULL, 0, TSTACK_DEFAULT, &t, 1);
    #endif
        rdtscll(end);

		DELAY(10000);
		PRINT("Trial %u %llu \n", i, end-start);

        JOIN_FUNC(t, NULL);

    }
}


static volatile int thread_run_done = 0;

static FUNC_TYPE
thread_run_func FUNC_HDR
{
	thread_run_done = 1;
    RETURN;
}


#define RUN_TRIALS 100

void time_thread_run(void);
void
time_thread_run (void)
{
	THREAD_T t;
	unsigned i;
	uint64_t start, end;

	for (i = 0; i < RUN_TRIALS; i++) {
#ifdef __USER
		pthread_attr_t attr;
		cpu_set_t cpus;
		pthread_attr_init(&attr);
		CPU_ZERO(&cpus);
		CPU_SET(1, &cpus);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
		pthread_create(&t, &attr, thread_run_func, NULL);
#else
        nk_thread_start(thread_run_func, NULL, NULL, 0, TSTACK_DEFAULT, &t, 1);
#endif

		rdtscll(start);

		while (!thread_run_done);

		rdtscll(end);

		DELAY(100);
	
		JOIN_FUNC(t, NULL);

		thread_run_done = 0;

		PRINT("TRIAL %u %llu cycles\n", i, end-start);
	}
}

/* this includes both the create and the latency for the thread to actually run */
void time_thread_both(void);
void
time_thread_both (void)
{
	THREAD_T t;
	unsigned i;
	uint64_t start, end;

	for (i = 0; i < RUN_TRIALS; i++) {

		rdtscll(start);

#ifdef __USER
		pthread_attr_t attr;
		cpu_set_t cpus;
		pthread_attr_init(&attr);
		CPU_ZERO(&cpus);
		CPU_SET(1, &cpus);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
		pthread_create(&t, &attr, thread_run_func, NULL);
#else
        nk_thread_start(thread_run_func, NULL, NULL, 0, TSTACK_DEFAULT, &t, 1);
#endif

		while (!thread_run_done);

		rdtscll(end);

		DELAY(100);
	
		JOIN_FUNC(t, NULL);

		thread_run_done = 0;

		PRINT("TRIAL %u %llu cycles\n", i, end-start);
	}
}


static volatile int done[2];
static volatile int ready[2];
static volatile int go;
#define YIELD_COUNT 100
#define CTX_SWITCH_TRIALS 100

typedef struct switch_cont {
	BARRIER_T * b;
	unsigned char id; /* 0 or 1 */
} switch_cont_t;


static FUNC_TYPE
thread_switch_func FUNC_HDR
{
	switch_cont_t * t = (switch_cont_t*)in;

	ready[t->id] = 1;

	YIELD();

	while (!go) { YIELD(); }
//BARRIER_WAIT(t->b);

	int i;
	for (i = 0; i < YIELD_COUNT; i++) {
		YIELD();
	}

	done[t->id] = 1;

	RETURN;
}



void time_ctx_switch(void);
void
time_ctx_switch (void)
{
	THREAD_T t[2];
	BARRIER_T * b = malloc(sizeof(BARRIER_T));
	switch_cont_t * cont1 = malloc(sizeof(switch_cont_t));
	switch_cont_t * cont2 = malloc(sizeof(switch_cont_t));
	uint64_t start = 0;
	uint64_t end = 0;
	int i;

	/* setup thread arguments */
	cont1->b = b;
	cont1->id = 0;
	cont2->b = b;
	cont2->id = 1;

	for (i = 0; i < CTX_SWITCH_TRIALS; i++)  {

		BARRIER_INIT(b, 3);

		// pin them both to core 1
#ifdef __USER
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(1, &cpuset);

		pthread_create(&t[0], NULL, thread_switch_func, cont1);
		pthread_setaffinity_np(t[0], sizeof(cpu_set_t), &cpuset);

		pthread_create(&t[1], NULL, thread_switch_func, cont2);
		pthread_setaffinity_np(t[1], sizeof(cpu_set_t), &cpuset);
#else
		nk_thread_start(thread_switch_func, cont1, NULL, 0, TSTACK_DEFAULT, &t[0], 1);
		nk_thread_start(thread_switch_func, cont2, NULL, 0, TSTACK_DEFAULT, &t[1], 1);

#endif

		DELAY(10000);

		while ( !(ready[0] && ready[1]) );

		go = 1;

		//BARRIER_WAIT(b);

		rdtscll(start);
		while ( !(done[0] && done[1]) );
		rdtscll(end);

		/* is this accurate? */
		PRINT("TRIAL %u %llu\n", i, (end-start)/(YIELD_COUNT*2));

		JOIN_FUNC(t[0], NULL);
		JOIN_FUNC(t[1], NULL);

		BARRIER_DESTROY(b);

		done[0] = 0;
		done[1] = 0;
		ready[0] = 0;
		ready[1] = 0;
		go = 0;

	}
}

void time_ipi_send (void);
void
time_ipi_send(void)
{
    int i;
    struct apic_dev * apic = per_cpu_get(apic);
    uint64_t start, end;
    for (i = 0; i < 100; i++) {
        rdtscll(start);

        apic_ipi(apic, 1, APIC_NULL_KICK_VEC);

        rdtscll(end);

        PRINT("TRIAL %u %llu\n", i, end-start);
    }
}

#define TRIALS 100
static uint64_t int80_end = 0;

static int
int80_handler (excp_entry_t * excp, excp_vec_t v, void *state)
{
    rdtscll(int80_end);
    return 0;
}

void time_int80 (void);
void
time_int80 (void)
{
    int i;
    uint64_t start;
    if (register_int_handler(0x80, int80_handler, NULL)) {
	PRINT("FAILED TO REGISTER HANDLER FOR INT 0x80\n");
	return;
    }
    sti();
    for (i = 0; i < TRIALS; i++) {
        rdtscll(start);
        asm volatile ("":::"memory");
        asm volatile ("int $0x80");

        PRINT("TRIAL %u %llu\n", i, int80_end-start);

        int80_end = 0;
        start = 0;
    }
}

uint64_t syscall_end = 0;
static uint64_t syscall_start = 0;


void syscall_handler(void)
{
}

static void
syscall_setup (void)
{
    uint64_t r;

    /* enable fast syscall */
    r = msr_read(IA32_MSR_EFER);
    r |= EFER_SCE;
    msr_write(IA32_MSR_EFER, r);

    /* SYSCALL and SYSRET CS in upper 32 bits */
    msr_write(AMD_MSR_STAR, ((0x8llu & 0xffffllu) << 32) | ((0x8llu & 0xffffllu) << 48));

    /* target address */
    msr_write(AMD_MSR_LSTAR, (uint64_t)syscall_handler);
}


void time_syscall (void);
void
time_syscall (void)
{
    int i;
    uint64_t start;

    syscall_setup();

    for (i = 0; i < TRIALS; i++) {

        rdtscll(syscall_start);

        /* the callee will just do a retq, no sysret. Demeted huh? */
        asm volatile ("pushq $b%=\n\t"
                      "syscall\n\t"
                      "b%=:\n" : : : "memory", "rcx");


        PRINT("TRIAL %u %llu\n", i, syscall_end-syscall_start);

        syscall_end  = 0;

    }

}

static uint64_t nemo_end = 0;
static volatile int nemo_done = 0;
static void
nemo_wakeup (void)
{
    rdtscll(nemo_end);
    nemo_done = 1;
}

void time_nemo_event (void);
void
time_nemo_event (void)
{
    unsigned i,j;
    uint64_t start;

    nemo_init();

    nemo_event_id_t id = nemo_register_event_action(nemo_wakeup, NULL);

    if (id < 0) {
        printk("couldn't register test nemo task\n");
        return;
    }

    for (i = 0; i < NUM_THREADS; i++) {

        if (i == BSP_CORE) continue;

        for (j = 0; j < TRIALS; j++) {

            rdtscll(start);

            nemo_event_notify(id, i);

            while (!nemo_done);

            PRINT("TRIAL %u RC: %u %llu cycles \n",
                j, 
                i, 
                nemo_end-start);

            nemo_done = 0;
            nemo_end = 0;

        }
    }
}


static void
nemo_bcast_wakeup (void)
{
    uint64_t tmp;
    rdtscll(tmp);
    core_counters[my_cpu_id()] = tmp;
    bset(my_cpu_id(),(unsigned long*)core_recvd);
}


void time_nemo_bcast (void);
void
time_nemo_bcast (void)
{
    unsigned i;
    uint64_t start;

    nemo_init();

    nemo_event_id_t id = nemo_register_event_action(nemo_bcast_wakeup, NULL);

    for (i = 0; i < TRIALS; i++) {

        rdtscll(start);

        nemo_event_broadcast(id);

        cores_wait();

        int j;
        for (j = 0; j < NUM_THREADS; j++) {

            if (j==BSP_CORE) continue;

            if (start > core_counters[j]) {
                panic("Strangeness occured in cycle count - start=%llu end=%llu\n", start, core_counters[j]);

            }

            PRINT("TRIAL %u RC: %u %llu cycles \n",
                    i, 
                    j, 
                    core_counters[j] - start);

        }

        memset((void*)core_recvd, 0, sizeof(uint64_t)*4);
        memset((void*)core_counters, 0, sizeof(uint64_t)*NUM_THREADS);

    }
}

static volatile int sync_go = 0;
static volatile int sync_done = 0;
static uint64_t sync_end = 0;

static FUNC_TYPE
sync_wakeup FUNC_HDR
{
    while (!sync_go);

    rdtscll(sync_end);

    sync_done = 1;

    RETURN;
}


void time_sync_event (void);
void 
time_sync_event (void)
{
    unsigned i, j;
    uint64_t start;

    for (i = 0; i < NUM_THREADS; i++) {

        if (i == BSP_CORE) continue;

        for (j = 0; j < TRIALS; j++) {

            THREAD_T t;

            nk_thread_start(sync_wakeup, NULL, NULL, 0, TSTACK_DEFAULT, &t, i);

            DELAY(100000);

            rdtscll(start);

            /* signal */
            sync_go = 1;

            while (!sync_done);

            PRINT("TRIAL %u RC: %u %llu cycles\n", j, i, sync_end-start);

            sync_go   = 0;
            sync_end  = 0;
            sync_done = 0;

            JOIN_FUNC(t, NULL);
        }
    }
}

#ifndef __USER
#if 0
void page_alloc_test(void);
void 
page_alloc_test (void)
{

#define N 100

	void * x[N];
	int i, j;

	for (j = 0; j < 10; j++) {
		printk("Trial %u\n", j);

		for (i = 0; i < N; i++) {

			x[i] = (void*)nk_alloc_pages(16);
		}

		for (i = 0; i < N; i++) {
			nk_free_pages(x[i], 16);
		}

		nk_dump_page_map();
	}


}
#endif

#undef N
#define N 10000
void malloc_test(void);
void
malloc_test (void)
{
	void * x[N];
	int i;
		

	for (i = 0; i < N; i++) {
		x[i] = malloc(4096);
		memset(x[i], 0, 4096);
	}


	for (i = 0; i < N; i++) {
		free(x[i]);
	}

}

#endif

void run_benchmarks(void);
void 
run_benchmarks(void)
{
    time_syscall();
}

#ifdef __USER 

int main () {

#ifdef CREATE
    time_thread_create();
#endif

#ifdef CREATE_LONG
    time_threads_long();
#endif

#ifdef RUN
	time_thread_run();
#endif

#ifdef BOTH
	time_thread_both();
#endif

#ifdef CONDVAR
    time_condvar();
#endif

#ifdef CONDVAR_BCAST
    time_cvar_bcast();
#endif

#ifdef CTX_SWITCH
    time_ctx_switch();
#endif
    return 0;
}

static int
handle_bench (char * buf, void * priv)
{
    run_benchmarks();
    return 0;
}

static struct shell_cmd_impl bench_impl = {
    .cmd      = "bench",
    .help_str = "bench",
    .handler  = handle_bench,
};
nk_register_shell_cmd(bench_impl);

#endif
