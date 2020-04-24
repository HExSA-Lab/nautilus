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
static int      barking_on_some_cpu=0;

/*
  Currently, this is a thin wrapper on the i8254 PIT
  and the first IOAPIC.   Both of these devices are
  always included in the build.

  There are several assumptions:
     - there is at least one IOAPIC
     - the PIT's interrupt line is wired to pin 2
       of the first IOAPIC (this is common)

  And some limitations:
     - We attempt to broadcast the timer to all CPUs
       via APIC logical destination mode.  This works fine
       for XAPIC.   For X2APIC, this is an attempt which
       at best is going to deliver to the first 16 CPUs
     - The i8254 has a limit of about 50ms.  We use
       a per-cpu counter to extend this.  Of course
       this is not particularly efficient since it means
       we are NMIing every CPU, at best, 20 times per second
     - when nk_watchdog_check_* fail, it means that the 
       total time has expired and the watchdog is 
       barking for real
     - watchdog petting also indicates whether any CPU
       has expired

  TURNING ON DEBUGGING OUTPUT IS LIKELY TO CAUSE DEADLOCKS
  SO BE CAREFUL/AWARE OF THIS

 */


int nk_watchdog_pet(void)
{
    if (!bark_timeout_ns) {
	DEBUG("skipping pet of nonexistent watchdog\n");
	return 0; // watchdog does not exist yet
    }

    if (barking_on_some_cpu) {
	DEBUG("watchdog is barking on some cpu\n");
	return 1;
    }

    struct cpu *my_cpu = get_cpu();


    // only cpu 0 will reset the timer, for efficiency

    if (my_cpu->id==0) {
	if (i8254_set_oneshot(bark_timeout_ns)) {
	    ERROR("Failed to set i8254 timer\n");
	    return -1;
	} else {
	    DEBUG("pet from %016lx (timer reset)\n",__builtin_return_address(0));
	}
    } else {
	DEBUG("pet from %016lx\n",__builtin_return_address(0));
    }
	    
    my_cpu->watchdog_count = 0;

    return 0;
}

int nk_watchdog_init(uint64_t timeout_ns)
{
    struct sys_info *sys = per_cpu_get(system);
    int num_cpus = sys->num_cpus;
    int i;
    for (i=0; i < num_cpus; i++) {
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



    struct naut_info *c = nk_get_nautilus_info();

    if (c->sys.num_ioapics < 1) {
	ERROR("system has no ioapics\n");
	return -1;
    }

    uint64_t entry;
    
    if (c->sys.cpus[0]->apic->mode == APIC_XAPIC) {
	// we will assume that the i8254 is hooked to pin 2 of
	// the first ioapic, and we will broadcast it to all cpus
	// using logical destination delivery to 0x01
	// we assume that all apics have their LDR/LDRM set to match
	//
	// logical mode delivery to all matching apics
	// 8 bits of destination (match against APIC LDR, LDMR registers)
	//     00000001
	// 17+8=25  64-25 = 39 bits of reserverd
	// next 17 bits:
	// unmasked = 0 
	// pin mode = edge = 0
	// level-latched = 0 (read bit)
	// polarity = active high = 0
	// dest busy bit = 0 (not used - this is a read bit)
	// dest mode = 1 (logical - all 8 bits of dest select set)
	// del mode = NMI = 100 = 4
	// vector = 00000010 = 2  (does not matter, NMI is always 2)
	//
	// last 17 bits:  0 0000 1100 0000 0010
	//                  0    c    0    2
	// 0x00 00 00 00 00 00 0c 02

	entry = 0x0100000000000c02ULL;

	INFO("configuring as an xapic system (flat logical to 0x1)\n");
	
    } else if (c->sys.cpus[0]->apic->mode == APIC_X2APIC) {
	// this is similar to to XAPIC, but we have no control
	// over the LDR on the APICs, so we need to read it from
	// the APIC, find the cluster, and then configure to send
	// to that cluster.    Note that we also assume there is only
	// a single cluster
	//
	// It is not obvious whether this will actually work
	// since the ioapic may not be able to form the message we
	// hope for... 

	// find out the cluster of the first apic
	// note that the DFR is is ignored (must be cluster mode)
	//
        uint32_t ldr = msr_read(0x80d); // LDR is now in an MSR for... speed?
	uint32_t cluster = ldr >> 16;
	uint32_t logid = ldr & 0xffff;

	// this is a wild-ass guess to select all 16 processors
	// in the cluster that contains cpu 0
	entry = 0x0000000000000c02ULL | (((uint64_t)(ldr | 0xf)) << 32);

	INFO("configuring as an x2apic system - cluster 0x%x logid 0x%x -- THIS WILL PROBABLY NOT WORK\n", cluster, logid); 
    } else {
	ERROR("Unsupported APIC type %d - not enabling watchdog\n");
	return -1;
    }
    
    
    struct ioapic *ioapic = c->sys.ioapics[0];


    DEBUG("setting entry %0016lx\n",entry);
    
    ioapic_write_irq_entry(ioapic,
			   2,         // pin 2
			   entry);
    
    nk_watchdog_pet();

    return 0;
}


void nk_watchdog_nmi(void)
{
    
    struct cpu *my_cpu = get_cpu();
    my_cpu->watchdog_count += 1;

    uint64_t trigger_count = my_cpu->watchdog_count;
    
    DEBUG("NMIing timer on CPU %d, count = %lu, limit = %lu\n",
	  my_cpu->id, trigger_count, timeout_limit);
    
    if (trigger_count >= timeout_limit || barking_on_some_cpu) {
	DEBUG("barking on CPU %d\n",my_cpu->id);
        __sync_fetch_and_or(&barking_on_some_cpu, 1);
    } else {
	if (my_cpu->id==0) {
	    DEBUG("reseting PIT\n");
	    if (i8254_set_oneshot(bark_timeout_ns)) {
		ERROR("Failed to set i8254 timer\n");
		__sync_fetch_and_or(&barking_on_some_cpu, -1);
	    }
	} else {
	    DEBUG("skipping PIT reset\n");
	}
    }
}

int nk_watchdog_check_any_cpu(void)
{
    DEBUG("Checking for any watchdog timer on this cpu: barking_on_some_cpu=%d\n", barking_on_some_cpu);

    return barking_on_some_cpu;
}

int nk_watchdog_check_this_cpu(void)
{
    struct cpu *my_cpu = get_cpu();
    uint64_t trigger_count = my_cpu->watchdog_count; 

    DEBUG("Checking watchdog timer on this cpu:  count = %lu, limit = %lu\n", trigger_count, timeout_limit);
    if (trigger_count >= timeout_limit) {
	return 1;
    } else {
	return 0;
    }
}

void nk_watchdog_reset(void)
{
    if (my_cpu_id()==0) { 
    
	DEBUG("reseting watchdog on all CPUs\n");
	
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
	
	__sync_fetch_and_and(&barking_on_some_cpu, 0);
    }
	
}


void nk_watchdog_deinit(void)
{
}



