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
 * Copyright (c) 2018, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/timer.h>
#include <nautilus/waitqueue.h>
#include <nautilus/scheduler.h>
#include <nautilus/semaphore.h>
#include <nautilus/shell.h>

// This is a trival implementation of classic semaphores for threads ONLY
// interrupts can use the try functions only

// that is NOT intended to be used for anything that requires performance


// set this to one to use the tried and true polling based implementation
// of push/pull with timeout instead of the (efficient) multiple wait queue
// implementations
#define USE_POLLING_TIMEOUT_FUNCS 0

#ifndef NAUT_CONFIG_DEBUG_SEMAPHORES
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("semaphore: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("semaphore: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("semaphore: " fmt, ##args)


static uint64_t   count=0;
static spinlock_t state_lock;
static struct list_head sem_list;

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);


#define SEMAPHORE_LOCK_CONF uint8_t _semaphore_lock_flags
#define SEMAPHORE_LOCK(s) _semaphore_lock_flags = spin_lock_irq_save(&(s)->lock)
#define SEMAPHORE_TRY_LOCK(s) spin_try_lock_irq_save(&(s)->lock,&_semaphore_lock_flags)
#define SEMAPHORE_UNLOCK(s) spin_unlock_irq_restore(&(s)->lock, _semaphore_lock_flags)
#define SEMAPHORE_UNIRQ(s) irq_enable_restore(_semaphore_lock_flags)

struct nk_semaphore {
    spinlock_t         lock;
    struct list_head   node; // for global list of named semaphores
    uint64_t           refcount;
    char               name[NK_SEMAPHORE_NAME_LEN];

    // count>0  =>  normal operation (down will not wait)
    // count==0 =>  next down will wait
    // count <0 => -count waiters exist, next down will wait
    int                 count;
    nk_wait_queue_t    *wait_queue;
    int                 prospective_count; // count blocked in timed down
};

struct nk_semaphore *nk_semaphore_create(char *name,
					 int init_count,
					 nk_semaphore_type_t type,
					 void *type_characteristics)
{
    STATE_LOCK_CONF;
    
    char buf[32];
    
    if (!name) {
	snprintf(buf,NK_SEMAPHORE_NAME_LEN, "semaphore%lu", __sync_fetch_and_add(&count,1));
	name = buf;
    }
    
    DEBUG("create %s count=%d\n",name, init_count);
    
    struct nk_semaphore *s = malloc(sizeof(*s));

    if (!s) {
	return 0;
    }

    memset(s,0,sizeof(*s));
    spinlock_init(&s->lock);
    INIT_LIST_HEAD(&s->node);
    s->refcount = 1;
    s->count = init_count;
    strncpy(s->name,name,NK_SEMAPHORE_NAME_LEN), s->name[NK_SEMAPHORE_NAME_LEN-1]=0;
    snprintf(buf,NK_WAIT_QUEUE_NAME_LEN,"%s-wait",s->name);
    s->wait_queue = nk_wait_queue_create(buf);

    if (!s->wait_queue) {
	free(s);
	ERROR("Failed to allocate wait queue\n");
	return 0;
    }

    STATE_LOCK();
    list_add_tail(&s->node,&sem_list);
    STATE_UNLOCK();

    DEBUG("created %s count=%d\n",s->name,s->count);
    
    return s;
}


void nk_semaphore_attach(struct nk_semaphore *s)
{
    SEMAPHORE_LOCK_CONF;
    DEBUG("attach to semaphore %s\n",s->name);
    SEMAPHORE_LOCK(s);
    s->refcount++;
    SEMAPHORE_UNLOCK(s);
    DEBUG("attach to semaphore %s end\n",s->name);
}

struct nk_semaphore *nk_semaphore_find(char *name)
{
    struct list_head *cur;
    struct nk_semaphore *target=0;

    DEBUG("find semaphore with name %s\n",name);
    STATE_LOCK_CONF;
    STATE_LOCK();
    list_for_each(cur,&sem_list) {
	if (!strncasecmp(list_entry(cur,struct nk_semaphore,node)->name,name,NK_SEMAPHORE_NAME_LEN)) { 
	    target = list_entry(cur,struct nk_semaphore, node);
	    break;
	}
    }
    STATE_UNLOCK();
    if (target) {
	DEBUG("find semaphore with name %s succeeded and attached\n",name);
	nk_semaphore_attach(target);
    } else {
	DEBUG("find semaphore with name %s failed\n",name);
    }
    return target;
}

void nk_semaphore_release(struct nk_semaphore *s)
{
    SEMAPHORE_LOCK_CONF;
    
    DEBUG("release semaphore with name %s\n",s->name);

    SEMAPHORE_LOCK(s);
    s->refcount--;
    if (s->refcount>0) {
	SEMAPHORE_UNLOCK(s);
	DEBUG("release semaphore with name %s - simple release\n",s->name);
	return;
    } else {
	STATE_LOCK_CONF;

	STATE_LOCK();
	list_del_init(&s->node);
	STATE_UNLOCK();
    
	nk_wait_queue_wake_all(s->wait_queue);
	nk_wait_queue_destroy(s->wait_queue);
	SEMAPHORE_UNLOCK(s);
	free(s);
	DEBUG("release semaphore with name %s - complex release\n",s->name);
    }
}

int nk_semaphore_try_up(struct nk_semaphore *s)
{
    SEMAPHORE_LOCK_CONF;

    DEBUG("try up start %s\n",s->name);
    
    int oldcount;
    int prospectives;
    if (SEMAPHORE_TRY_LOCK(s)) {
	return -1;
    }
    oldcount = s->count;
    s->count++;
    prospectives = s->prospective_count;
    SEMAPHORE_UNLOCK(s);
    if (oldcount<0 || prospectives) {
	// we just woke someone up
	DEBUG("try up wake %s\n",s->name);
	nk_wait_queue_wake_one(s->wait_queue);
    }
    DEBUG("up done %s\n",s->name);
    return 0;
}


void nk_semaphore_up(struct nk_semaphore *s)
{
    SEMAPHORE_LOCK_CONF;

    DEBUG("up start %s\n",s->name);
    
    int oldcount;
    int prospectives;
    SEMAPHORE_LOCK(s);
    oldcount = s->count;
    prospectives = s->prospective_count;
    s->count++;
    SEMAPHORE_UNLOCK(s);
    if (oldcount<0 || prospectives) {
	// we just woke someone up
	DEBUG("up wake %s\n",s->name);
	nk_wait_queue_wake_one(s->wait_queue);
    }
    DEBUG("up done %s\n",s->name);
}

int nk_semaphore_try_down(struct nk_semaphore *s)
{
    SEMAPHORE_LOCK_CONF;

    //DEBUG("try down %s\n",s->name);
    
    int have=0;
    if (SEMAPHORE_TRY_LOCK(s)) {
	return -1;
    }
    if (s->count>0) {
	s->count--;
	have = 1;
    }
    SEMAPHORE_UNLOCK(s);
    if (have) {
	//DEBUG("try down %s succeeded\n",s->name);
    } else {
	//DEBUG("try down %s failed\n",s->name);
    }
    return !have;
}


void nk_semaphore_down(struct nk_semaphore *s)
{
    SEMAPHORE_LOCK_CONF;

    DEBUG("down start %s\n",s->name);
    
    SEMAPHORE_LOCK(s);
    s->count--;
    if (s->count>=0) {
	SEMAPHORE_UNLOCK(s);
	DEBUG("down end %s - no wait\n",s->name);
	return;
    } else {
	DEBUG("down sleep %s\n",s->name);
	// we need to gracefully put ourselves to sleep
	nk_thread_t *t = get_cur_thread();

	// disable preemption early since interrupts may remain on given our locking model
	preempt_disable();

	// onto the semaphore's wait queue we go
	t->status = NK_THR_WAITING;
	nk_wait_queue_enqueue(s->wait_queue,t);

	// and go to sleep - this will also release the lock
	// and reenable preemption
	nk_sched_sleep(&s->lock);

	// We are now back, which means we must be good to go
	// except for interrupts
	SEMAPHORE_UNIRQ(s);

	DEBUG("down end %s - waited\n", s->name);

	return;
    }
}

struct op {
    struct nk_semaphore *sem;
    nk_timer_t          *timer;
};

// This only verifies that the count allows a possible down
// it does not actually do the down
static int check_count(void *s)
{
    struct op *o = (struct op *)s;
    return __sync_fetch_and_or(&o->sem->count,0) >= 1;
}

static int check_timer(void *s)
{
    struct op *o = (struct op *)s;
    return __sync_fetch_and_or(&o->timer->state,0)==NK_TIMER_SIGNALLED;
}

#if USE_POLLING_TIMEOUT_FUNCS

// this is a hideous, busy-wait implementation
// a real implementation would depend on being on multiple waitlists
// or us actually implementing wait-on-multiple-objects
int nk_semaphore_down_timeout(struct nk_semaphore *s, uint64_t timeout_ns)
{
    uint64_t start = nk_sched_get_realtime();
    uint64_t now;
    
    DEBUG("down timeout=%lu %s start\n",timeout_ns,s->name);
    do {
	if (!nk_semaphore_try_down(s)) {
	    DEBUG("down timeout=%lu %s end\n",timeout_ns,s->name);
	    // gotcha
	    return 0;
	}
	
	nk_sched_yield(0); // alternatively actually sleep here....
	now = nk_sched_get_realtime();
	
    } while ((now-start)<timeout_ns);
    
    DEBUG("down timeout=%lu %s end (timeout)\n",timeout_ns,s->name);
    // timeout
    return 1;
}

#else

int nk_semaphore_down_timeout(struct nk_semaphore *s, uint64_t timeout_ns)
{
    SEMAPHORE_LOCK_CONF;
    uint64_t start = nk_sched_get_realtime();
    uint64_t now = start;
    int oldval=0;
    
    DEBUG("down timeout=%lu %s start\n",timeout_ns,s->name);

 retry:
    if ((now-start) > timeout_ns) {
	// quick completion in this case
	DEBUG("down timeout %s ends with timeout\n",s->name);
	return 1;
    }
    
    SEMAPHORE_LOCK(s);
    oldval = s->count;
    if (oldval>0) {
	s->count--; // quick completion in this case
    } 
    SEMAPHORE_UNLOCK(s);

    if (oldval>0) {
	DEBUG("down timeout  %s ends with semaphore acquire\n",s->name);
	return 0;
    } else {
	nk_timer_t *t = nk_timer_get_thread_default();

	if (!t) {
	    ERROR("Failed to acquire timer for thread...\n");
	    return -1;
	}

	struct op o = { .sem = s, .timer = t };
	
	// the queues we will simultaneously be on
	nk_wait_queue_t *queues[2] = { s->wait_queue, t->waitq} ;
	// their condition checks
        int (*condchecks[2])() = { check_count, check_timer };
	// and state
	void *states[2] = { &o, &o }; 
	
	DEBUG("down sleep / timeout %s\n",s->name);

	if (nk_timer_set(t, timeout_ns - (now-start), NK_TIMER_WAIT_ONE, 0, 0, 0)) {
	    ERROR("Cannot set timer\n");
	    return -1;
	}

	if (nk_timer_start(t)) {
	    ERROR("Cannot start timer\n");
	    return -1;
	}

	// we are a prospective
	__sync_fetch_and_add(&s->prospective_count,1);

	DEBUG("starting multiple sleep\n");
	
	nk_wait_queue_sleep_extended_multiple(2,queues,condchecks,states);

	DEBUG("returned from multiple sleep and checking\n");

	// once we get here, we know we are off both wait queues
	// and we need to figure out what happened.   We can do that
	// by just starting over from the top, but adjusting the time
	// we also need to cancel the timer in case our wakeup was
	// due to the queue and not the time

	nk_timer_cancel(t);
  
	// no longer a prospective
	__sync_fetch_and_add(&s->prospective_count,-1);
	
	now = nk_sched_get_realtime();

	goto retry;
    }
}

#endif


int nk_semaphore_init()
{
    INIT_LIST_HEAD(&sem_list);
    spinlock_init(&state_lock);
    INFO("inited\n");
    return 0;
}

void nk_semaphore_deinit()
{
    if (!list_empty(&sem_list)) { 
	ERROR("Extant semaphores on deinit\n");
	return;
    }
    spinlock_deinit(&state_lock);
    INFO("deinit\n");
}


void nk_semaphore_dump_semaphores()
{
    struct list_head *cur;
    struct nk_semaphore *s=0;

    STATE_LOCK_CONF;
    STATE_LOCK();
    list_for_each(cur,&sem_list) {
	s = list_entry(cur,struct nk_semaphore, node);
	nk_vc_printf("%s : refcount=%lu count=%lu\n",
		     s->name, s->refcount, s->count);
    }
    STATE_UNLOCK();
}


static int
handle_sems (char * buf, void * priv)
{
    nk_semaphore_dump_semaphores();
    return 0;
}


static struct shell_cmd_impl sems_impl = {
    .cmd      = "sems",
    .help_str = "sems",
    .handler  = handle_sems,
};
nk_register_shell_cmd(sems_impl);
