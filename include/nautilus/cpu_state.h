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
 * Copyright (c) 2017, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/* Wrappers for preemption, interrupt level, etc, for use when */
/* struct cpu isn't available  */

#ifndef __CPU_STATE
#define __CPU_STATE

#include <nautilus/msr.h>

//
// This code assumes that %gs base is pointing to
// the struct cpu of the running cpu and it assumes the
// specific layout of struct cpu
//


static inline void *__cpu_state_get_cpu()
{
    // there must be a smarter way to do this....
    // leaq %gs:... does not do it though
    return (void *) msr_read(MSR_GS_BASE);
}

static inline void preempt_disable() 
{
    void *base = __cpu_state_get_cpu();
    if (base) {
	// per-cpu functional
	__sync_fetch_and_add((uint16_t *)((uint64_t)base+10),1);
    } else {
	// per-cpu is not running, so we are not going to get preempted anyway
    }
}

static inline void preempt_enable() 
{
    void *base = __cpu_state_get_cpu();
    if (base) {
	// per-cpu functional
	__sync_fetch_and_sub((uint16_t *)((uint64_t)base+10),1);
    } else {
	// per-cpu is not running, so we are not going to get preempted anyway
    }
}

// make cpu preeemptible once again
// this should only be called by scheduler in handling
// special context switch requests
static inline void preempt_reset() 
{
    void *base = __cpu_state_get_cpu();
    if (base) {
	// per-cpu functional
	__sync_fetch_and_and((uint16_t *)((uint64_t)base+10),0);
    } else {
	// per-cpu is not running, so we are not going to get preempted anyway
    }
}

static inline int preempt_is_disabled()
{
    void *base = __cpu_state_get_cpu();
    if (base) {
	// per-cpu functional
	return __sync_fetch_and_add((uint16_t *)((uint64_t)base+10),0);
    } else {
	// per-cpu is not running, so we are not going to get preempted anyway
	return 1;
    }
}

static inline uint16_t interrupt_nesting_level()
{
    void *base = __cpu_state_get_cpu();
    if (base) {
	return __sync_fetch_and_add((uint16_t *)((uint64_t)base+8),0);
    } else {
	return 0; // no interrupt should be on if we don't have percpu
    }
    
}

static inline int in_interrupt_context()
{
    return interrupt_nesting_level()>0;
}

// Do not directly use these functions or sti/cli unless you know
// what you are doing...  
// Instead, use irq_disable_save and a matching irq_enable_restore
#define enable_irqs() sti()
#define disable_irqs() cli()

static inline uint8_t irqs_enabled (void)
{
    uint64_t rflags = read_rflags();
    return (rflags & RFLAGS_IF) != 0;
}

static inline uint8_t irq_disable_save (void)
{
    preempt_disable();

    uint8_t enabled = irqs_enabled();

    if (enabled) {
        disable_irqs();
    }

    preempt_enable();

    return enabled;
}
        

static inline void irq_enable_restore (uint8_t iflag)
{
    if (iflag) {
        /* Interrupts were originally enabled, so turn them back on */
        enable_irqs();
    }
}

#endif
