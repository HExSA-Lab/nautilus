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
#include <nautilus/acpi.h>
#include <nautilus/cpu.h>
#include <nautilus/paging.h>
#include <nautilus/intrinsics.h>
#include <nautilus/naut_assert.h>
#include <nautilus/dev.h>
#include <dev/hpet.h>
#include <nautilus/math.h>

#ifndef NAUT_CONFIG_DEBUG_HPET
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif 

#define HPET_PRINT(fmt, args...) printk("HPET: " fmt, ##args)
#define HPET_DEBUG(fmt, args...) DEBUG_PRINT("HPET: " fmt, ##args)


#define FEMTOS_PER_SEC 0x38D7EA4C68000


inline uint64_t
nk_hpet_get_freq (void)
{
    struct hpet_dev * hpet = nk_get_nautilus_info()->sys.hpet;

    if (!hpet) {
        panic("No HPET device found\n");
    }

    return hpet->freq;
}


inline uint64_t 
nk_hpet_get_cntr (void)
{
    struct hpet_dev * hpet = nk_get_nautilus_info()->sys.hpet;
    if (!hpet) {
        panic("No HPET device found\n");
        return 0;
    }

    return hpet_read(hpet, HPET_MAIN_CTR_REG);
}



/* 
 * This enables the HPET
 */
static inline void 
hpet_cntr_run (struct hpet_dev * hpet)
{
    uint64_t val = hpet_read(hpet, HPET_GEN_CFG_REG);
    hpet_write(hpet, HPET_GEN_CFG_REG, val | 0x1);
    hpet->cntr_status = HPET_COUNTER_RUNNING;
}


static inline void
hpet_cntr_halt (struct hpet_dev * hpet)
{
    uint64_t val = hpet_read(hpet, HPET_GEN_CFG_REG);
    hpet_write(hpet, HPET_GEN_CFG_REG, val & ~0x1);
    hpet->cntr_status = HPET_COUNTER_HALTED;
}


static inline hpet_stat_t
hpet_get_cntr_stat (struct hpet_dev * hpet)
{
    return hpet->cntr_status;
}


static inline uint8_t 
hpet_get_cmp_stat (struct hpet_comparator * cmp)
{
    return cmp->stat;
}

static inline void
hpet_cmp_run (struct hpet_comparator * cmp)
{
    hpet_write(cmp->parent, TIMER_N_CFG_CAP_REG(cmp->idx), 
            hpet_read(cmp->parent, TIMER_N_CFG_CAP_REG(cmp->idx)) | TN_ENABLE);
}

static inline void
hpet_cmp_stop (struct hpet_comparator * cmp)
{
    hpet_write(cmp->parent, TIMER_N_CFG_CAP_REG(cmp->idx), 
            hpet_read(cmp->parent, TIMER_N_CFG_CAP_REG(cmp->idx)) & ~TN_ENABLE);
}

static void 
hpet_write_cmp_val (struct hpet_comparator * cmp, uint64_t val)
{
    hpet_write(cmp->parent, TIMER_N_CMP_VAL_REG(cmp->idx), val);
}


static struct hpet_comparator *
hpet_get_free_timer (struct hpet_dev * hpet)
{
    int i;
    for (i = 0; i < hpet->num_cmps; i++) {
        if (hpet_get_cmp_stat(&hpet->cmps[i]) == TIMER_DISABLED) {
            return &(hpet->cmps[i]);
        }
    }
    return NULL;
}


/* TODO THIS NEEDS TO BE IMPLEMENTED */

static int
hpet_request_irq (struct hpet_comparator * cmp, uint32_t mask)
{
    int irq;

    ASSERT(cmp);
    /* nk_request_irq -> scan IOAPICS -> check each ioapic entry to 
     * see if it has been assigned */

    irq = 0;//nk_request_irq(cmp->int_route_cap);

    if (irq < 0) {
        return -1;
    }

    cmp->int_route = irq;

    /* TODO: setup a handler */

    /* Tell the timer which IRQ to raise */
    hpet_write(cmp->parent, 
            TIMER_N_CFG_CAP_REG(cmp->idx), 
            hpet_read(cmp->parent, TIMER_N_CFG_CAP_REG(cmp->idx)) | (irq & 0x1fU) << 9);

    return 0;
}


int
hpet_set_oneshot (uint64_t ticks)
{
    struct hpet_dev * hpet = nk_get_nautilus_info()->sys.hpet;
    struct hpet_comparator * cmp = NULL;

    if (!hpet) {
        return -1;
    }

    cmp = hpet_get_free_timer(hpet);
    if (!cmp) {
        HPET_DEBUG("Could not find a free HPET timer\n");
        return -1;
    }

    /* do we need to set up this timer for an IOAPIC pin? */
    if (!cmp->int_route) {
        if (hpet_request_irq(cmp, cmp->int_route_cap) != 0) {
            HPET_DEBUG("Could not assign IRQ for HPET timer\n");
            return -1;
        }
    }

    /* set the count and turn on the timer */
    hpet_write_cmp_val(cmp, hpet_read(hpet, HPET_MAIN_CTR_REG) + ticks);

    /* turn on the main counter if it isn't already */
    if (hpet_get_cntr_stat(hpet) == HPET_COUNTER_HALTED) {
        hpet_cntr_run(hpet);
    }

    hpet_cmp_run(cmp);

    return 0;
}


/* 
 * this is really just the bare minimum initialization
 * we will do more when we actually assign the timer
 *
 */
static int
hpet_init_comparator (struct hpet_dev * hpet,
                      struct hpet_comparator * cmp, 
                      unsigned idx)
{
    uint64_t val;

    HPET_DEBUG("Setting up Timer %u\n", idx, (void*)cmp);

    val = hpet_read(hpet, TIMER_N_CFG_CAP_REG(idx));

    cmp->idx               = idx;
    cmp->parent            = hpet;
    cmp->int_route_cap     = TN_GET_INT_ROUTE_CAP(val);
    cmp->supports_periodic = TN_GET_PER_INT_CAP(val);
    cmp->supports_fsb_del  = TN_GET_FSB_INT_DEL_CAP(val);
    cmp->size              = TN_GET_SIZE_CAP(val);
    
    /* all of our comparators will be in 64-bit mode */
    cmp->timer_32mode      = 0;

    cmp->stat     = TIMER_DISABLED;
    cmp->int_type = TIMER_EDGE; 

    /* make sure it's not running */
    hpet_cmp_stop(cmp);

    /* we don't need to actually write the cfg register
     * at this point since we're using default (0) values
     */

    /* NOTE that we're not setting up legacy replacement
     * routing */
    
    return 0;
}

static int
parse_hpet_tbl (struct acpi_table_header * hdr, void * arg)
{
    struct hpet_dev ** hpet_ptr = (struct hpet_dev **)arg;
    struct acpi_table_hpet * hpet_tbl = (struct acpi_table_hpet*)hdr;
    struct hpet_dev * hpet = NULL;
    uint64_t cap;
    unsigned i;

    HPET_PRINT("Initializing HPET Device\n", (void*)hpet_tbl);
    HPET_DEBUG("\tID: 0x%x\n", hpet_tbl->id);
    HPET_DEBUG("\tBase Address: %p\n", (void*)hpet_tbl->address.address);
    HPET_DEBUG("\t\tAccess Width: %u\n", hpet_tbl->address.access_width);
    HPET_DEBUG("\t\tBit Width:    %u\n", hpet_tbl->address.bit_width);
    HPET_DEBUG("\t\tBit Offset:   %u\n", hpet_tbl->address.bit_offset);
    HPET_DEBUG("\t\tAspace ID:  0x%x\n", hpet_tbl->address.space_id);
    HPET_DEBUG("\tSeq Num: 0x%x\n", hpet_tbl->sequence);
    HPET_DEBUG("\tMinimum Tick Rate %u\n", hpet_tbl->minimum_tick);
    HPET_DEBUG("\tFlags: 0x%x\n", hpet_tbl->flags);

    /* first map in the page */
    nk_map_page_nocache(ROUND_DOWN_TO_PAGE(hpet_tbl->address.address), PTE_PRESENT_BIT|PTE_WRITABLE_BIT, PS_4K);

    /* get the number of comparators */
    cap = *((volatile uint64_t*)(hpet_tbl->address.address + HPET_GEN_CAP_ID_REG));

    hpet = malloc(sizeof(struct hpet_dev) + sizeof(struct hpet_comparator)*GEN_CAP_GET_NUM_TIMERS(cap));
    if (!hpet) {
        ERROR_PRINT("Could not allocate HPET device\n");
        return -1;
    }
    memset(hpet, 0, sizeof(struct hpet_dev) + sizeof(struct hpet_comparator)*GEN_CAP_GET_NUM_TIMERS(cap));

    hpet->id       = hpet_tbl->id;
    hpet->min_tick = hpet_tbl->minimum_tick;
    hpet->flags    = hpet_tbl->flags;
    hpet->seq      = hpet_tbl->sequence;
    
    hpet->bit_width    = hpet_tbl->address.bit_width;
    hpet->bit_offset   = hpet_tbl->address.bit_offset;
    hpet->space_id     = hpet_tbl->address.space_id;
    hpet->access_width = hpet_tbl->address.access_width;
    hpet->base_addr    = hpet_tbl->address.address;

    unsigned long freq = FEMTOS_PER_SEC;
    unsigned int period = GEN_CAP_GET_CLK_PER(cap);
    hpet->freq     = freq/period;
    hpet->nanos_per_tick = period / 1000000;
    hpet->vendor   = GEN_CAP_GET_VENDOR_ID(cap);
    hpet->num_cmps = GEN_CAP_GET_NUM_TIMERS(cap);
    hpet->rev_id   = GEN_CAP_GET_REV_ID(cap);

    HPET_DEBUG("\tFrequency: %lu.%luMHz\n", hpet->freq / 1000000, hpet->freq % 1000000);
    HPET_DEBUG("\tVendor ID: 0x%x\n", hpet->vendor);
    HPET_DEBUG("\tRevision ID: 0x%x\n", hpet->rev_id);
    HPET_DEBUG("\tComparator Count: %u\n", hpet->num_cmps);

    for (i = 0; i < hpet->num_cmps; i++) {
        hpet_init_comparator(hpet, &(hpet->cmps[i]), i);
    }

    *hpet_ptr = hpet;

    return 0;
}

static struct nk_dev_int ops = {
    .open=0,
    .close=0,
};

int 
nk_hpet_init (void)
{
    struct hpet_dev * hpet = NULL;

    if (acpi_table_parse(ACPI_SIG_HPET, parse_hpet_tbl, &hpet)) {
        printk("No HPET device found\n");
        return 0;
    }

    nk_get_nautilus_info()->sys.hpet = hpet;

    // go ahead and start the counter
    hpet_cntr_run(hpet);

    nk_dev_register("hpet",NK_DEV_TIMER,0,&ops,0);

    return 0;
}





