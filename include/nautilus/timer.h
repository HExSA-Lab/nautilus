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
#ifndef __TIMER_H__
#define __TIMER_H__

struct naut_info;

struct nk_timer;

struct nk_timer *nk_alloc_timer();
void             nk_free_timer(struct nk_timer *t);

// configures and starts the timer
int nk_set_timer(struct nk_timer *t, 
		 uint64_t ns, // from the present time
		 uint64_t flags,
#define TIMER_SPIN     0x1
#define TIMER_CALLBACK 0x2
		 void (*callback)(void *p), 
		 void *p,
		 uint32_t cpu);

int nk_cancel_timer(struct nk_timer *t);

// only makes sense for a spin or blocking timer, not a callback...
int nk_wait_timer(struct nk_timer *t);

int nk_sleep(uint64_t ns);
int nk_delay(uint64_t ns);

int nk_timer_init();
void nk_timer_deinit();

// handler returns ns from return time when it must be 
// called again at the latest.  The cpu time driver
// (e.g., apic) will invoke this one every timer interrupt
// whether this amount of timer has passed or not
uint64_t nk_timer_handler(void);

#endif
