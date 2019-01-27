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
 * Copyright (c) 2018, The Interweaving Project <http://interweaving.org>
 *                     The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */


#include <nautilus/nautilus.h>
#include <nautilus/waitqueue.h>
#include <nautilus/scheduler.h>
#include <nautilus/shell.h>


/*
  This is an extended wait queue implementation that makes it possible 
  for a thread to wait on multiple queues at once. 

  IMPORTANT DEBUGGING NOTE: Wait queues are involved in non-polled
  serial output once serial_init() has finished at boot. Debugging
  output with serial mirroring uses serial output.  As a consequence,
  debug output within the wait queue implementation itself can easily
  cause deadlock.  Even polled serial output uses a lock.  
  IF YOU MUST HAVE DEBUG OUTPUT BE VERY CAREFUL.

 */

static uint64_t count=0;

static spinlock_t state_lock;
static struct list_head wq_list;

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);



nk_wait_queue_t *nk_wait_queue_create(char *name)
{
    nk_wait_queue_t *q = malloc(sizeof(*q));
    if (q) {
	STATE_LOCK_CONF;
	memset(q,0,sizeof(*q));
	if (name) {
	    strncpy(q->name,name,NK_WAIT_QUEUE_NAME_LEN);
	    q->name[NK_WAIT_QUEUE_NAME_LEN-1] = 0;
	} else {
	    snprintf(q->name,NK_WAIT_QUEUE_NAME_LEN,"waitqueue%lu",__sync_fetch_and_add(&count,1));
	}
	INIT_LIST_HEAD(&q->list);
	INIT_LIST_HEAD(&q->node);
	spinlock_init(&q->lock);
	STATE_LOCK();
	list_add_tail(&q->node,&wq_list);
	STATE_UNLOCK();
    }
    return q;
}

void  nk_wait_queue_destroy(nk_wait_queue_t *q)
{
    STATE_LOCK_CONF;
    STATE_LOCK();
    list_del_init(&q->node);
    STATE_UNLOCK();
    free(q);
}


/*
 * nk_wait_queue_sleep_extended
 *
 * Goes to sleep on the given queue, checking a condition as it does so
 *
 * @q: the thread queue to sleep on
 * @cond_check - condition to check (return nonzero if true) atomically with queuing
 * @state - state for cond_check
 *  
 */
void nk_wait_queue_sleep_extended(nk_wait_queue_t *wq, int (*cond_check)(void *state), void *state)
{
    nk_thread_t * t = get_cur_thread();
    uint8_t flags;

    WQ_DEBUG("Thread %lu (%s) going to sleep on queue %s\n", t->tid, t->name, wq->name);

    // grab control over the the wait queue
    flags = spin_lock_irq_save(&wq->lock);

    // at this point, any waker is either about to start on the
    // queue or has just finished  It's possible that
    // we have raced with with it and it has just finished
    // we therefore need to double check the condition now

    if (cond_check && cond_check(state)) { 
	// The condition we are waiting on has been achieved
	// already.  The waker is either done waking up
	// threads or has not yet started.  In either case
	// we do not want to queue ourselves
	spin_unlock_irq_restore(&wq->lock, flags);
	WQ_DEBUG("Thread %lu (%s) has fast wakeup on queue %sw - condition already met\n", t->tid, t->name, wq->name);
	return;
    } else {
	// the condition still is not signalled 
	// or the condition is not important, therefore
	// while still holding the lock, put ourselves on the 
	// wait queue

	WQ_DEBUG("Thread %lu (%s) is queueing itself on queue %s\n", t->tid, t->name, wq->name);
	
	t->status = NK_THR_WAITING;
	if (nk_wait_queue_enqueue_extended(wq,t,1)) {
	    WQ_ERROR("Cannot enqueue thread onto wait queue....\n");
	    panic("Cannot enqueue thread onto wait queue....\n");
	    return;
	}
	
	// force arch and compiler to do above writes
	__asm__ __volatile__ ("mfence" : : : "memory"); 

	// We now keep interrupts off across the context switch
	// since we now allow an interrupt handler to do a wake
	// and we do not want to race with it here.
	
	WQ_DEBUG("Thread %lu (%s) is having the scheduler put itself to sleep on queue %s\n", t->tid, t->name, wq->name);

	// We now get the scheduler to do a context switch
	// and just after it completes its scheduling pass, 
	// it will release the wait queue lock for us
	// it will also reenable preemption on its context switch out
	// and it will reset interrupts according to the thread it
	// is switching to
	nk_sched_sleep(&wq->lock);

	// When we return, wq->lock is released, and our interrupt state is the same
	// as we left it, which means we now need to restore state
	// note that for the duration we were switched out, other threads may have
	// had interrupts on.
	irq_enable_restore(flags);
	
	WQ_DEBUG("Thread %lu (%s) has slow wakeup on queue %s\n", t->tid, t->name, wq->name);

	return;
    }
}

struct op {
    int             count;
    nk_wait_queue_t **wq;
    int             (**cond_check)(void *);
    void            **state;
};

static int cond_check_multiple(void *state)
{
    struct op *o = (struct op *)state;
    int i;

    for (i=0;i<o->count;i++) {
	WQ_DEBUG("cond check %s\n",o->wq[i]->name);
	if (o->cond_check && o->cond_check[i]) {
	    if (o->state && o->state[i]) {
		if (o->cond_check[i](o->state[i])) { 
		    return 1;
		}
	    } else {
		if (o->cond_check[i](0)) {
		    return 1;
		}
	    }
	}
    }
    return 0;
}

static void wait_queue_release(void *state)
{
    struct op *o = (struct op *)state;
    int i;
    for (i=0;i<o->count;i++) {
	if (o->wq && o->wq[i]) {
	    WQ_DEBUG("queue lock release %s\n", o->wq[i]->name);
	    spin_unlock(&o->wq[i]->lock);
	}
    }
}

void nk_wait_queue_sleep_extended_multiple(int num_wq, nk_wait_queue_t **wq, int (**cond_check)(void *state), void **state)
{
    int i;
    nk_thread_t * t = get_cur_thread();
    uint8_t flags;

    struct op o;

    o.count = num_wq;
    o.wq = wq;
    o.cond_check = cond_check;
    o.state = state;
    
    WQ_DEBUG("Thread %lu (%s) going to sleep on %d queues:\n", t->tid, t->name, num_wq);
    for (i=0;i<num_wq;i++) {
	WQ_DEBUG("  queue: %s,  cond_check: %p, state: %p\n", wq[i]->name, cond_check[i], state[i]);
    }
    
    // turn off interrupts on this cpu
    flags = irq_disable_save();

    // we now cannot be interrupted or preempted
    
    // Acquire all wait queue locks
    // wait queues must be provided in order to assure that the following
    // cannot deadlock
    for (i=0;i<num_wq;i++) {
	spin_lock(&wq[i]->lock);
    }
    
    // We now own all the wait queues

    // check to see if any condition has been signalled before we actually go to sleep
    // this may have happened because our irq_disable_save and locking of wait queues
    // might have raced with a waker
    if (cond_check && cond_check_multiple(&o)) { 
	// At least one of the conditions we are waiting on has been achieved
	// already.
	for (i=0;i<num_wq;i++) {
	    spin_unlock(&wq[i]->lock);
	}
	irq_enable_restore(flags);
	WQ_DEBUG("Thread %lu (%s) has fast wakeup on some queue - condition(s) already met\n", t->tid, t->name);
	return;
    } else {
	// no condition has been signalled, or the condition is not important
	// we now put ourselves on all the wait queues...

	WQ_DEBUG("Thread %lu (%s) is queueing itself on all the queues\n", t->tid, t->name);
	
	t->status = NK_THR_WAITING;

	if (nk_wait_queue_enqueue_multiple_extended(num_wq, wq, t, 1)) {
	    WQ_ERROR("Cannot enqueue thread onto one or more wait queues....\n");
	    panic("Cannot enqueue thread onto one or more wait queues....\n");
	    return;
	}
	
	// force arch and compiler to do above writes
	__asm__ __volatile__ ("mfence" : : : "memory"); 

	// We now keep interrupts off across the context switch
	// since we now allow an interrupt handler to do a wake
	// and we do not want to race with it here.
	
	WQ_DEBUG("Thread %lu (%s) is having the scheduler put itself to sleep on all the queues\n", t->tid, t->name);

	// We now get the scheduler to do a context switch
	// and just after it completes its scheduling pass, 
	// it will release all of the wait queue locks for us
	// it will also reenable preemption on its context switch out
	// and it will reset interrupts according to the thread it
	// is switching to
	nk_sched_sleep_extended(wait_queue_release,(void*)&o);

	// When we return, all the wait queue locks are released, and
	// our interrupt state is the same as we left it (off)

	// We now need to remove ourselves from all the wait queues where
	// we have not been signalled - we can be awoken only once
	//
	// note that the dequeue will need to reacquire the locks
	nk_wait_queue_dequeue_multiple_extended(num_wq,wq,t,0);
	
	// we now need to restore state note that for the duration we
	// were switched out, other threads may have had interrupts
	// on.
	irq_enable_restore(flags);
	
	WQ_DEBUG("Thread %lu (%s) has slow wakeup on one or more of the queues\n", t->tid, t->name);

	return;
    }
    
}
				   

void nk_wait_queue_sleep(nk_wait_queue_t *wq)
{
    return nk_wait_queue_sleep_extended(wq,0,0);
}


void nk_wait_queue_wake_one_extended(nk_wait_queue_t * q, int havelock)
{
    nk_thread_t * t = 0;
    uint8_t flags=0;
    
    // avoid any output in this function since it can be called by even low-level serial output

    
    if (in_interrupt_context()) {
	//WQ_DEBUG("[Interrupt Context] Thread %lu (%s) is waking one waiter on wait queue %s\n", get_cur_thread()->tid, get_cur_thread()->name, q->name);
    } else {
	//WQ_DEBUG("Thread %lu (%s) is waking one waiter on wait queue %s\n", get_cur_thread()->tid, get_cur_thread()->name, q->name);
    }

    if (!havelock) {
	flags = spin_lock_irq_save(&q->lock);
    }

    t = nk_wait_queue_dequeue_extended(q, 1);
    
    if (!t) {
        goto out;
    }

    //
    // A thread that is sleeping on multiple queues may be awoken along multiple paths
    // hence we may be racing with a completed wakeup along a different path, which has
    // already made the thread schedulable

    if (__sync_bool_compare_and_swap(&t->status, NK_THR_WAITING, NK_THR_SUSPENDED)) {
	// if we switched it from waiting to suspended, we are responsible for getting
	// the scheduler involved
	if (nk_sched_awaken(t, t->current_cpu)) { 
	    WQ_ERROR("Failed to awaken thread\n");
	    goto out;
	}

	nk_sched_kick_cpu(t->current_cpu);

	//WQ_DEBUG("Thread queue wake one (q=%p) woke up thread %lu (%s)\n", (void*)q, t->tid, t->name);

    } else {

	//WQ_DEBUG("Thread queue wake one (q=%p) found that thread %lu (%s) was already awake\n", (void*)q, t->tid, t->name);
    }

 out:
    if (!havelock) {
	spin_unlock_irq_restore(&q->lock,flags);
    }

}



void nk_wait_queue_wake_all_extended(nk_wait_queue_t * q, int havelock)
{
    nk_thread_t * t = 0;
    uint8_t flags=0;

    // avoid any output in this function since it can be called by even low-level serial output

    if (in_interrupt_context()) {
	//WQ_DEBUG("[Interrupt Context] Thread %lu (%s) is waking all waiters on wait queue %s\n", get_cur_thread()->tid, get_cur_thread()->name, q->name);
    } else {
	//WQ_DEBUG("[Thread Context] Thread %lu (%s) is waking all waiters on wait queue %s\n", get_cur_thread()->tid, get_cur_thread()->name, q->name);
    }

    if (!havelock) {
	flags = spin_lock_irq_save(&q->lock);
    }

    while ((t = nk_wait_queue_dequeue_extended(q,1))) {

	if (__sync_bool_compare_and_swap(&t->status, NK_THR_WAITING, NK_THR_SUSPENDED)) {
	    // if we switched it from waiting to suspended, we are responsible for getting
	    // the scheduler involved
	    if (nk_sched_awaken(t, t->current_cpu)) { 
		WQ_ERROR("Failed to awaken thread\n");
		goto out;
	    }
	    
	    nk_sched_kick_cpu(t->current_cpu);

	    //WQ_DEBUG("Waking all waiters on wait queue %s woke thread %lu (%s)\n", q->name,t->tid,t->name);
	    
	} else {

	    //WQ_DEBUG("Waking all waiters on wait %s found that thread %lu (%s) was already awake\n", q->name, t->tid, t->name);
	}
    }

 out:
    if (!havelock) {
	spin_unlock_irq_restore(&q->lock, flags);
    }
}


int nk_wait_queue_init()
{
    INIT_LIST_HEAD(&wq_list);
    spinlock_init(&state_lock);
    WQ_INFO("inited\n");
    return 0;
}

void nk_wait_queue_deinit()
{
    if (!list_empty(&wq_list)) { 
	WQ_ERROR("Extant wait queues on deinit\n");
	return;
    }
    spinlock_deinit(&state_lock);
    WQ_INFO("deinit\n");
}


void nk_wait_queue_dump_queues()
{
    struct list_head *cur;
    struct nk_wait_queue *q=0;

    STATE_LOCK_CONF;
    STATE_LOCK();
    list_for_each(cur,&wq_list) {
	q = list_entry(cur,struct nk_wait_queue, node);
	nk_vc_printf("%-32s %lu waiters\n", q->name, q->num_wait);
    }
    STATE_UNLOCK();
}


static int
handle_wqs (char * buf, void * priv)
{
    nk_wait_queue_dump_queues();
    return 0;
}


static struct shell_cmd_impl wqs_impl = {
    .cmd      = "wqs",
    .help_str = "wqs",
    .handler  = handle_wqs,
};
nk_register_shell_cmd(wqs_impl);
