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
 * Copyright (c) 2018, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __PMC_H__
#define __PMC_H__

#include <nautilus/naut_types.h>


#define AMD_PMC_LEAF      0x80000001
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

#define AMD_PERF_CTL_MSR_N(n) (AMD_PERF_CTL0_MSR + 2*(n))
#define AMD_PERF_CTR_MSR_N(n) (AMD_PERF_CTR0_MSR + 2*(n))

#define IA32_PMC_LEAF 0xa

#define IA32_PMC_BASE         0x0c1
#define IA32_PERFEVTSEL_BASE  0x186
#define INTEL_PERF_CTL_MSR_N(n) (IA32_PERFEVTSEL_BASE + (n))
#define INTEL_PERF_CTR_MSR_N(n) (IA32_PMC_BASE + (n))


/* EVENTS */

/* Intel */
#define INTEL_UNHALTED_CORE_CYCLES 0x00
#define INTEL_INSTR_RETIRED        0x01
#define INTEL_UNHALTED_REF_CYCLES  0x02
#define INTEL_LLC_REF              0x03
#define INTEL_LLC_MISS             0x04
#define INTEL_BRANCH_RETIRED       0x05
#define INTEL_BRANCH_MISS_RETIRED  0x06

#define INTEL_NUM_ARCH_EVENTS 7

/* Broadwell-specific */
#define INTEL_DTLB_LOAD_MISS_WALK  0x07


/* AMD */
#define AMD_DATA_CACHE_MISS      0x00
#define AMD_INSTR_CACHE_MISS     0x01
#define AMD_L2_CACHE_MISS        0x02
#define AMD_UNIFIED_TLB_MISS     0x03
#define AMD_SMI_INT              0x04
#define AMD_INSTR_FETCH_STALLS   0x05
#define AMD_BRANCH_MISS_RETIRED  0x06
#define AMD_INSTR_CACHE_INVALS   0x07


#define AMD_EXT_CNT_FLAG 0x1
#define AMD_NB_CNT_FLAG  0x2
#define AMD_L3_CNT_FLAG  0x4


typedef struct amd_event_attr {
    const char * name;
    uint8_t id;
    uint8_t umask;
    uint8_t slot_mask;
} amd_event_attr_t;

typedef struct intel_event_attr {
    const char * name;
    uint8_t id;
    uint8_t umask;
    uint8_t bit_pos;
} intel_event_attr_t;


typedef struct event_platform_attrs {

    union {
        struct intel_event_attr * intel_attrs;
        struct amd_event_attr * amd_attrs;
    };

} event_platform_attrs_t;

typedef struct pmc_event {
    uint8_t assigned_idx;
    uint8_t enabled;
    uint8_t bound;

    uint32_t sw_id;

    uint64_t val;

    const char * name;
    event_platform_attrs_t attrs;
} perf_event_t;


typedef enum {
    PMC_SLOT_FREE = 0,
    PMC_SLOT_USED = 1,
} slot_status_t;


typedef struct perf_slot {
    slot_status_t status;
    perf_event_t * event;
} perf_slot_t;


typedef struct pmc_ctl_intel {
    
    union {
        uint64_t val;
        struct {
            uint8_t event_select;
            uint8_t unit_mask;
            uint8_t usr : 1;
            uint8_t os : 1;
            uint8_t e : 1;
            uint8_t pc : 1;
            uint8_t intr : 1;
            uint8_t any : 1;
            uint8_t en : 1;
            uint8_t inv : 1;
            uint8_t cmask;
            uint32_t rsvd1;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed)) pmc_ctl_intel_t;


typedef struct pmc_ctl_amd {

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
} __attribute__((packed)) pmc_ctl_amd_t;


struct pmc_ops {
    uint64_t (*read_ctr)(uint8_t idx);
    void     (*write_ctr)(uint8_t idx, uint64_t val);
    void     (*enable_ctr)(perf_event_t * event);
    void     (*disable_ctr)(perf_event_t * event);
    int      (*bind_ctr)(perf_event_t * event, int slot);
    void     (*unbind_ctr)(int slot);
    int      (*init)(struct pmc_info * pmc);
    void     (*event_init)(perf_event_t * event);

    int      (*version)();
    int      (*msr_cnt)();
    int      (*msr_width)();

};

typedef struct pmc_info {

    int valid;

    int hw_num_slots;
    int sw_num_slots;
    perf_slot_t * slots;

    uint8_t version_id;
    uint8_t msr_cnt;
    uint8_t msr_width;

    struct pmc_ops * ops;

    union {
        uint32_t intel_fl;
        uint32_t amd_fl;
    };

} pmc_info_t;


/* EXTERNAL INTERFACE */

int      nk_pmc_init(struct naut_info * naut);

perf_event_t * nk_pmc_create(uint32_t event_id);
void           nk_pmc_destroy(perf_event_t * event);

void     nk_pmc_start(perf_event_t * event);
void     nk_pmc_stop(perf_event_t * event);

uint64_t nk_pmc_read(perf_event_t * event);
void     nk_pmc_write(perf_event_t * event, uint64_t val);

void     nk_pmc_report(void);

#endif /* !__PMC_H__! */
