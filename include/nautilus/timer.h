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
 * Copyright (c) 2018, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2018, The Interweaving Project <http://interweaving.org>
 *                     The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __TIMER_H__
#define __TIMER_H__

#include <nautilus/list.h>

#define NK_TIMER_NAME_LEN 32

typedef struct nk_wait_queue nk_wait_queue_t;

// the timer structure is visible here because code that
// integrates timer waitqueues and other wait queues will need
// to manipulate it directly
typedef struct nk_timer {
    char              name[NK_TIMER_NAME_LEN];
    volatile enum { NK_TIMER_INACTIVE = 0,
		    NK_TIMER_ACTIVE,
		    NK_TIMER_SIGNALLED} state;
    uint64_t          flags;
    uint64_t          time_ns;  // time relative to CPU reset
    nk_wait_queue_t   *waitq;   // used for non-spin waits
    uint32_t          cpu;      // cpu to use for callback
    void              (*callback)(void *priv);
    void              *priv;
    struct list_head  node;            // global list of all timers
    struct list_head  active_node;     // global list of active timers
} nk_timer_t;

nk_timer_t *nk_timer_create(char *name);
void        nk_timer_destroy(nk_timer_t *t);

// allocate a default timer for the current thread if needed
// always returns the current thread's default timer
nk_timer_t *nk_timer_get_thread_default();

// configures the timer
// Currently, flags can only be used in isolation, with
// the exception of NK_TIMER_CALLBACK
//
int nk_timer_set(nk_timer_t *t, 
		 uint64_t ns, // from the present time
		 uint64_t flags,
#define NK_TIMER_WAIT_ALL  0x1  // thread waits on wait queue for timer expiration
		                //   all threads wake on expiration
#define NK_TIMER_WAIT_ONE  0x2  // thread waits on wait queue for timer expiration
		                //   one thread wakes on expiration
#define NK_TIMER_SPIN      0x4  // thread busy-waits until timer expires
#define NK_TIMER_CALLBACK  0x8  // thread continue immediately,
		                //   callback is invoked on expiration
#define NK_TIMER_CALLBACK_WAIT 0x10 // wait until callback completes
#define NK_TIMER_CALLBACK_LOCAL_SYNC 0x20 // avoid xcall on cpu-local callback
		 void (*callback)(void *p), 
		 void *p,
		 uint32_t cpu);
// For callbacks, cpu is target CPU or one of the following:
#define NK_TIMER_CALLBACK_ALL_CPUS -1
#define NK_TIMER_CALLBACK_THIS_CPU -2

int nk_timer_reset(nk_timer_t *t,
		   uint64_t ns);  // from the present time

int nk_timer_start(nk_timer_t *t);

int nk_timer_cancel(nk_timer_t *t);

// should only be called for a non-callback timer
int nk_timer_wait(nk_timer_t *t);

// block on a timer for the time given
int nk_sleep(uint64_t ns);
// spin on a timer for the time given
int nk_delay(uint64_t ns);

int nk_timer_init();
void nk_timer_deinit();

void nk_timer_dump_timers();

// The cpu time driver (e.g., apic) will invoke the following handler
// function on every timer interrupt, regardless of how much time has passed
// The handler returns the time (in ns) from now whereupon it must be
// called again at the latest.
uint64_t nk_timer_handler(void);

#endif
