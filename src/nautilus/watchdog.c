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
 * Author: Peter Dinda <pdinda@northwestern.edu>
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


static uint64_t bark_timeout_ns=0;

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
}

int nk_watchdog_init(uint64_t timeout_ns)
{

    bark_timeout_ns = timeout_ns;
    INFO("timeout set to %lu ns\n",bark_timeout_ns);

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

void nk_watchdog_deinit(void)
{
}



