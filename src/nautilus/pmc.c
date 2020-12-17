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
 * Copyright (c) 2020, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/msr.h>
#include <nautilus/nautilus.h>
#include <nautilus/cpu.h>
#include <nautilus/cpuid.h>
#include <nautilus/pmc.h>
#include <nautilus/mm.h>
#include <nautilus/shell.h>


/*
 * PMC Subsystem
 * =============
 *
 * Currently, counters are represented as events, which have both generic and
 * platform-specific feature attributes. The usage model is as follows:
 *
 * (1) counter = nk_pmc_create(id) 
 *
 * where id is the *software* ID of the event, specific to Nautilus. In order to
 * use new hardware events, you must define a new row in the event_attr struct
 * for the specific platform of concern. SW IDs are tied to HW counter
 * representations based on their numerical order.
 *
 * While HW slots are distinct from SW slots (due to the fact that different
 * platforms have a different number of counter registers), this call also has
 * the effect of binding a SW slot to a HW slot, since we currently assume that
 * HW and SW slots directly correspond. 
 *
 * (2) nk_pmc_start(counter);
 *
 * This will start the counter by changing its state and writing the appropriate
 * 'enable' bit in the counter control reg.
 * 
 * (3) count = nk_pmc_read(counter);
 *
 * This call retrieves the current counter value. If the counter is enabled, it
 * will get the most recent value from the hardware reg. If not, it will get the
 * value from a "shadow" copy corresponding to the most recently read value.
 *
 * (4) nk_pmc_stop(counter);
 *
 * Disables the hardware counter.
 *
 * (5) nk_pmc_destroy(counter);
 *
 * Destroys the SW counter and frees up any HW state it occupied.
 *
 * Worth noting:
 *
 * From AMD manual: "The performance counters are not guaranteed to produce identical
 * measurements each time they are used to measure a particular instruction
 * sequence, and they should not be used to take measurements of very small
 * instruction sequences. The RDPMC instruction is not serializing, and it can be
 * executed out-of-order with respect to other instructions around it. Even when
 * bound by serializing instructions, the system environment at the time the
 * instruction is executed can cause events to be counted before the counter value
 * is loaded into EDX:EAX."
 *
 */

#define PMC_DEBUG(fmt, args...) DEBUG_PRINT("PMC: " fmt, ##args)
#define PMC_INFO(fmt, args...)  nk_vc_printf("PMC: " fmt, ##args)
#define PMC_ERR(fmt, args...)   ERROR_PRINT("PMC: " fmt, ##args)
#define PMC_WARN(fmt, args...)  WARN_PRINT("PMC: " fmt, ##args)

#ifndef NAUT_CONFIG_DEBUG_PMC
#undef PMC_DEBUG
#define PMC_DEBUG(fmt, args...)
#endif


/*
 * These are the AMD-specific PMC event attributes. If you add new counters,
 * they should go here. The ordering gives the software ID, and the hardware ID
 * is in column 1. Unit mask is column 2, and slot mask is column 3. Note that
 * the slot mask is AMD-specific, and it determines which of the HW PMC regs the
 * specific counter can be installed in.
 *
 * Note that these can vary by uarch. These were taken from AMD's BIOS/Kern
 * Dev Guide (Rev 3.14) for Family 15h model 00h-0fh processors. Check the
 * BIOS kern guide for specific type if in doubt. 
 * TODO: we should automate this discovery with a CPU fam/model to event ID
 * mapping table...
 *
 */
static amd_event_attr_t amd_event_attrs[256] = 
{ 
    {"Data Cache Accesses",              0x040, 0x00, 0x3f}, // includes speculative accessses
    {"Data Cache Misses",                0x041, 0x03, 0x3f}, // includes speculative accesses. accesses during pending refills don't count
    {"dCache Refills from L2 or System", 0x042, 0x01, 0x3f}, // includes only "fill with good data" in unit mask
    {"L1 Refills from System",           0x043, 0x00, 0x07}, 
    {"Instruction Cache Misses",         0x081, 0x00, 0x07}, // # of ifetches and prefetch reqs that miss in i$
    {"iCache Refills from L2",           0x082, 0x00, 0x07}, // # of i$ refills satisfied from L2
    {"iCache Refills from System",       0x083, 0x00, 0x07}, // # of i$ refills from system memory (or another $)
    {"Requests to L2 Cache",             0x07d, 0x5f, 0x07}, // Reqs. to L2 for i$ fills, d$ fills, pt lookups for tlb
    {"L2 Cache Misses",                  0x07e, 0x27, 0x07}, // reqs that miss in L2. (L2$ prefetch reqs, TLB pt walks, dc fill, ic fill)
    {"Unified TLB Hits",                 0x045, 0x33, 0x07}, // includes all page sizes (1G, 2M, 4K). Includes spec.
    {"Unified TLB Misses",               0x046, 0x33, 0x07}, // all page sizes. includes spec.
    {"L1 ITLB Miss, L2 ITLB Hit",        0x084, 0x00, 0x07}, // # ifetches that miss in L1 iTLB but hit in L2 TLB
    {"L1 ITLB Miss, L2 ITLB Miss",       0x085, 0x07, 0x07}, // # ifetches that miss in both L1 and L2 TLBs (all page sizes)
    {"Page Splinters",                   0x165, 0x03, 0x07}, //  "# of TLB reloads where a large page is installed into TLB as a smaller page size"
    {"SMI Interrupts Received",          0x02b, 0x00, 0x3f}, 
    {"Instruction Fetch Stalls",         0x087, 0x00, 0x07},
    {"Mispredicted Branches Retired",    0x0c3, 0x00, 0x3f},
    {"Instr. Cache Lines Invalidated",   0x08c, 0x00, 0x07},
    {"Load/Store Dispatch (Just loads)", 0x029, 0x01, 0x3f},
    {"Load/Store Dispatch (Just stores)",0x029, 0x02, 0x3f},
    {"Load/Store Dispatch (Everything)", 0x029, 0x07, 0x3f},
};


/*
 * These are the Intel-specific PMC event attributes.  More or less the same as
 * the AMD attrs, except instead of a slot mask, we have an extra attr which
 * tells us which bit in the PMC CPUID leaf will tell us wheter or not the HW
 * supports this counter.
 *
 */
static intel_event_attr_t intel_event_attrs[256] = 
{
	{"Unhalted Core Cycles",        0x3c, 0x00, 0},
	{"Instructions Retired",        0xc0, 0x00, 1},
	{"Unhalted Reference Cycles",   0x3c, 0x01, 2},
	{"LLC References",              0x2e, 0x4f, 3},
	{"LLC Misses",                  0x2e, 0x41, 4},
	{"Branch Instructions Retired", 0xc4, 0x00, 5},
	{"Branch Misses Retired",       0xc5, 0x00, 6},
    /* begin impl specific */
    {"dTLB Load Misses -> Walk",    0x08, 0x01, 0},
};
	

static inline uint32_t
intel_get_pmc_leaf (void)
{
    cpuid_ret_t ret;
    cpuid(IA32_PMC_LEAF, &ret);
    return ret.a;
}


static inline uint32_t
amd_get_pmc_leaf (void)
{
    cpuid_ret_t ret;
    cpuid(AMD_PMC_LEAF, &ret);
    return ret.c;
}


static inline uint8_t
amd_has_ext_counters (void)
{
    return !!(amd_get_pmc_leaf() & (1 << 23));
}


static inline uint8_t
amd_has_nb_counters (void)
{
    return !!(amd_get_pmc_leaf() & (1 << 24));
}


// KCH TODO: I can't tell if it's a documentaiton bug or not,
// but AMD's BIOS Kern guide says bit 28 *value 0* indicates support
// for the L2I MSRs. That seems bizarre since all the rest are 1s...
// Might be an issue on newer AMDs
static inline uint8_t
amd_has_l2i_counters (void)
{
    return !!(amd_get_pmc_leaf() & (1 << 28));
}


static int
intel_get_pmc_version (struct pmc_info * pmc)
{
    if (cpuid_leaf_max() >= IA32_PMC_LEAF) {
        return intel_get_pmc_leaf() & 0xff;
    } 

    return 0;
}


static int
intel_get_pmc_msr_count (struct pmc_info * pmc)
{
    return (intel_get_pmc_leaf() >> 8) & 0xff;
}


static int
intel_get_pmc_msr_bitwidth (struct pmc_info * pmc)
{
    return (intel_get_pmc_leaf() >> 16) & 0xff;
}


static inline int
pmc_assign_slot (pmc_info_t * pmc, 
                 perf_event_t * event, 
                 int slot)
{
    PMC_DEBUG("Assigning event (%s) to slot %d\n", event->name, slot);

    if (pmc->ops->bind_ctr(pmc, event, slot) != 0) {
        PMC_DEBUG("Could not bind counter to slot\n");
        return -1;
    }

    pmc->slots[slot].status = PMC_SLOT_USED;
    pmc->slots[slot].event  = event;

    event->assigned_idx = slot;
    event->bound        = 1;

    return 0;
}


/*
 * Bind a perf event to a software slot. -1 for
 * slot number means we can pick any available slot.
 *
 */
static int
pmc_bind (perf_event_t * event, int slot)
{
    pmc_info_t * pmc = nk_get_nautilus_info()->sys.pmc_info;
    if (!event) {
        PMC_ERR("Bad event given in %s\n", __func__);
        return -1;
    }

    if (event->bound) {
        PMC_ERR("Attempt to bind event which is already bound (to slot %d)\n",
                event->assigned_idx);
        return -1;
    }

    ASSERT(slot < pmc->sw_num_slots);

    /* any free slot is fine */
    if (slot == -1) {
        int i;
        for (i = 0; i < pmc->sw_num_slots; i++) {

            /* assign slot can fail, particulary if 
             * binding the event to the register doesn't work
             * (e.g. due to register constraints that AMD has)
             */
            if (pmc->slots[i].status == PMC_SLOT_FREE && 
                pmc_assign_slot(pmc, event, i) == 0) {
                return 0;
            }
        }

        if (i == pmc->sw_num_slots) {
            PMC_DEBUG("Could not bind event, no suitable slots free\n");
            return -1;
        }

    }

    if (pmc->slots[slot].status != PMC_SLOT_FREE) {
        PMC_ERR("Could not assign event to slot %d, slot taken\n", slot);
        return -1;
    }

    if (pmc_assign_slot(pmc, event, slot) != 0) {
        PMC_WARN("Could not assign PMC id (0x%02x) to slot %d\n",
                event->sw_id,
                slot);
        return -1;
    }

    return 0;
}


static int
pmc_unbind (perf_event_t * event)
{
    pmc_info_t * pmc = nk_get_nautilus_info()->sys.pmc_info;

    PMC_DEBUG("Unbinding event (0x%02x) from slot %d\n", 
            event->sw_id,
            event->assigned_idx);

    if (!event) {
        PMC_ERR("Bad event given in %s\n", __func__);
        return -1;
    }

    if (event->bound == 0) {
        PMC_WARN("Attempt to unbind event which is already unbound\n");
        return -1;
    }

    ASSERT(event->assigned_idx < pmc->sw_num_slots);

    pmc->slots[event->assigned_idx].status = PMC_SLOT_FREE;
    pmc->slots[event->assigned_idx].event  = NULL;

    pmc->ops->unbind_ctr(pmc, event->assigned_idx);

    event->assigned_idx = 0;
    event->bound        = 0;

    return 0;
}


void
nk_pmc_report (void)
{
    unsigned i;
    pmc_info_t * pmc = nk_get_nautilus_info()->sys.pmc_info;
    perf_event_t * event = NULL;

    if (!pmc) {
        PMC_WARN("Cannot create report, PMC subsystem disabled\n");
        return;
    }

    PMC_INFO("++++++++ Perf Monitor Event Report ++++++++\n");

    for (i = 0; i < pmc->sw_num_slots; i++) {

        perf_slot_t * slot = &pmc->slots[i];

        if (slot->status == PMC_SLOT_FREE) {
            continue;
        }

        event = slot->event;

        ASSERT(event);

        PMC_INFO("SLOT [%u] (%s) %llu\n", 
                i,
                event->name,
                nk_pmc_read(event));

    }

    PMC_INFO("++++++++ Perf Monitor Report End ++++++++\n");

}


static void
intel_flags_init (pmc_info_t * pmc)
{
	cpuid_ret_t ret;
	int i;

    if (intel_get_pmc_version(pmc) <  1) {
        pmc->valid = 0;
        PMC_WARN("Insufficient Intel PMC version to support perf counter subsystem\n");
        return;
    } else {
        cpuid(IA32_PMC_BASE, &ret);
        pmc->intel_fl = ret.b;
        pmc->valid = 1;
    }

	PMC_DEBUG("Intel Arch PMC Events Supported:\n");

	for (i = 0; i < INTEL_NUM_ARCH_EVENTS; i++) {
		if ((pmc->intel_fl & (1 << intel_event_attrs[i].bit_pos)) == 0) {
			PMC_DEBUG("  [%s]\n", intel_event_attrs[i].name);
		}
	}

    PMC_DEBUG("Intel attribute flags set to 0x%08x\n", pmc->intel_fl);

}


static void
amd_flags_init (pmc_info_t * pmc)
{
    // We have 4 perf ctr. slots on all implementations (AMD Arch Programmers Ref Man. 13.2.1)
    // Extended support (2 new regs, six total) depends on CPUID 0x8000_0001[ecx].23
    if (cpuid_ext_leaf_max() >= 1 && amd_has_ext_counters()) {
        pmc->amd_fl |= AMD_EXT_CNT_FLAG;
        PMC_DEBUG("Using AMD Extended performance counters (6 slots)\n");
    } else {
        PMC_DEBUG("Using AMD Legacy performance counters (4 sltos)\n");
    }

    if (amd_has_nb_counters()) {
        pmc->amd_fl |= AMD_NB_CNT_FLAG;
        PMC_DEBUG("AMD Northbridge PMC counter regs available\n");
    }

    if (amd_has_l2i_counters()) {
        pmc->amd_fl |= AMD_L2I_CNT_FLAG;
        PMC_DEBUG("AMD L2I PMC counter regs available\n");
    }

    PMC_DEBUG("AMD attribute flags set to 0x%08x\n", pmc->amd_fl);

    pmc->valid = 1;
}


static uint64_t
intel_read_ctl (struct pmc_info * pmc, uint8_t idx)
{
    return msr_read(INTEL_PERF_CTL_MSR_N(idx));
}


static void
intel_write_ctl (struct pmc_info * pmc, uint8_t idx, uint64_t val)
{
    PMC_DEBUG("Writing 0x%016x to Intel PMC Ctrl reg %d\n", val, idx);
    msr_write(INTEL_PERF_CTL_MSR_N(idx), val);
}


static uint64_t
intel_read_ctr (struct pmc_info * pmc, uint8_t idx)
{
    return msr_read(INTEL_PERF_CTR_MSR_N(idx));
}


static void
intel_write_ctr (struct pmc_info * pmc, uint8_t idx, uint64_t val)
{
    PMC_DEBUG("Writing 0x%016x to Intel HW slot %d\n");
    msr_write(INTEL_PERF_CTR_MSR_N(idx), val);
}


static void
intel_event_init (struct pmc_info * pmc, perf_event_t * event)
{
    PMC_DEBUG("Initiliazing Intel event state (sw_id=0x%02x)\n", event->sw_id);
    // TODO: check to make sure this is a valid event!
    event->attrs.intel_attrs = &intel_event_attrs[event->sw_id];
    event->name = event->attrs.intel_attrs->name;
}


static int
intel_bind_ctr (struct pmc_info * pmc, perf_event_t * event, int slot)
{
    pmc_ctl_intel_t ctl;

    PMC_DEBUG("Binding Intel HW slot %d to [%s]\n", 
            slot,
            event->name);

    ctl.val = 0;

    ctl.usr = 1;
    ctl.os  = 1;
    ctl.event_select = event->attrs.intel_attrs->id;
    ctl.unit_mask    = event->attrs.intel_attrs->umask;
    
    intel_write_ctl(pmc, slot, ctl.val);

    return 0;
}


static void
intel_unbind_ctr (struct pmc_info * pmc, int slot)
{
    PMC_DEBUG("Unbinding Intel HW slot %d\n", slot);
    intel_write_ctl(pmc, slot, 0);
}


static void
intel_enable_ctr (struct pmc_info * pmc, perf_event_t * event)
{
    pmc_ctl_intel_t ctl;

    PMC_DEBUG("Enabling Intel HW counter %d\n", event->assigned_idx);

    ctl.val = intel_read_ctl(pmc, event->assigned_idx);

    ctl.en = 1;

    intel_write_ctl(pmc, event->assigned_idx, ctl.val);
}


static void
intel_disable_ctr (struct pmc_info * pmc, perf_event_t * event)
{
    pmc_ctl_intel_t ctl;

    PMC_DEBUG("Disabling Intel HW counter %d\n", event->assigned_idx);

    ctl.val = intel_read_ctl(pmc, event->assigned_idx);

    ctl.en = 0;

    intel_write_ctl(pmc, event->assigned_idx, ctl.val);
}


static int
intel_pmc_init (pmc_info_t * pmc)
{
    PMC_DEBUG("Initializing Intel-specific PMC state\n");

	intel_flags_init(pmc);

    return 0;
}


static int
amd_get_pmc_version (struct pmc_info *pmc)
{
    // AMD does not appear to have version numbering
    return 0;
}


static inline int
amd_get_pmc_msr_count (struct pmc_info * pmc)
{
    if (pmc->amd_fl & AMD_EXT_CNT_FLAG)
        return AMD_PERF_SLOTS_EXT;

    return AMD_PERF_SLOTS_LEGACY;
}


static int
amd_get_pmc_msr_bitwidth (struct pmc_info * pmc)
{
    // KCH: here we actually report the maximum supported width. 
    // actual width is implementation-dependent, but it's
    // not clear from the AMD mans how to query the *actual*
    // bit-width...
    return 64;
}


static int
amd_pmc_init (pmc_info_t * pmc)
{
    PMC_DEBUG("Initializing AMD-specific PMC state\n");

    amd_flags_init(pmc);

    return 0;
}


static inline uint8_t
amd_has_ext (struct pmc_info * pmc)
{
    return pmc->amd_fl & AMD_EXT_CNT_FLAG;
}


static inline uint64_t
amd_read_ctr (struct pmc_info * pmc, uint8_t idx)
{
    uint64_t ret = amd_has_ext(pmc) ? 
                   msr_read(AMD_PERF_CTR_MSR_N(idx)) :
                   msr_read(AMD_PERF_CTR_MSR_N_LEGACY(idx));
    return ret;
}


static inline void
amd_write_ctr (struct pmc_info * pmc, uint8_t idx, uint64_t val)
{
    PMC_DEBUG("Writing 0x%016x to AMD HW slot %d\n");
    if (amd_has_ext(pmc))
        msr_write(AMD_PERF_CTR_MSR_N(idx), val);
    else 
        msr_write(AMD_PERF_CTR_MSR_N_LEGACY(idx), val);
}


static inline uint64_t
amd_read_ctl (struct pmc_info * pmc, uint8_t idx)
{
    uint64_t ret = amd_has_ext(pmc) ? 
                   msr_read(AMD_PERF_CTL_MSR_N(idx)) :
                   msr_read(AMD_PERF_CTL_MSR_N_LEGACY(idx));
    return ret;
}


static inline void
amd_write_ctl (struct pmc_info * pmc, uint8_t idx, uint64_t val)
{
    PMC_DEBUG("Writing 0x%016x to AMD  PMC Ctrl reg %d\n", val, idx);
    if (amd_has_ext(pmc))
        msr_write(AMD_PERF_CTL_MSR_N(idx), val);
    else 
        msr_write(AMD_PERF_CTL_MSR_N_LEGACY(idx), val);
}


static void
amd_event_init (struct pmc_info * pmc, perf_event_t * event)
{
    PMC_DEBUG("Initiliazing AMD event state (sw_id=0x%02x)\n", event->sw_id);
    event->attrs.amd_attrs = &amd_event_attrs[event->sw_id];
    event->name            = event->attrs.amd_attrs->name;
}


static int
amd_bind_ctr (struct pmc_info * pmc, perf_event_t * event, int slot)
{
    pmc_ctl_amd_t ctl;

    PMC_DEBUG("Binding AMD HW slot %d to [%s]\n", 
            slot,
            event->name);

    // If we don't have extended counter support, we only have 4 slots, so
    // slot #s > 3 are not supported
    if (!(pmc->amd_fl & AMD_EXT_CNT_FLAG) && slot >= AMD_PERF_SLOTS_LEGACY) {
        PMC_WARN("Attempt to bind to slot %d when there are only %d available slots\n",
                slot, AMD_PERF_SLOTS_LEGACY);
        return -1;
    }

    if (!(event->attrs.amd_attrs->slot_mask & (1 << slot))) {
            PMC_DEBUG("This event type cannot bind to slot %d (constraint=0x%02x)\n", 
                    slot,
                    event->attrs.amd_attrs->slot_mask);
            return -1;
    }

    ctl.val = 0;
    ctl.usr = 1;
    ctl.os  = 1;
    ctl.event_select0 = event->attrs.amd_attrs->id;
    ctl.unit_mask     = event->attrs.amd_attrs->umask;
    
    amd_write_ctl(pmc, slot, ctl.val);

    return 0;
}


static void
amd_unbind_ctr (struct pmc_info * pmc, int slot)
{
    PMC_DEBUG("Unbinding AMD HW slot %d\n", slot);
    amd_write_ctl(pmc, slot, 0);
}


static void
amd_enable_ctr (struct pmc_info * pmc, perf_event_t * event)
{
    pmc_ctl_amd_t ctl;

    PMC_DEBUG("Enabling AMD HW slot %d (%s)\n", 
            event->assigned_idx, 
            event->name);

    ctl.val = amd_read_ctl(pmc, event->assigned_idx);
    ctl.en = 1;
    amd_write_ctl(pmc, event->assigned_idx, ctl.val);
}


static void
amd_disable_ctr (struct pmc_info * pmc, perf_event_t * event)
{
    pmc_ctl_amd_t ctl;

    PMC_DEBUG("Disabling AMD HW slot %d (%s)\n", 
            event->assigned_idx,
            event->name);

    ctl.val = amd_read_ctl(pmc, event->assigned_idx);
    ctl.en = 0;
    amd_write_ctl(pmc, event->assigned_idx, ctl.val);
}


static struct pmc_ops amd_ops = {
	.init        = amd_pmc_init,
	.read_ctr    = amd_read_ctr,
	.write_ctr   = amd_write_ctr,
    .event_init  = amd_event_init,
    .bind_ctr    = amd_bind_ctr,
    .unbind_ctr  = amd_unbind_ctr,
    .enable_ctr  = amd_enable_ctr,
    .disable_ctr = amd_disable_ctr,
    .version     = amd_get_pmc_version,
    .msr_cnt     = amd_get_pmc_msr_count,
    .msr_width   = amd_get_pmc_msr_bitwidth,
};

static struct pmc_ops intel_ops = {
	.init        = intel_pmc_init,
	.read_ctr    = intel_read_ctr,
	.write_ctr   = intel_write_ctr,
    .event_init  = intel_event_init,
    .bind_ctr    = intel_bind_ctr,
    .unbind_ctr  = intel_unbind_ctr,
    .enable_ctr  = intel_enable_ctr,
    .disable_ctr = intel_disable_ctr,
    .version     = intel_get_pmc_version,
    .msr_cnt     = intel_get_pmc_msr_count,
    .msr_width   = intel_get_pmc_msr_bitwidth,
};


static int
platform_pmc_init (pmc_info_t * pmc)
{
    if (nk_is_intel()) {
        pmc->ops = &intel_ops;
    } else if (nk_is_amd()) {
		pmc->ops = &amd_ops;
    } else {
        PMC_WARN("No PMC support for this platform\n");
		return 0;
    }

	return pmc->ops->init(pmc);
}


void
nk_pmc_start (perf_event_t * event)
{
    pmc_info_t * pmc = nk_get_nautilus_info()->sys.pmc_info;

    if (!event) {
        PMC_ERR("Bad event given\n");
        return;
    }

    if (event->enabled) {
        PMC_WARN("Event already enabled\n");
        return;
    }

    if (!event->bound) {
        PMC_ERR("Attempt to enable unbound event (id=0x%02x)\n", event->sw_id);
        return;
    }

    event->enabled = 1;

    pmc->ops->enable_ctr(pmc, event);
}


void 
nk_pmc_stop(perf_event_t * event)
{
    pmc_info_t * pmc = nk_get_nautilus_info()->sys.pmc_info;

    if (!event) {
        PMC_ERR("Bad event given\n");
        return;
    }

    if (!event->enabled) {
        PMC_WARN("Event already disabled\n");
        return;
    }

    if (!event->bound) {
        PMC_ERR("Attempt to disable unbound event (id=0x%02x)\n", event->sw_id);
        return;
    }

    event->enabled = 0;
    pmc->ops->disable_ctr(pmc, event);
}


uint64_t 
nk_pmc_read (perf_event_t * event)
{
    pmc_info_t * pmc = nk_get_nautilus_info()->sys.pmc_info;

    if (event->enabled) {
        event->val = pmc->ops->read_ctr(pmc, event->assigned_idx);
    }

    return event->val;
}


void
nk_pmc_write (perf_event_t * event, uint64_t val)
{
    pmc_info_t * pmc = nk_get_nautilus_info()->sys.pmc_info;

    event->val = val;

    // if it's not bound to a slot, we only update its shadow val

    if (event->bound) {
        pmc->ops->write_ctr(pmc, event->assigned_idx, val);
    } 
}


/*
 * Note that ID is Nautilus-specific and 
 * has different meaning on Intel and AMD!
 */
perf_event_t *
nk_pmc_create (uint32_t event_id)
               
{
    pmc_info_t * pmc = nk_get_nautilus_info()->sys.pmc_info;
    perf_event_t * event = NULL;

    if (!pmc->valid) {
        PMC_WARN("Cannot create perf counter, PMC system disabled\n");
        return NULL;
    }

    PMC_DEBUG("Creating event for SW event id 0x%02x\n", event_id);

    event = malloc(sizeof(perf_event_t));
    if (!event) {
        PMC_ERR("Could not allocate event\n");
        return NULL;
    }
    memset(event, 0, sizeof(perf_event_t));

    event->sw_id = event_id;

    pmc->ops->event_init(pmc, event);

    if (pmc_bind(event, -1) != 0) {
        PMC_ERR("Could not create event: unsuccessful bind\n");
        goto out_err;
    }

    return event;

out_err:
    free(event);
    return NULL;
}


void
nk_pmc_destroy (perf_event_t * event)
{
    if (!event) {
        PMC_WARN("Attempt to destroy bad event\n");
        return;
    }

    PMC_DEBUG("Destroying event (id=0x%02x)\n", event->sw_id);

    if (event->enabled) {
        nk_pmc_stop(event);
    }

    if (event->bound) {
        pmc_unbind(event);
    }

    free(event);
}


int 
nk_pmc_init (struct naut_info * naut)
{
    pmc_info_t * pmc = NULL;
    
    PMC_INFO("Initializing PMC subsystem\n");

    pmc = malloc(sizeof(pmc_info_t));
    if (!pmc) {
        PMC_ERR("Could not allocate PMC info\n");
        return -1;
    }
    memset(pmc, 0, sizeof(pmc_info_t));

    naut->sys.pmc_info = pmc;

    platform_pmc_init(pmc);

    if (!pmc->valid) {
        PMC_WARN("Disabling PMC subsystem\n");
        return 0;
    }

    pmc->version_id = pmc->ops->version(pmc);
    pmc->msr_cnt    = pmc->ops->msr_cnt(pmc);
    pmc->msr_width  = pmc->ops->msr_width(pmc);

    PMC_DEBUG("platform version (ignore for AMD): %d\n", pmc->version_id);
    PMC_DEBUG("MSR count: %d\n", pmc->msr_cnt);
    PMC_DEBUG("MSR bitwidth: %d\n", pmc->msr_width);

    pmc->hw_num_slots = pmc->msr_cnt;

    // TODO: these don't necessarily need to be the same
    // for now we assume SW slot ID == HW slot ID
    pmc->sw_num_slots = pmc->hw_num_slots;

	PMC_DEBUG("Using %d SW PMC slots -> %d HW slots\n", pmc->sw_num_slots, pmc->hw_num_slots);
	pmc->slots = malloc(sizeof(perf_slot_t)*pmc->sw_num_slots);
	if (!pmc->slots) {
		PMC_ERR("Could not allocate PMC slots\n");
		return -1;
	}
	memset(pmc->slots, 0, sizeof(perf_slot_t)*pmc->sw_num_slots);

    return 0;
}


#define NUM_EVENTS 4

static void
test_pmc (int pmc_id)
{
    //uint64_t start, stop;
    uint64_t start[NUM_EVENTS], stop[NUM_EVENTS];
    void * x = NULL;
    //perf_event_t * p = NULL;
    perf_event_t * p[NUM_EVENTS];

    uint16_t ids[NUM_EVENTS] = {3, 2, 1, 0};

    PMC_INFO("Testing perf counter subsystem (counter_id=0x%02x)\n", pmc_id);

    for (int i = 0; i < NUM_EVENTS; i++) {
        p[i] = nk_pmc_create(ids[i]);
        if (!p[i]) {
            PMC_ERR("Could not create counter for id 0x%02x\n", ids[i]);
            return;
        }
    }
    //p = nk_pmc_create(pmc_id);

    /*
    if (!p) {
        PMC_ERR("Could not create counter for id 0x%02x\n", pmc_id);
        return;
    }
    */


    for (int i = 0; i < NUM_EVENTS; i++) {
        PMC_INFO("  Counter name = [%s]\n", p[i]->name);
        nk_pmc_start(p[i]);
        start[i] = nk_pmc_read(p[i]);
    }

    //start = nk_pmc_read(p);

    x = malloc(PAGE_SIZE);
    memset(x, 0, PAGE_SIZE);
    free(x);

    for (int i = 0; i < NUM_EVENTS; i++) {
        stop[i] = nk_pmc_read(p[i]);
        nk_pmc_stop(p[i]);
        nk_pmc_destroy(p[i]);
        PMC_INFO("  Count reported: %lld\n", stop[i]-start[i]);
    }
}


static int
handle_pmc (char * buf, void * priv)
{
    int pmc_id;

    if (sscanf(buf, "pmctest %d", &pmc_id) == 1) {
        test_pmc(pmc_id);
        return 0;
    }

    nk_vc_printf("Unknown PMC test\n");

    return 0;
}


static struct shell_cmd_impl pmc_impl = {
    .cmd      = "pmctest",
    .help_str = "pmctest id",
    .handler  = handle_pmc,
};
nk_register_shell_cmd(pmc_impl);
