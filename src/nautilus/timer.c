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
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <nautilus/percpu.h>
#include <nautilus/mm.h>
#include <nautilus/timer.h>
#include <nautilus/scheduler.h>
#include <nautilus/spinlock.h>

#include <stddef.h>

#ifndef NAUT_CONFIG_DEBUG_TIMERS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("timer: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("timer: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("timer: " fmt, ##args)

static spinlock_t state_lock;
#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);

static struct list_head timer_list;


struct nk_timer {
    uint64_t          flags;    
    uint64_t          time_ns;  // time relative to CPU reset
    nk_thread_queue_t *waitq;   // used for non-spin waits
    uint32_t          cpu;      // cpu to use for callback
    void              (*callback)(void *priv);
    void              *priv;
    struct list_head  node;     // global list of active timers
    volatile uint8_t  signaled; // 1 = timer has fired
};


struct nk_timer *nk_alloc_timer()
{
    struct nk_timer *t = malloc(sizeof(struct nk_timer));
    
    if (!t) { 
	ERROR("Timer allocation failed\n");
	return 0;
    }
    
    memset(t,0,sizeof(struct nk_timer));

    t->waitq = nk_thread_queue_create();

    if (!t->waitq) { 
	ERROR("Timer allocation of thread queue failed\n");
	free(t);
	return 0;
    }
    
    return t;
}

void nk_free_timer(struct nk_timer *t)
{
    nk_cancel_timer(t); // remove from list at least
    nk_thread_queue_destroy(t->waitq);
    free(t);
}

int nk_set_timer(struct nk_timer *t, 
		 uint64_t ns, 
		 uint64_t flags,
		 void (*callback)(void *p), 
		 void *p,
		 uint32_t cpu)
{
    STATE_LOCK_CONF;
    
    t->flags = flags ;
    t->time_ns = nk_sched_get_realtime() + ns;
    t->callback = callback;
    t->priv = p;
    t->cpu = cpu;
    t->signaled = 0;

    STATE_LOCK();
    list_add(&t->node, &timer_list);
    STATE_UNLOCK();
    
    DEBUG("Timer %p set: flags=0x%llx, time=%lluns, callback=%p priv=%p cpu=%lu, signaled=%d\n",	  t, t->flags, t->time_ns, t->callback, t->priv, t->cpu, t->signaled);

    return 0;
}

int nk_cancel_timer(struct nk_timer *t)
{
    STATE_LOCK_CONF;
    struct list_head *cur;

    STATE_LOCK();
    // We need to scan the list because the timer could have
    // been canceled before this point or it could have fired
    // in either case, it is *not* on the list
    list_for_each(cur,&timer_list) {
	if (cur==&(t->node)) {
	    break;
	}
    }
    if (cur==&(t->node)) {
	DEBUG("Canceling timer %p\n",t);
	list_del(cur);
	// if anyone is waiting on it, it's their problem.... 
    } else {
	DEBUG("Not canceling timer %p as not in active list\n",t);
    }
    STATE_UNLOCK();
    t->signaled = 0;
    return 0;
}

int nk_wait_timer(struct nk_timer *t)
{
    DEBUG("Wait timer %p\n",t);
    while (!__sync_fetch_and_add(&t->signaled,0)) {
	if (!(t->flags & TIMER_SPIN)) { 
	    DEBUG("Going to sleep on thread queue\n");
	    nk_thread_queue_sleep(t->waitq);
	} else {
	    asm volatile ("pause");
	}
	DEBUG("Try again\n");
    }
    return 0;
}

static int _sleep(uint64_t ns, int spin) 
{
    struct nk_timer * t = nk_alloc_timer();
    
    if (!t) { 
        ERROR("Failed to allocate timer in sleep\n");
	return -1;
    }

    if (nk_set_timer(t, 
		     nk_sched_get_realtime() + ns,
		     spin ? TIMER_SPIN : 0,
		     0,
		     0,
		     0)) { 
	ERROR("Failed to set timer in sleep\n");
	return -1;
    }
		 
    int rc = nk_wait_timer(t);
    
    nk_free_timer(t);

    return rc;
}

int nk_sleep(uint64_t ns) { return _sleep(ns,0); }
int nk_delay(uint64_t ns) { return _sleep(ns,1); }

//
// Currently, the timer handler only makes sense on cpu 0
//
uint64_t nk_timer_handler (void)
{
    if (my_cpu_id()!=0) { 
	return -1;  // infinitely far in the future
    }

    STATE_LOCK_CONF;
    struct nk_timer *cur, *temp;
    uint64_t now = nk_sched_get_realtime();
    uint64_t earliest = -1;

    DEBUG("Timer update\n");

    STATE_LOCK();

    list_for_each_entry_safe(cur, temp, &timer_list, node) {
	if (now >= cur->time_ns) { 
	    DEBUG("Found expired timer %p\n",cur);
	    cur->signaled = 1;
	    list_del(&cur->node);
	    if (!(cur->flags & TIMER_SPIN)) { 
		// wake waiters
		DEBUG("Waking threads\n");
		nk_thread_queue_wake_all(cur->waitq);
	    }
	    if (cur->flags & TIMER_CALLBACK) { 
		// launch callback, but do not wait for it
		DEBUG("Launching callback\n");
		smp_xcall(cur->cpu,
			  cur->callback,
			  cur->priv,
			  0);
	    }
	} else {
	    if (cur->time_ns < earliest) { 
		// search for earliest active timer
		earliest = cur->time_ns;
	    }
	}
    }
    
    STATE_UNLOCK();
    
    DEBUG("Timer update: earliest is %llu\n",earliest);

    return earliest;
}


int nk_timer_init()
{
    spinlock_init(&state_lock);
    INIT_LIST_HEAD(&timer_list);

    INFO("Timers inited\n");
    return 0;
}

void nk_timer_deinit()
{
    INFO("Timers deinited\n");
    return;
}
