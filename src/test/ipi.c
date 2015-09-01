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
 * http://xtack.sandia.gov/hobbes
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
#include <nautilus/nautilus.h>
#include <dev/apic.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <nautilus/percpu.h>
#include <nautilus/atomic.h>
#include <nautilus/math.h>
#include <nautilus/naut_assert.h>

#define PING_VEC 0x42
#define PONG_VEC 0x32
#define PONG_BCAST_VEC 0x33

#define rdtscll(val)                    \
    do {                        \
    uint64_t tsc;                   \
    uint32_t a, d;                  \
    asm volatile("rdtsc" : "=a" (a), "=d" (d)); \
    *(uint32_t *)&(tsc) = a;            \
    *(uint32_t *)(((unsigned char *)&tsc) + 4) = d;   \
    val = tsc;                  \
    } while (0)


#define NUM_CORES 64

static volatile int done = 0;
// 256 bits for 228 cores
static volatile uint64_t core_recvd[4];
static volatile uint64_t core_counters[NUM_CORES];

static inline void 
bset(unsigned int nr, unsigned long * addr)
{
        asm volatile("lock bts %1, %0"
            : "+m" (*(volatile long *)(addr)) : "Ir" (nr) : "memory");
}

#define TRIALS 100
void ipi_test_setup(void);
void ipi_begin_test(cpu_id_t);

static uint64_t diffs[TRIALS];

static struct time_struct {
    uint64_t start_cycle;
    uint64_t end_cycle;
    uint64_t sum;
    uint64_t max;
    uint64_t min;
    volatile uint64_t  done;
} time;


static inline void __always_inline
time_start (void)
{
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    time.start_cycle = lo | ((uint64_t)(hi) << 32);
}


static inline void __always_inline
time_end (void)
{
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    time.end_cycle = lo | ((uint64_t)(hi) << 32);
}


int
ping (excp_entry_t * excp, excp_vec_t vec);
int
ping (excp_entry_t * excp, excp_vec_t vec)
{
    struct apic_dev * apic = per_cpu_get(apic);
    apic_write(apic, APIC_REG_ICR2,  0<< APIC_ICR2_DST_SHIFT);
    apic_write(apic, APIC_REG_ICR, APIC_DEL_MODE_FIXED | PONG_VEC);
    IRQ_HANDLER_END();
    return 0;
}


static uint64_t ipi_oneway_end = 0;

static int
pong (excp_entry_t * excp, excp_vec_t vec)
{
    rdtscll(ipi_oneway_end);
    IRQ_HANDLER_END();
    *(volatile int *)&done = 1;
    return 0;
}

static int
pong_bcast (excp_entry_t * excp, excp_vec_t vec)
{
    uint64_t tmp;
    rdtscll(tmp);
    core_counters[my_cpu_id()] = tmp;
	bset(my_cpu_id(), (unsigned long *)core_recvd);
	IRQ_HANDLER_END();
	return 0;
}


void
ipi_test_setup (void)
{
    if (register_int_handler(PING_VEC, ping, NULL) != 0) {
        ERROR_PRINT("Could not register int handler\n");
        return;
    }

    if (register_int_handler(PONG_VEC, pong, NULL) != 0) {
        ERROR_PRINT("Could not register int handler\n");
        return;
    }

    if (register_int_handler(PONG_BCAST_VEC, pong_bcast, NULL) != 0) {
        ERROR_PRINT("Could not register int handler\n");
        return;
    }
}

#define APIC_WRITE(apic, reg, val) \
    *(volatile uint32_t*)(apic->base_addr + (reg)) = (val);

void
ipi_begin_test (cpu_id_t cpu)
{
    struct apic_dev * apic = per_cpu_get(apic);
    int i;

	unsigned remote_apic = nk_get_nautilus_info()->sys.cpus[cpu]->lapic_id;

	/* warm it up */
    for (i = 0; i < TRIALS; i++) {
        apic_write(apic, APIC_REG_ESR, 0);
        apic_write(apic, APIC_REG_ICR2, 0 | (remote_apic<< APIC_ICR2_DST_SHIFT));
        apic_write(apic, APIC_REG_ICR, 0 | APIC_DEL_MODE_FIXED | PING_VEC);
		while (!(*(volatile int*)&done));
		done = 0;
	}

    for (i = 0; i < TRIALS; i++) {
    
        uint64_t end = 0;
        uint64_t start = 0;

        apic_write(apic, APIC_REG_ESR, 0);

        apic_write(apic, APIC_REG_ICR2, 0 | (remote_apic << APIC_ICR2_DST_SHIFT));
        apic_write(apic, APIC_REG_ICR, 0 | APIC_DEL_MODE_FIXED | PING_VEC);

        rdtscll(start);

        while (!(*(volatile int*)&done));

        rdtscll(end);

        if (start > end) {
            ERROR_PRINT("Strangeness occured in cycle count\n");
        }

		done = 0;

		printk("TRIAL %u RC:%u %u cycles\n", i, cpu, end-start);
        udelay(10);
	}

}

void ipi_oneway_test (cpu_id_t cpu);
void
ipi_oneway_test (cpu_id_t cpu)
{
    struct apic_dev * apic = per_cpu_get(apic);
    int i;

	unsigned remote_apic = nk_get_nautilus_info()->sys.cpus[cpu]->lapic_id;

	/* warm it up */
    for (i = 0; i < TRIALS; i++) {
        apic_write(apic, APIC_REG_ICR2, 0 | (remote_apic << APIC_ICR2_DST_SHIFT));
        apic_write(apic, APIC_REG_ICR, 0 | APIC_DEL_MODE_FIXED | PONG_VEC);
		while (!done);
		done = 0;
	}

    for (i = 0; i < TRIALS; i++) {
    
        uint64_t start = 0;

        apic_write(apic, APIC_REG_ICR2, 0 | (remote_apic << APIC_ICR2_DST_SHIFT));
        apic_write(apic, APIC_REG_ICR, 0 | APIC_DEL_MODE_FIXED | PONG_VEC);

        rdtscll(start);

        /* TODO: put something here that I know the latency of. Where is the time going? */

        while (!(*(volatile int*)&done));

#if 0
        if (start > ipi_oneway_end) {
            ERROR_PRINT("Strangeness occured in cycle count\n");
        }
#endif

		done = 0;

		printk("TRIAL %u RC:%u %u cycles\n", i, cpu, ipi_oneway_end-start);
	}
}


static inline void
cores_wait(void)
{
    unsigned long i = 0;
    while (core_recvd[0] != 0xfffffffffffffffe) {
#if 0
        if (++i % 10000000 == 0) {
            printk("SPIN WARNING: 0x%016llx\n", core_recvd[0]);
        }
#endif
    }

}


void ipi_bcast_test (void);
void
ipi_bcast_test (void)
{
    struct apic_dev * apic = per_cpu_get(apic);
    int i;
    uint64_t start = 0;

	/* warm it up */
    for (i = 0; i < TRIALS; i++) {
        apic_write(apic, APIC_REG_ESR, 0);
		apic_bcast_ipi(apic, PONG_BCAST_VEC);
		cores_wait();
	}

	memset((void*)core_recvd, 0, sizeof(uint64_t)*4);
	memset((void*)core_counters, 0, sizeof(uint64_t)*NUM_CORES);

    for (i = 0; i < TRIALS; i++) {
    

        apic_write(apic, APIC_REG_ESR, 0);

        rdtscll(start);

		apic_bcast_ipi(apic, PONG_BCAST_VEC);

        cores_wait();

		int j;
		for (j = 0; j < NUM_CORES; j++) {

			if (j == 0) continue;

			if (start > core_counters[j]) {
				panic("Strangeness occured in cycle count - start=%llu end=%llu\n", start, core_counters[j]);
			}

			printk("TRIAL %u RC:%u %u cycles\n", i, j, core_counters[j] - start);
		}

		memset((void*)core_recvd, 0, sizeof(uint64_t)*4);
		memset((void*)core_counters, 0, sizeof(uint64_t)*NUM_CORES);

	}

}


