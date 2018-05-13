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
#include <nautilus/scheduler.h>
#include <nautilus/semaphore.h>

// This is a trival implementation of classic semaphores for threads ONLY
// that is NOT intended to be used for anything that requires performance

struct nk_semaphore {
    spinlock_t         lock;
    // count>0  =>  normal operation (down will not wait)
    // count==0 =>  next down will wait
    // count <0 => -count waiters exist, next down will wait
    int                count;
    nk_thread_queue_t *wait_queue;
};

struct nk_semaphore *nk_semaphore_create(int init_count)
{
    struct nk_semaphore *s = malloc(sizeof(*s));

    if (!s) {
	return 0;
    }

    memset(s,0,sizeof(*s));

    spinlock_init(&s->lock);
    s->count = init_count;
    s->wait_queue = nk_thread_queue_create();

    if (!s->wait_queue) {
	free(s);
	return 0;
    }
    
    return s;
}
    
void nk_semaphore_destroy(struct nk_semaphore *s)
{
    spin_lock(&s->lock);
    nk_thread_queue_wake_all(s->wait_queue);
    nk_thread_queue_destroy(s->wait_queue);
    spin_unlock(&s->lock);
    // potential reace here... 
    free(s);
}

void nk_semaphore_up(struct nk_semaphore *s)
{
    int oldcount;
    spin_lock(&s->lock);
    oldcount = s->count;
    s->count++;
    spin_unlock(&s->lock);
    if (oldcount<0) {
	// we just woke someone up
	nk_thread_queue_wake_one(s->wait_queue);
    }
}

int nk_semaphore_try_down(struct nk_semaphore *s)
{
    int have=0;
    spin_lock(&s->lock);
    if (s->count>0) {
	s->count--;
	have = 1;
    }
    spin_unlock(&s->lock);
    return !have;
}


void nk_semaphore_down(struct nk_semaphore *s)
{
    spin_lock(&s->lock);
    s->count--;
    if (s->count>=0) {
	spin_unlock(&s->lock);
	return;
    } else {
	// we need to gracefully put ourselves to sleep
	nk_thread_t *t = get_cur_thread();

	// disable preemption early since interrupts remain on
	preempt_disable();

	// onto the semaphore's wait queue we go
	t->status = NK_THR_WAITING;
	nk_enqueue_entry(s->wait_queue,&t->wait_node);

	// and go to sleep - this will also release the lock
	// and reenable preemption
	nk_sched_sleep(&s->lock);

	// We are now back, which means we must be good to go
	// since we could only be awaked by an up
	return;
    }
}

// this is a hideous, busy-wait implementation
// a real implementation would depend on being on multiple waitlists
// or us actually implementing wait-on-multiple-objects
int nk_semaphore_down_timeout(struct nk_semaphore *s, uint64_t timeout_ns)
{
    uint64_t start = nk_sched_get_realtime();
    uint64_t now;
    
    do {
	if (!nk_semaphore_try_down(s)) {
	    // gotcha
	    return 0;
	}

	nk_sched_yield(0); // alternatively actually sleep here....
	now = nk_sched_get_realtime();

    } while ((now-start)<timeout_ns);

    // timeout
    return 1;
}

