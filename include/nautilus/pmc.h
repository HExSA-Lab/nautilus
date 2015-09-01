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
#ifndef __PMC_H__
#define __PMC_H__

#include <nautilus/naut_types.h>


#define NUM_PERF_SLOTS AMD_PERF_SLOTS

#define PMC_SLOT_FREE 0
#define PMC_SLOT_USED 1

#define AMD_PERF_SLOTS    6
#define AMD_PERF_CTL0_MSR 0xc0010200
#define AMD_PERF_CTL1_MSR 0xc0010202
#define AMD_PERF_CTL2_MSR 0xc0010204
#define AMD_PERF_CTL3_MSR 0xc0010206
#define AMD_PERF_CTL4_MSR 0xc0010208
#define AMD_PERF_CTL5_MSR 0xc001020a

#define AMD_PERF_CTR0_MSR 0xc0010201
#define AMD_PERF_CTR1_MSR 0xc0010203
#define AMD_PERF_CTR2_MSR 0xc0010205
#define AMD_PERF_CTR3_MSR 0xc0010207
#define AMD_PERF_CTR4_MSR 0xc0010209
#define AMD_PERF_CTR5_MSR 0xc001020b

#define PERF_CTL_MSR_N(n) (AMD_PERF_CTL0_MSR + 2*(n))
#define PERF_CTR_MSR_N(n) (AMD_PERF_CTR0_MSR + 2*(n))


/* EVENTS */

#define AMD_PMC_DCACHE_MISS  0x41 // PERF_CTL[5:0]
#define AMD_PMC_ICACHE_MISS  0x81 // PERF_CTL[2:0]
#define AMD_PMC_L2_MISS      0x7e // PERF_CTL[2:0]
#define AMD_PMC_TLB_MISS     0x46 // PERF_CTL[2:0]

/* Counts the number of SMIs received. */
#define AMD_PMC_SMI_CNT      0x2b // PERF_CTL[5:0]


/* The number of cycles the instruction fetcher is stalled for the core. This
 * may be for a variety of reasons such as branch predictor updates,
 * unconditional branch bubbles, far jumps and cache misses, instruction
 * fetching for the other core while instruction fetch for this core is
 * stalled, among others. May be overlapped by instruction dispatch stalls or
 * instruction execution, such that these stalls don't necessarily impact
 * performance. */
#define AMD_PMC_IFETCH_STALL 0x87 // PERF_CTL[2:0]

/* The number of branch instructions retired, of any type, that were not
 * correctly predicted.  This includes those for which prediction is not
 * attempted (far control transfers, exceptions and interrupts).
 */
#define AMD_PMC_BRANCH_MISS  0xC3 // PERF_CTL[5:0]

/* the number of instruction cache lines invalidated. a non-smc event is cmc
 * (cross modify- ing code), either from the other core of the compute unit or
 * another compute compute unit. */
#define AMD_PMC_ICACHE_INV   0x8C // PERF_CTL[2:0]


typedef struct event_prop {
    const char * name;
    uint8_t slot_mask;
} event_prop_t;

typedef struct pmc_event {
    uint8_t assigned_idx;
    uint8_t enabled;
    uint8_t id;
    event_prop_t * prop;
} perf_event_t;



typedef struct perf_slot {
    uint8_t status;
    perf_event_t * event;
} perf_slot_t;


typedef struct pmc_ctl {

    union {
        uint64_t val;
        struct {
            uint8_t event_select0 : 8;
            uint8_t unit_mask     : 8;
            uint8_t usr           : 1;
            uint8_t os            : 1;
            uint8_t edge          : 1;
            uint8_t rsvd0         : 1;
            uint8_t int_enable    : 1;
            uint8_t rsvd1         : 1;
            uint8_t en            : 1;
            uint8_t inv           : 1;
            uint8_t cnt_mask      : 8;
            uint8_t event_select1 : 4;
            uint8_t rsvd2         : 4;
            uint8_t hg_only       : 2;
            uint32_t rsvd3        : 22;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed)) pmc_ctl_t;


perf_event_t * assign_perf_event(uint8_t event_id, uint8_t unit_mask);
void release_perf_event(perf_event_t * event);

void enable_perf_event(perf_event_t * event);
void enable_all_events(void);
void disable_perf_event(perf_event_t * event);
void disable_all_events(void);
void reset_all_counters(void);

uint64_t read_event_count(perf_event_t * event);

int pmc_init(void);
void perf_report(void);



#endif /* !__PMC_H__! */
