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
#ifndef __HPET_H__
#define __HPET_H__

#include <nautilus/nautilus.h>
#include <nautilus/bits.h>

int nk_hpet_init(void);
inline uint64_t nk_hpet_get_freq(void);
inline uint64_t nk_hpet_get_cntr(void);

#define HPET_GEN_CAP_ID_REG   0x00
#define HPET_GEN_CFG_REG      0x10
#define HPET_GEN_INT_STAT_REG 0x20
#define HPET_MAIN_CTR_REG     0xf0

#define TIMER_N_CFG_CAP_REG(n) (0x100 + 0x20*(n))
#define TIMER_N_CMP_VAL_REG(n) (0x108 + 0x20*(n))
#define TIMER_N_FSB_IR_REG(n)  (0x110 + 0x20*(n))

#define GEN_CAP_GET_CLK_PER(x)    ((x) >> 32)
#define GEN_CAP_GET_VENDOR_ID(x)  (((x) >> 16) & 0xffffULL)
#define GEN_CAP_GET_LEG_RT_CAP(x) __CHECK_BIT(x, 15)
#define GEN_CAP_GET_CNT_SZ_CAP(x) __CHECK__BIT(x, 13)
#define GEN_CAP_GET_NUM_TIMERS(x) ( (((x)>>8) & 0xfULL) + 1)
#define GEN_CAP_GET_REV_ID(x)     ((x) & 0xffULL)

#define GEN_CFG_GET_LEG_RT_CNF(x) __CHECK_BIT(x, 1)
#define GEN_CFG_GET_CNF_EN(x)     __CHECK_BIT(x, 0)

#define GEN_INT_GET_TN_INT_STAT(x, n) __CHECK_BIT(x, n)

#define TN_GET_INT_ROUTE_CAP(x)     ((x) >> 32)
#define TN_GET_FSB_INT_DEL_CAP(x)   __CHECK_BIT(x, 15)
#define TN_GET_FSB_EN_CNF(x)        __CHECK_BIT(x, 14)
#define TN_GET_INT_ROUTE_CNF(x)     (((x)>>9) & 0x1fULL)
#define TN_GET_32MODE_CNF(x)        __CHECK_BIT(x, 8)
#define TN_GET_VAL_SET_CNF(x)       __CHECK_BIT(x, 6)
#define TN_GET_SIZE_CAP(x)          __CHECK_BIT(x, 5)
#define TN_GET_PER_INT_CAP(x)       __CHECK_BIT(x, 4)
#define TN_GET_TYPE_CNF(x)          __CHECK_BIT(x, 3)
#define TN_GET_INT_ENB_CNF(x)       __CHECK_BIT(x, 2)
#define TN_GET_INT_TYPE_CNF(x)      __CHECK_BIT(x, 1)

#define TN_ENABLE (1<<2)
#define TN_PERIODIC (1<<3)
#define TN_VAL_SET   (1<<6)

struct hpet_dev;


typedef enum {TIMER_TYPE_ONESHOT, TIMER_TYPE_PERIODIC} hpet_cmp_type_t;
typedef enum {TIMER_SIZE_32, TIMER_SIZE_64} hpet_cmp_size_t;
typedef enum {TIMER_EDGE, TIMER_LEVEl} hpet_cmp_int_type_t;
typedef enum {TIMER_DISABLED, TIMER_RUNNING} hpet_cmp_stat_t;

/* TODO: we should probably unify timers (using callbacks etc) */

struct hpet_comparator {
    uint8_t idx; /* which timer number am I */
    struct hpet_dev * parent;

    uint32_t int_route_cap;   /* which IOAPIC pins can I be mapped to? */
    uint8_t supports_periodic; 
    uint8_t supports_fsb_del; /* HW supports direct FSB delivery of this timer's irq */
    hpet_cmp_size_t size;   /* is the timer 32-bit (0) or 64-bit? (1) */

    /* runtime configuration */
    uint8_t int_route;        /* which IOAPIC pin *am* I mapped to? */
    uint8_t timer_32mode;     /* am I configured as a 32-bit timer? */
    hpet_cmp_type_t mode;        /* oneshot or periodic? */
    uint8_t timer_val_set;    /* will writes be allowed to the accumulator? */


    hpet_cmp_stat_t stat; /* enabled or not? */
    hpet_cmp_int_type_t int_type; /* edge or level? */

};

typedef enum { HPET_MAP_STD, HPET_MAP_LEGACY } hpet_map_type_t;
typedef enum { HPET_COUNTER_HALTED, HPET_COUNTER_RUNNING } hpet_stat_t;

struct hpet_dev {
    ulong_t base_addr;
    ulong_t freq;
    uint16_t vendor;
    uint8_t  rev_id;

    uint8_t wide_timer; /* main counter can operate in 64b mode */
    uint8_t leg_rep;   /* can use legacy replacement mapping */
    
    uint8_t num_cmps; /* how many comparators we have */

    /* from ACPI */
    uint32_t id;
    uint16_t min_tick;
    uint8_t flags;
    uint8_t seq; // sequence number

    unsigned long nanos_per_tick;

    /* info about the base address */
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t space_id;
    uint8_t access_width;

    /* Runtime configuration info */
    hpet_map_type_t mapping_mode;
    hpet_stat_t cntr_status;

    /* must be last */
    struct hpet_comparator cmps[0];

};

static inline void
hpet_write (struct hpet_dev * hpet, uint32_t reg, uint64_t val)
{
    *((volatile uint64_t*)(hpet->base_addr + reg)) = val;
}

static inline uint64_t
hpet_read (struct hpet_dev * hpet, uint32_t reg)
{
    return *((volatile uint64_t*)(hpet->base_addr + reg));
}

struct naut_info;
extern struct naut_info * nk_get_nautilus_info(void);
static inline unsigned long
nk_hpet_nanos_per_tick (void)
{
    struct hpet_dev * hpet = nk_get_nautilus_info()->sys.hpet;
    return hpet->nanos_per_tick;
}

#endif
