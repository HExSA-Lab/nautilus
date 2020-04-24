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
#include <nautilus/cpu.h>
#include <nautilus/math.h>
#include <nautilus/irq.h>
#include <nautilus/dev.h>
#include <dev/i8254.h>


#ifndef NAUT_CONFIG_DEBUG_PIT
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif


static void 
i8254_disable (void)
{
    /* disable the PIT on setup */
    outb(0x30, PIT_CMD_REG);
    outb(0, PIT_CHAN0_DATA);
    outb(0, PIT_CHAN0_DATA);
}


ulong_t
i8254_calib_tsc (void)
{
    uint32_t latch = LATCH;
    uint64_t tsc, t1, t2, delta;
    ulong_t ms = MS;
	ulong_t tscmin, tscmax;
    int loopmin = LOOPS;
	int pitcnt;

	/* Set the Gate high, disable speaker */
	outb((inb(0x61) & ~0x02) | 0x01, 0x61);

	/*
	 * Setup CTC channel 2* for mode 0, (interrupt on terminal
	 * count mode), binary count. Set the latch register to 50ms
	 * (LSB then MSB) to begin countdown.
	 */
	outb(0xb0, 0x43);
	outb(latch & 0xff, 0x42);
	outb(latch >> 8, 0x42);

	tsc = t1 = t2 = rdtsc();

	pitcnt = 0;
	tscmax = 0;
	tscmin = ULONG_MAX;
	while ((inb(0x61) & 0x20) == 0) {
		t2 = rdtsc();
		delta = t2 - tsc;
		tsc = t2;
		if ((unsigned long) delta < tscmin)
			tscmin = (unsigned int) delta;
		if ((unsigned long) delta > tscmax)
			tscmax = (unsigned int) delta;
		pitcnt++;
	}

	/*
	 * Sanity checks:
	 *
	 * If we were not able to read the PIT more than loopmin
	 * times, then we have been hit by a massive SMI
	 *
	 * If the maximum is 10 times larger than the minimum,
	 * then we got hit by an SMI as well.
	 */
	if (pitcnt < loopmin) {
        return ULONG_MAX;
    }
        
    /*
    if (tscmax > 10 * tscmin) {
        return ULONG_MAX;
    }
    */

	/* Calculate the PIT value */
	delta = t2 - t1;
	return delta / ms;
}


static int 
pit_irq_handler (excp_entry_t * excp, excp_vec_t vec, void *state)
{
    DEBUG_PRINT("Received PIT Timer interrupt\n");
    IRQ_HANDLER_END();
    return 0;
}

static struct nk_dev_int ops = {
    .open=0,
    .close=0,
};

// Added to allow using channel 0 as a (bad) watchdog timer
int i8254_set_oneshot(uint64_t time_ns)
{
    uint16_t cycles;

    uint64_t secs_recip = 1000000000UL/time_ns;
    uint64_t ticks = 1193180UL/secs_recip;
    

    if (ticks>65536) {
	ERROR_PRINT("Impossible PIT request %lu ns, %lu ticks\n",time_ns,ticks);
	return -1;
    } else {
	if (ticks==65536) {
	    cycles = 0;
	} else {
	    cycles = (uint16_t) ticks;
	}
    }

    //
    // configure PIT channel 0 for terminal count in binary
    //
    // 00 11 000 0 (mode 0) 

    uint8_t ctrl =
	PIT_CHAN(PIT_CHAN_SEL_0) |
	PIT_ACC_MODE(PIT_ACC_MODE_BOTH) |
	PIT_MODE(PIT_MODE_TERMINAL_COUNT) ;

    outb(ctrl,PIT_CMD_REG);

    outb(cycles & 0xff, PIT_CHAN0_DATA);
    outb((cycles>>8) & 0xff, PIT_CHAN0_DATA);

    // timer now set
    return 0;
    
}


int 
i8254_init (struct naut_info * naut)
{
    printk("Initializing i8254 PIT\n");

    DEBUG_PRINT("Gating PIT channel 0\n");
    i8254_disable();

    if (register_irq_handler(PIT_TIMER_IRQ, pit_irq_handler, NULL) < 0) {
        ERROR_PRINT("Could not register timer interrupt handler\n");
        return -1;
    }
    
    nk_dev_register("i8254",NK_DEV_TIMER,0,&ops,0);

    nk_unmask_irq(PIT_TIMER_IRQ);

    return 0;
}
