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
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, The Interweaving Project <http://interweaving.org>
 *                     The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Peter Dinda <pdinda@northwestern.edu>
 *          Michael Cuevas <cuevas@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <dev/i8254.h>
#include <dev/apic.h>
#include <dev/ioapic.h>


#ifndef NAUT_CONFIG_DEBUG_WATCHDOG
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("watchdog: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("watchdog: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("watchdog: " fmt, ##args)

#define CEIL_DIV(x,y)  (((x)/(y)) + !!((x)%(y)))

/* quantum is actually 54 ms, but we use 50 ms for simplicity */
#define I8254_QUANTUM_NS 50000000
static uint64_t bark_timeout_ns=0;
static uint64_t timeout_limit=0;
static uint8_t nk_watchdog_monitor_entry=0;

/*
  Currently, this is a thin wrapper on the i8254 PIT
  and the first IOAPIC.   Both of these devices are
  always included in the build.

  There are several assumptions:
     - there is at least one IOAPIC
     - the PIT's interrupt line is wired to pin 2
       of the first IOAPIC (this is common)

  And some limitations:
     - the NMI is delivered to CPU 0 only at this point
     - the PIT's longest interval is about 54 ms, which
       is then the limit on the watchdog.   In other words
       we must pet the watchdog at least that often.  
       If you compile NK with the default HZ=10, the 
       interval between interrupts could be as long as
       100 ms.    Thus you need to use a higher HZ
 */


void nk_watchdog_pet(void)
{
    //DEBUG("pet from %016lx\n",__builtin_return_address(0));
    
    if (!bark_timeout_ns) {
	return; // watchdog does not exist yet
    }
     
    if (i8254_set_oneshot(bark_timeout_ns)) {
	ERROR("Failed to set i8254 timer\n");
	return;
    }

    struct cpu *my_cpu = get_cpu();
    my_cpu->watchdog_count = 0;
}

int nk_watchdog_init(uint64_t timeout_ns)
{
    struct sys_info *sys = per_cpu_get(system);
    int num_cpus = sys->num_cpus;
    int i;
    for (i=0; i < num_cpus; i++)
    {
        sys->cpus[i]->watchdog_count = 0;
    }

    if (timeout_ns > I8254_QUANTUM_NS) {
        bark_timeout_ns = I8254_QUANTUM_NS;
        timeout_limit = CEIL_DIV(timeout_ns, I8254_QUANTUM_NS);
        i8254_set_oneshot(I8254_QUANTUM_NS); 
        INFO("timeout set to %lu ns\n", I8254_QUANTUM_NS * timeout_limit);
    } else {
        bark_timeout_ns = timeout_ns;
        timeout_limit = 1;
        i8254_set_oneshot(bark_timeout_ns);
        INFO("timeout set to %lu ns\n",bark_timeout_ns);
    }

    // we will assume that the i8254 is hooked to pin 2 of
    // the first ioapic, and we will route it to the first
    // cpu
    //
    // physical mode delivery to apic 0 = 000
    // unmasked = 0 
    // edge = 0
    // active high = 0
    // NMI = 100 = 4
    // vector = 2 (does not matter, NMI is always 2)
    //


    struct naut_info *c = nk_get_nautilus_info();

    if (c->sys.num_ioapics < 1) {
	ERROR("system has no ioapics\n");
	return -1;
    }
    
    struct ioapic *ioapic = c->sys.ioapics[0];

    uint64_t entry = 2 | (DELMODE_NMI << DEL_MODE_SHIFT);

    DEBUG("setting entry %0016lx\n",entry);
    
    ioapic_write_irq_entry(ioapic,
			   2,         // pin 2
			   entry);
    
    nk_watchdog_pet();

    return 0;
}

int nk_watchdog_check(void)
{
    struct cpu *my_cpu = get_cpu();
    my_cpu->watchdog_count += 1;
    uint64_t trigger_count = my_cpu->watchdog_count; 
    DEBUG("Checking watchdog timer on CPU %d, count = %lu, limit = %lu\n", my_cpu->id, trigger_count, timeout_limit);
    if (trigger_count >= timeout_limit || nk_watchdog_monitor_entry) { 
        __sync_fetch_and_or(&nk_watchdog_monitor_entry, 1);
        return 0; 
    }

    if (i8254_set_oneshot(bark_timeout_ns)) {
	ERROR("Failed to set i8254 timer\n");
	return 0;
    }
    return 1;
}

void nk_watchdog_reset(void)
{
    __sync_fetch_and_and(&nk_watchdog_monitor_entry, 0);
    
    // this assumes we want to reset the watchdog count on every CPU
    // We may not want to do this all the time. We could have this 
    // function take an int that indicates whether or not we should
    // reset all watchdog counts 
    struct sys_info *sys = per_cpu_get(system);
    int num_cpus = sys->num_cpus;
    int i;
    for (i=0; i < num_cpus; i++) {
        sys->cpus[i]->watchdog_count = 0;
    }
}


void nk_watchdog_deinit(void)
{
}



