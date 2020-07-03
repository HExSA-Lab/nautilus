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
#include <nautilus/shell.h>

#include <stddef.h>


#ifndef NAUT_CONFIG_DEBUG_TIMERS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("timer: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("timer: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("timer: " fmt, ##args)

// guards list of all timers
static spinlock_t state_lock;
#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_TRY_LOCK()  spin_try_lock_irq_save(&state_lock,&_state_lock_flags)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);

// guards list of active timers
static spinlock_t active_lock;
#define ACTIVE_LOCK_CONF uint8_t _active_lock_flags
#define ACTIVE_LOCK() _active_lock_flags = spin_lock_irq_save(&active_lock)
#define ACTIVE_TRY_LOCK()  spin_try_lock_irq_save(&active_lock,&_active_lock_flags)
#define ACTIVE_UNLOCK() spin_unlock_irq_restore(&active_lock, _active_lock_flags);

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
    STATE_LOCK_CONF;
    
    nk_timer_cancel(t); // remove from active list 
    nk_wait_queue_destroy(t->waitq);
    
    STATE_LOCK();
    list_del_init(&t->node); // remove from timer list
    STATE_UNLOCK();
    
    free(t);
}

int nk_timer_set(nk_timer_t *t, 
		 uint64_t ns, 
		 uint64_t flags,
		 void (*callback)(void *p), 
		 void *p,
		 uint32_t cpu)
{
    int oldstate;

    oldstate = __sync_lock_test_and_set(&t->state, NK_TIMER_INACTIVE);

    if (oldstate == NK_TIMER_ACTIVE) {
	ERROR("Weird - setting active timer %s\n", t->name);
    }
    
    t->flags = flags ;
    t->time_ns = nk_sched_get_realtime() + ns;
    t->callback = callback;
    t->priv = p;
    if (cpu==NK_TIMER_CALLBACK_THIS_CPU) { 
	t->cpu = my_cpu_id();
    } else {
	t->cpu = cpu;
    }

    DEBUG("set %s : state=%s flags=0x%llx (%s%s%s), time=%lluns, callback=%p priv=%p cpu=%lu\n",
	  t->name,
	  t->state==NK_TIMER_INACTIVE ? "inactive" :
	  t->state==NK_TIMER_ACTIVE ? "ACTIVE" :
	  t->state==NK_TIMER_SIGNALLED ? "SIGNALLED" : "UNKNOWN",
	  t->flags,
	  t->flags & NK_TIMER_WAIT_ALL ? "wait-all" :
	  t->flags & NK_TIMER_WAIT_ONE ? "wait-one" :
	  t->flags & NK_TIMER_SPIN ? "spin" :
	  t->flags & NK_TIMER_CALLBACK ? "callback" : "UNKNOWN",
	  t->flags & NK_TIMER_CALLBACK_WAIT ? " wait" : "",
	  t->flags & NK_TIMER_CALLBACK_LOCAL_SYNC ? " local-sync" : "",
	  t->time_ns, t->callback, t->priv, t->cpu);

    return 0;
}


int nk_timer_reset(nk_timer_t *t, 
		   uint64_t ns)
{
    int oldstate;

    oldstate = __sync_lock_test_and_set(&t->state, NK_TIMER_INACTIVE);

    if (oldstate == NK_TIMER_ACTIVE) {
	ERROR("Weird - resetting active timer %s\n", t->name);
    }
    
    t->time_ns = nk_sched_get_realtime() + ns;

    DEBUG("reset %s : state=%s flags=0x%llx (%s%s%s), time=%lluns, callback=%p priv=%p cpu=%lu\n",
	  t->name,
	  t->state==NK_TIMER_INACTIVE ? "inactive" :
	  t->state==NK_TIMER_ACTIVE ? "ACTIVE" :
	  t->state==NK_TIMER_SIGNALLED ? "SIGNALLED" : "UNKNOWN",
	  t->flags,
	  t->flags & NK_TIMER_WAIT_ALL ? "wait-all" :
	  t->flags & NK_TIMER_WAIT_ONE ? "wait-one" :
	  t->flags & NK_TIMER_SPIN ? "spin" :
	  t->flags & NK_TIMER_CALLBACK ? "callback" : "UNKNOWN",
	  t->flags & NK_TIMER_CALLBACK_WAIT ? " wait" : "",
	  t->flags & NK_TIMER_CALLBACK_LOCAL_SYNC ? " local-sync" : "",
	  t->time_ns, t->callback, t->priv, t->cpu);

    return 0;
}

int nk_timer_start(nk_timer_t *t)
{
    ACTIVE_LOCK_CONF;
    int was_active=0;
    
    ACTIVE_LOCK();
    if (t->state == NK_TIMER_ACTIVE) {
	// do not add it again if it's already been started...
	was_active = 1;
    } else {
	t->state = NK_TIMER_ACTIVE;
	list_add_tail(&t->active_node, &active_timer_list);
	was_active = 0;
    }
    ACTIVE_UNLOCK();

    if (was_active) { 
	ERROR("Weird:  started already active timer %s\n",t->name);
    } else {
	DEBUG("start %s\n",t->name);
    }

    return 0;
}    

int nk_timer_cancel(nk_timer_t *t)
{
    ACTIVE_LOCK_CONF;
    int was_active=0;

    ACTIVE_LOCK();
    // we may not be active - only delete if we are
    if (t->state == NK_TIMER_ACTIVE) { 
	list_del_init(&t->active_node);
	was_active=1;
    }
    t->state = was_active ? NK_TIMER_SIGNALLED : NK_TIMER_INACTIVE;
    ACTIVE_UNLOCK();
    // now do handling that does not require the lock
    if (was_active) { 
	DEBUG("canceling %s\n",t->name);
	if (t->flags & NK_TIMER_WAIT_ALL) {
	    DEBUG("waking all thread timer %p %s waitqueue %p %s \n", t, t->name, t->waitq, t->waitq->name);
	    nk_wait_queue_wake_all(t->waitq);
	    return 0;
	}
	if (t->flags & NK_TIMER_WAIT_ONE) {
	    nk_wait_queue_wake_one(t->waitq);
	    return 0;
	}
	// nothing to do for other modes
	return 0;
    } else {
	DEBUG("not canceling %s as not in active list\n",t->name);
	return -1;
    }
}

static int check(void *state)
{
    nk_timer_t *t = state;
    return __sync_fetch_and_add(&t->state,0) == NK_TIMER_SIGNALLED;
}

int nk_timer_wait(nk_timer_t *t)
{
    DEBUG("wait %s with mode %s \n",t->name,
	  t->flags & NK_TIMER_WAIT_ALL ? "wait-all" :
	  t->flags & NK_TIMER_WAIT_ONE ? "wait-one" :
	  t->flags & NK_TIMER_SPIN ? "spin" :
	  t->flags & NK_TIMER_CALLBACK ? "callback (uh...)" : "UNKNOWN");

    // must be either active or signalled...
    if (t->state == NK_TIMER_INACTIVE) {
	ERROR("waiting on inactive timer %s\n",t->name);
	return -1;
    }
    
    if (t->flags & NK_TIMER_CALLBACK) {
	ERROR("trying to wait on a callback timer\n");
	return -1;
    }
    
    while (!check(t)) {
	if (!(t->flags & NK_TIMER_SPIN)) {
           DEBUG("going to sleep on wait queue timer %p %s waitqueue %p %s \n", t, t->name, t->waitq, t->waitq->name);
	    nk_wait_queue_sleep_extended(t->waitq, check, t);
	} else {
	    asm volatile ("pause");
	}
	//DEBUG("try again\n");
    }
    return 0;
}

nk_timer_t *nk_timer_get_thread_default()
{
    nk_thread_t *thread = get_cur_thread();

    if (!thread->timer) {
	char buf[NK_TIMER_NAME_LEN];
	if (thread->name[0]) {
	    snprintf(buf,NK_TIMER_NAME_LEN,"thread-%s-timer",thread->name);
	} else {
	    snprintf(buf,NK_TIMER_NAME_LEN,"thread-%lu-timer",thread->tid);
	}
	thread->timer = nk_timer_create(buf);
    }

    // note the per-thread timer is deallocated by nk_thread_destroy

    return thread->timer;
}

static int _sleep(uint64_t ns, int spin) 
{
    nk_timer_t *t = nk_timer_get_thread_default();

    if (!t) { 
        ERROR("No timer available in sleep\n");
	return -1;
    }

    if (nk_timer_set(t, 
		     ns,
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

    return nk_timer_wait(t);
    
}

int nk_sleep(uint64_t ns) { return _sleep(ns,0); }
int nk_delay(uint64_t ns) { return _sleep(ns,1); }

//
// Currently, the timer handler only makes sense on cpu 0
//
//
// Note that debug output here is often a bad idea since
// timers are used in places for efficient debug output
// and so could cause deadlock.   Only enable the
// debug output if you know what you are doing
uint64_t nk_timer_handler (void)
{
    uint32_t my_cpu = my_cpu_id();
    
    if (my_cpu!=0) {
	//DEBUG("update: cpu %d - ignored/infinity\n",my_cpu);
	return -1;  // infinitely far in the future
    }

    ACTIVE_LOCK_CONF;
    nk_timer_t *cur, *temp;
    uint64_t now = nk_sched_get_realtime();
    uint64_t earliest = -1;
    struct list_head expired_list;
    INIT_LIST_HEAD(&expired_list);

    ACTIVE_LOCK();
    
    // first, find expired timers with lock held
    list_for_each_entry_safe(cur, temp, &active_timer_list, active_node) {
	//DEBUG("considering %s\n",cur->name);
	if (now >= cur->time_ns) { 
	    //DEBUG("found expired timer %s\n",cur->name);
	    cur->state = NK_TIMER_SIGNALLED;
	    list_del_init(&cur->active_node);
	    list_add_tail(&cur->active_node, &expired_list);
	}
	    
    }
    ACTIVE_UNLOCK();

    // now handle expired timers without holding the lock
    // so that callbacks/etc can restart the timer if desired
    list_for_each_entry_safe(cur, temp, &expired_list, active_node) {
	//DEBUG("handle expired timer %s\n",cur->name);
	list_del_init(&cur->active_node);
	
	if (cur->flags & NK_TIMER_WAIT_ONE) {
	    
	    //DEBUG("waking one thread\n");
	    nk_wait_queue_wake_one(cur->waitq);
	    
	} else if (cur->flags & NK_TIMER_WAIT_ALL) {
	    
	    //DEBUG("waking all threads\n");
	    nk_wait_queue_wake_all(cur->waitq);
	    
	} else if (cur->flags & NK_TIMER_CALLBACK) {
	    
	    uint32_t cur_cpu, min_cpu, max_cpu;
	    int wait = !!(cur->flags & NK_TIMER_CALLBACK_WAIT);
	    
	    if (cur->cpu == NK_TIMER_CALLBACK_ALL_CPUS) {
		min_cpu = 0;
		max_cpu = nk_get_num_cpus() - 1;
	    } else {
		min_cpu = cur->cpu;
		max_cpu = cur->cpu;
	    }
	    
	    for (cur_cpu=min_cpu; cur_cpu<= max_cpu; cur_cpu++) {
		
		if ((cur_cpu == my_cpu) && (cur->flags & NK_TIMER_CALLBACK_LOCAL_SYNC)) {
		    cur->callback(cur->priv);
		} else {
		    smp_xcall(cur_cpu,
			      cur->callback,
			      cur->priv,
			      wait);
		}
	    }
	} else {
	    //ERROR("unsupported 0x%lx\n", cur->flags);
	}
    }

    // Now we need to scan for the earliest given that the callbacks
    // may have started new timers, with lock held
    ACTIVE_LOCK();
    list_for_each_entry_safe(cur, temp, &active_timer_list, active_node) {
	//DEBUG("check timer %s\n",cur->name);
	if (cur->time_ns < earliest) { 
	    earliest = cur->time_ns;
	}
    }
    ACTIVE_UNLOCK();

    //DEBUG("update: earliest is %llu\n",earliest);

    now = nk_sched_get_realtime();
    
    return earliest > now ? earliest-now : 0;
}


int nk_timer_init()
{
    spinlock_init(&state_lock);
    spinlock_init(&active_lock);
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
	nk_vc_printf("%-32s %s %s%s%s %luw %luns 0x%lx %u %p \n",
		     t->name,
		     t->state==NK_TIMER_INACTIVE ? "inactive" :
		     t->state==NK_TIMER_ACTIVE ? "ACTIVE" :
		     t->state==NK_TIMER_SIGNALLED ? "SIGNALLED" : "UNKNOWN",
		     t->flags & NK_TIMER_WAIT_ALL ? "wait-all" :
		     t->flags & NK_TIMER_WAIT_ONE ? "wait-one" :
		     t->flags & NK_TIMER_SPIN ? "spin" :
		     t->flags & NK_TIMER_CALLBACK ? "callback" : "UNKNOWN",
		     t->flags & NK_TIMER_CALLBACK_WAIT ? " wait" : "",
		     t->flags & NK_TIMER_CALLBACK_LOCAL_SYNC ? " local-sync" : "",
		     t->waitq->num_wait,
		     t->time_ns, t->flags, t->cpu, t->callback);
    }
    STATE_UNLOCK();
}

static int
handle_delay (char * buf, void * priv)
{
    uint64_t time_us;

    if (sscanf(buf,"delay %lu", &time_us) == 1) {
        nk_vc_printf("Delaying for %lu us\n", time_us);
        nk_delay(time_us*1000UL);
        return 0;
    }

    nk_vc_printf("invalid delay format\n");

    return 0;
}

static int
handle_sleep (char * buf, void * priv)
{
    uint64_t time_us;

    if (sscanf(buf,"sleep %lu", &time_us) == 1) {
        nk_vc_printf("Sleeping for %lu us\n", time_us);
        nk_sleep(time_us*1000UL);
        return 0;
    }

    nk_vc_printf("invalid sleep format\n");

    return 0;
}


static int
handle_timers (char * buf, void * priv)
{
    nk_timer_dump_timers();
    return 0;
}


static struct shell_cmd_impl delay_impl = {
    .cmd      = "delay",
    .help_str = "delay us",
    .handler  = handle_delay,
};
nk_register_shell_cmd(delay_impl);

static struct shell_cmd_impl sleep_impl = {
    .cmd      = "sleep",
    .help_str = "sleep us",
    .handler  = handle_sleep,
};
nk_register_shell_cmd(sleep_impl);

static struct shell_cmd_impl timers_impl = {
    .cmd      = "timers",
    .help_str = "timers",
    .handler  = handle_timers,
};
nk_register_shell_cmd(timers_impl);
