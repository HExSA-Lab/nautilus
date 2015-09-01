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
#include <nautilus/msr.h>
#include <nautilus/nautilus.h>
#include <nautilus/cpu.h>
#include <nautilus/pmc.h>
#include <nautilus/mm.h>


#define PMC_DEBUG(fmt, args...) DEBUG_PRINT("PMC: " fmt, ##args)
#define PMC_INFO(fmt, args...)  printk("PMC: " fmt, ##args)
#define PMC_ERR(fmt, args...)   ERROR_PRINT("PMC: " fmt, ##args)
#define PMC_WARN(fmt, args...)  WARN_PRINT("PMC: " fmt, ##args)

static perf_slot_t  pmc_slots[NUM_PERF_SLOTS];

static event_prop_t event_props[256] = 
{ 
    [AMD_PMC_DCACHE_MISS]  = {"Data Cache Misses",              0x3f},
    [AMD_PMC_ICACHE_MISS]  = {"Instruction Cache Misses",       0x07},
    [AMD_PMC_L2_MISS]      = {"L2 Cache Misses",                0x07},
    [AMD_PMC_TLB_MISS]     = {"Unified TLB Misses",             0x07},
    [AMD_PMC_SMI_CNT]      = {"SMI Interrupts",                 0x3f},
    [AMD_PMC_IFETCH_STALL] = {"Instruction Fetch Stalls",       0x07},
    [AMD_PMC_BRANCH_MISS]  = {"Mispredicted Branches Retired",  0x3f},
    [AMD_PMC_ICACHE_INV]   = {"Instr. Cache Lines Invalidated", 0x07},
};



static inline uint64_t
read_pmc_ctl (uint8_t idx)
{
    if (idx >= NUM_PERF_SLOTS) {
        WARN_PRINT("Attempt to access invalid PMC CTL slot (%u)\n", idx);
        return 0;
    }

    return msr_read(PERF_CTL_MSR_N(idx));
}

static inline void
write_pmc_ctl (uint8_t idx, uint64_t val)
{
    if (idx >= NUM_PERF_SLOTS) {
        WARN_PRINT("Attempt to access invalid PMC CTL slot (%u)\n", idx);
        return;
    }

    msr_write(PERF_CTL_MSR_N(idx), val);
}


static inline uint64_t
read_pmc_ctr (uint8_t idx)
{ 
    if (idx >= NUM_PERF_SLOTS) {
        PMC_WARN("Attempt to access invalid PMC CTR slot (%u)\n", idx);
        return 0;
    }

    return msr_read(PERF_CTR_MSR_N(idx));

}


static inline void
write_pmc_ctr (uint8_t idx, uint64_t val)
{ 
    if (idx >= NUM_PERF_SLOTS) {
        PMC_WARN("Attempt to access invalid PMC CTR slot (%u)\n", idx);
        return;
    }

    msr_write(PERF_CTR_MSR_N(idx), val);
}


perf_event_t * 
assign_perf_event (uint8_t event_id, uint8_t unit_mask)
{
    uint8_t i;
    perf_event_t * event = NULL;
    perf_slot_t * slot = NULL;

    for (i = 0; i < NUM_PERF_SLOTS; i++) {

        slot = &pmc_slots[i];

        if (slot->status == PMC_SLOT_FREE && 
            event_props[event_id].slot_mask & (1<<i)) {

            pmc_ctl_t ctl;

            PMC_INFO("Assigning PMC slot %u to performance event %x (%s) with unit mask 0x%x\n", 
                    i, 
                    event_id,
                    event_props[event_id].name,
                    unit_mask);

            ctl.val           = 0;
            ctl.event_select0 = (uint8_t)event_id;
            ctl.usr           = 1;
            ctl.os            = 1;
            ctl.unit_mask     = unit_mask;

            slot->status = PMC_SLOT_USED;

            write_pmc_ctl(i, ctl.val);
            /* clear it just in case */
            write_pmc_ctr(i, 0);

            break;
        }
    }

    if (i == NUM_PERF_SLOTS) {
        PMC_ERR("Could not assign event 0x%x, no empty slots\n", event);
        return NULL;
    }

    event = malloc(sizeof(perf_event_t));
    if (!event) {
        PMC_ERR("Could not allocate new perf event\n");
        return NULL;
    }

    event->id           = event_id;
    event->assigned_idx = i;
    event->enabled      = 0;
    event->prop         = &event_props[event_id];

    slot->event = event;

    return event;
}


void
enable_perf_event (perf_event_t * event)
{
    pmc_ctl_t ctl;
    ASSERT(event);

    if (event->enabled) {
        PMC_WARN("Event 0x%x is already enabled\n", event->id);
        return;
    }

    uint8_t idx = event->assigned_idx;

    event->enabled = 1;

    ctl.val = read_pmc_ctl(idx);
    ctl.en = 1;

    write_pmc_ctl(idx, ctl.val);
}


void 
disable_perf_event (perf_event_t * event)
{
    pmc_ctl_t ctl;
    ASSERT(event);

    if (event->enabled == 0) {
        PMC_WARN("Event 0x%x is already disabled\n", event->id);
        return;
    }

    uint8_t idx = event->assigned_idx;

    event->enabled = 0;

    ctl.val = read_pmc_ctl(idx);
    ctl.en = 0;

    write_pmc_ctl(idx, ctl.val);
}


void
enable_all_events (void)
{
    unsigned i;
    for (i = 0; i < NUM_PERF_SLOTS; i++) {
        perf_slot_t * slot = &pmc_slots[i];
        if (slot->status == PMC_SLOT_USED) {
            enable_perf_event(slot->event);
        }
    }
}


void
disable_all_events (void)
{
    unsigned i;
    for (i = 0; i < NUM_PERF_SLOTS; i++) {
        perf_slot_t * slot = &pmc_slots[i];
        if (slot->status == PMC_SLOT_USED) {
            disable_perf_event(slot->event);
        }
    }
}


void
reset_all_counters (void)
{
    unsigned i;
    for (i = 0; i < NUM_PERF_SLOTS; i++) {
        write_pmc_ctr(i, 0);
    }
}


void
release_perf_event (perf_event_t * event)
{
    ASSERT(event);

    perf_slot_t * slot = NULL;
    uint8_t idx = event->assigned_idx;

    if (idx >= NUM_PERF_SLOTS) {
        PMC_ERR("Cannot release PMC slot for invalid index %u\n", idx);
        return;
    }

    slot = &pmc_slots[idx];

    if (slot->status == PMC_SLOT_USED) {
        slot->status = PMC_SLOT_FREE;
        slot->event  = NULL;
        write_pmc_ctl(idx, 0);
        write_pmc_ctr(idx, 0);
    }

    free(event);
}


uint64_t 
read_event_count (perf_event_t * event)
{
    ASSERT(event);

    uint8_t idx = event->assigned_idx;

    return read_pmc_ctr(idx);
}


void
perf_report (void)
{
    unsigned i;
    perf_event_t * event = NULL;

    PMC_INFO("++++++++ Perf Monitor Event Report ++++++++\n");

    for (i = 0; i < NUM_PERF_SLOTS; i++) {

        perf_slot_t * slot = &pmc_slots[i];

        if (slot->status == PMC_SLOT_FREE) {
            continue;
        }

        event = slot->event;

        ASSERT(event);

        PMC_INFO("SLOT [%u] (%s) %llu\n", 
                i,
                event->prop->name,
                read_event_count(event));

    }

    PMC_INFO("++++++++ Perf Monitor Report End ++++++++\n");

}

int 
pmc_init (void)
{

    /* TODO: make sure we actually have these counters, get the number of them from the index */

    memset(pmc_slots, 0, sizeof(pmc_slots));

#if 0
    assign_perf_event(AMD_PMC_TLB_MISS, 0x77);
    assign_perf_event(AMD_PMC_ICACHE_MISS, 0x0);
    assign_perf_event(AMD_PMC_DCACHE_MISS, 0x1);
    assign_perf_event(AMD_PMC_BRANCH_MISS, 0);
    assign_perf_event(AMD_PMC_ICACHE_INV, 0);
#endif

    return 0;
}


