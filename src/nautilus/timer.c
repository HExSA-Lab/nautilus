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
#include <nautilus/nautilus.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <nautilus/percpu.h>
#include <nautilus/mm.h>
#include <nautilus/timer.h>
#include <nautilus/waitqueue.h>
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
static struct list_head active_timer_list;

static uint64_t count=0;


nk_timer_t *nk_timer_create(char *name)
{
    char buf[NK_TIMER_NAME_LEN];
    char mbuf[NK_WAIT_QUEUE_NAME_LEN];
    
    struct nk_timer *t = malloc(sizeof(struct nk_timer));
    
    if (!t) { 
	ERROR("Timer allocation failed\n");
	return 0;
    }
    
    memset(t,0,sizeof(struct nk_timer));

    if (!name) {
	snprintf(buf,NK_TIMER_NAME_LEN,"timer%lu",__sync_fetch_and_add(&count,1));
	name = buf;
    }

    strncpy(t->name,name,NK_TIMER_NAME_LEN);
    t->name[NK_TIMER_NAME_LEN-1] = 0;
    
    snprintf(mbuf,NK_WAIT_QUEUE_NAME_LEN,"%s-wait",t->name);
    t->waitq = nk_wait_queue_create(mbuf);

    if (!t->waitq) { 
	ERROR("Timer allocation of thread queue failed\n");
	free(t);
	return 0;
    }

    INIT_LIST_HEAD(&t->node);
    INIT_LIST_HEAD(&t->active_node);
    
    
    STATE_LOCK_CONF;

    STATE_LOCK();
    list_add_tail(&t->node,&timer_list);
    STATE_UNLOCK();
    
    return t;
}

void nk_timer_destroy(nk_timer_t *t)
{
    nk_timer_cancel(t); // remove from list at least
    nk_wait_queue_destroy(t->waitq);
    free(t);
}

int nk_timer_set(nk_timer_t *t, 
		 uint64_t ns, 
		 uint64_t flags,
		 void (*callback)(void *p), 
		 void *p,
		 uint32_t cpu)
{
    
    t->state = NK_TIMER_INACTIVE;
    t->flags = flags ;
    t->time_ns = nk_sched_get_realtime() + ns;
    t->callback = callback;
    t->priv = p;
    t->cpu = cpu;

    DEBUG("set %s : state=%s flags=0x%llx (%s), time=%lluns, callback=%p priv=%p cpu=%lu\n",
	  t->name,
	  t->state==NK_TIMER_INACTIVE ? "inactive" :
	  t->state==NK_TIMER_ACTIVE ? "ACTIVE" :
	  t->state==NK_TIMER_SIGNALLED ? "SIGNALLED" : "UNKNOWN",
	  t->flags,
	  t->flags==NK_TIMER_WAIT_ALL ? "wait-all" :
	  t->flags==NK_TIMER_WAIT_ONE ? "wait-one" :
	  t->flags==NK_TIMER_SPIN ? "spin" :
	  t->flags==NK_TIMER_CALLBACK ? "callback" : "UNKNOWN",
	  t->time_ns, t->callback, t->priv, t->cpu);

    return 0;
}


int nk_timer_reset(nk_timer_t *t, 
		   uint64_t ns)
{
    t->state = NK_TIMER_INACTIVE;
    t->time_ns = nk_sched_get_realtime() + ns;

    DEBUG("reset %s : state=%s flags=0x%llx (%s), time=%lluns, callback=%p priv=%p cpu=%lu\n",
	  t->name,
	  t->state==NK_TIMER_INACTIVE ? "inactive" :
	  t->state==NK_TIMER_ACTIVE ? "ACTIVE" :
	  t->state==NK_TIMER_SIGNALLED ? "SIGNALLED" : "UNKNOWN",
	  t->flags,
	  t->flags==NK_TIMER_WAIT_ALL ? "wait-all" :
	  t->flags==NK_TIMER_WAIT_ONE ? "wait-one" :
	  t->flags==NK_TIMER_SPIN ? "spin" :
	  t->flags==NK_TIMER_CALLBACK ? "callback" : "UNKNOWN",
	  t->time_ns, t->callback, t->priv, t->cpu);

    return 0;
}

int nk_timer_start(nk_timer_t *t)
{
    STATE_LOCK_CONF;
    
    STATE_LOCK();
    t->state = NK_TIMER_ACTIVE;
    list_add_tail(&t->active_node, &active_timer_list);
    STATE_UNLOCK();

    DEBUG("start %s mt=%d\n",t->name,list_empty(&active_timer_list));

    return 0;
}    

int nk_timer_cancel(nk_timer_t *t)
{
    STATE_LOCK_CONF;
    struct list_head *cur;

    STATE_LOCK();
    // We need to scan the active list because the timer could have
    // been canceled before this point or it could have fired
    // in either case, it is *not* on the list
    list_for_each(cur,&active_timer_list) {
	if (cur==&(t->active_node)) {
	    break;
	}
    }
    if (cur==&(t->active_node)) {
	DEBUG("canceling %s\n",t->name);
	list_del_init(cur);
	if (t->flags == NK_TIMER_WAIT_ALL) {
	    nk_wait_queue_wake_all(t->waitq);
	}
	if (t->flags == NK_TIMER_WAIT_ONE) {
	    nk_wait_queue_wake_one(t->waitq);
	}
    } else {
	DEBUG("not canceling %s as not in active list\n",t->name);
    }
    t->state = NK_TIMER_INACTIVE;
    STATE_UNLOCK();
    return 0;
}

static int check(void *state)
{
    nk_timer_t *t = state;
    return __sync_fetch_and_add(&t->state,0) == NK_TIMER_SIGNALLED;
}

int nk_timer_wait(nk_timer_t *t)
{
    DEBUG("wait %s with mode %s \n",t->name,
	  t->flags==NK_TIMER_WAIT_ALL ? "wait-all" :
	  t->flags==NK_TIMER_WAIT_ONE ? "wait-one" :
	  t->flags==NK_TIMER_SPIN ? "spin" :
	  t->flags==NK_TIMER_CALLBACK ? "callback (uh...)" : "UNKNOWN");

    if (t->flags==NK_TIMER_CALLBACK) {
	ERROR("trying to wait on a callback timer\n");
	return -1;
    }
    while (!check(t)) {
	if (t->flags != NK_TIMER_SPIN) { 
	    DEBUG("going to sleep on thread queue\n");
	    nk_wait_queue_sleep_extended(t->waitq, check, t);
	} else {
	    asm volatile ("pause");
	}
	DEBUG("try again\n");
    }
    return 0;
}

static int _sleep(uint64_t ns, int spin) 
{
    nk_timer_t * t = nk_timer_create(0);
    
    if (!t) { 
        ERROR("failed to allocate timer in sleep\n");
	return -1;
    }

    if (nk_timer_set(t, 
		     nk_sched_get_realtime() + ns,
		     spin ? NK_TIMER_SPIN : NK_TIMER_WAIT_ALL,
		     0,
		     0,
		     0)) { 
	ERROR("Failed to set timer in sleep\n");
	return -1;
    }

    if (nk_timer_start(t)) {
	ERROR("Failed to start timer in sleep\n");
	return -1;
    }
		 
    int rc = nk_timer_wait(t);
    
    nk_timer_destroy(t);

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
	DEBUG("update: cpu %d - ignored/infinity\n",my_cpu_id());
	return -1;  // infinitely far in the future
    }

    STATE_LOCK_CONF;
    nk_timer_t *cur, *temp;
    uint64_t now = nk_sched_get_realtime();
    uint64_t earliest = -1;
    struct list_head expired_list;
    INIT_LIST_HEAD(&expired_list);

    // first, find expired timers with lock held
    STATE_LOCK();
    list_for_each_entry_safe(cur, temp, &active_timer_list, active_node) {
	if (now >= cur->time_ns) { 
	    DEBUG("found expired timer %s\n",cur->name);
	    cur->state = NK_TIMER_SIGNALLED;
	    list_del_init(&cur->active_node);
	    list_add_tail(&cur->active_node, &expired_list);
	}
	    
    }
    STATE_UNLOCK();

    // now handle expired timers without holding the lock
    // so that callbacks/etc can restart the timer if desired
    list_for_each_entry_safe(cur, temp, &expired_list, active_node) {
	DEBUG("handle expired timer %s\n",cur->name);
	list_del_init(&cur->active_node);
	switch (cur->flags) {
	case NK_TIMER_WAIT_ONE:
	    DEBUG("waking one thread\n");
	    nk_wait_queue_wake_one(cur->waitq);
	    break;
	case NK_TIMER_WAIT_ALL:
	    DEBUG("waking all threads\n");
	    nk_wait_queue_wake_all(cur->waitq);
	    break;
	case NK_TIMER_CALLBACK: 
	    // launch callback, but do not wait for it
	    DEBUG("launching callback for %s\n", cur->name);
	    smp_xcall(cur->cpu,
		      cur->callback,
		      cur->priv,
		      0);
	    break;
	default:
	    ERROR("unsupported 0x%lx\n", cur->flags);
	    break;
	}
    }

    // Now we need to scan for the earliest given that the callbacks
    // may have started new timers, with lock held
    STATE_LOCK();
    list_for_each_entry_safe(cur, temp, &active_timer_list, active_node) {
	if (cur->time_ns < earliest) { 
	    earliest = cur->time_ns;
	}
    }
    STATE_UNLOCK();
    
    DEBUG("update: earliest is %llu\n",earliest);

    return earliest;
}


int nk_timer_init()
{
    spinlock_init(&state_lock);
    INIT_LIST_HEAD(&timer_list);
    INIT_LIST_HEAD(&active_timer_list);

    INFO("Timers inited\n");
    return 0;
}

void nk_timer_deinit()
{
    INFO("Timers deinited\n");
    return;
}


void nk_timer_dump_timers()
{
    struct list_head *cur;
    nk_timer_t *t=0;

    STATE_LOCK_CONF;
    STATE_LOCK();
    list_for_each(cur,&timer_list) {
	t = list_entry(cur,nk_timer_t, node);
	nk_vc_printf("%-32s %s %s %luw %luns 0x%lx %u %p \n",
		     t->name,
		     t->state==NK_TIMER_INACTIVE ? "inactive" :
		     t->state==NK_TIMER_ACTIVE ? "ACTIVE" :
		     t->state==NK_TIMER_SIGNALLED ? "SIGNALLED" : "UNKNOWN",
		     t->flags==NK_TIMER_WAIT_ALL ? "wait-all" :
		     t->flags==NK_TIMER_WAIT_ONE ? "wait-one" :
		     t->flags==NK_TIMER_SPIN ? "spin" :
		     t->flags==NK_TIMER_CALLBACK ? "callback" : "UNKNOWN",
		     t->waitq->num_wait,
		     t->time_ns, t->flags, t->cpu, t->callback);
    }
    STATE_UNLOCK();
}

