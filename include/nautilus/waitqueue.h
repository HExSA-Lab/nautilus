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

#ifndef __WAITQUEUE_H__
#define __WAITQUEUE_H__

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/spinlock.h>
#include <nautilus/list.h>

#define WQ_INFO(fmt, args...) INFO_PRINT("waitqueue: " fmt, ##args)
#define WQ_ERROR(fmt, args...) ERROR_PRINT("waitqueue: " fmt, ##args)
#ifdef NAUT_CONFIG_DEBUG_WAITQUEUES
#define WQ_DEBUG(fmt, args...) DEBUG_PRINT("waitqueue: " fmt, ##args)
#else
#define WQ_DEBUG(fmt, args...)
#endif
#define WQ_WARN(fmt, args...)  WARN_PRINT("waitqueue: " fmt, ##args)

#define NK_WAIT_QUEUE_NAME_LEN 32


// Internal functions are visible here so that the compiler can
// optimize the various inline variants of the external functions in a
// site-appropriate manner.   This allows fast common use of the
// wait queue primitives in dependent code like timers, semaphores,
// and message queues. 
//

// A wait queue contains pointers to the threads on it, allowing one
// thread to be in multiple wait queues at once.  A thread has a count
// of the number of wait queues it is currently on.  
// 
typedef struct nk_wait_queue_entry {
   struct list_head  node;
   struct nk_thread *thread;   // 0 = unused
} nk_wait_queue_entry_t;


//
// A wait queue includes not the list of pointer nodes, but also an
// allocator for these nodes.  Since the maximum number of threads is
// known at compile time, the allocator cannot fail and we need not
// use malloc except when the wait queue is created.
//
// Also, when the number of threads per wait queue is small
// the allocator should require very few steps in its scan
//
typedef struct nk_wait_queue {
    char       name[NK_WAIT_QUEUE_NAME_LEN];
    spinlock_t lock;
    struct list_head node; // for list of wait queues
    uint64_t   num_wait;
    struct list_head list;
    struct nk_wait_queue_entry slots[NAUT_CONFIG_MAX_THREADS];
} nk_wait_queue_t;

// Queue creation/destruction are defined separately since they are unlikely
// to be performance critical

nk_wait_queue_t *nk_wait_queue_create(char *name);
void             nk_wait_queue_destroy(nk_wait_queue_t *q);


static inline nk_wait_queue_entry_t *nk_wait_queue_alloc_entry(nk_wait_queue_t *q, nk_thread_t *t)
{
    int i;
    for (i=0;i<NAUT_CONFIG_MAX_THREADS;i++) {
	if (__sync_bool_compare_and_swap(&q->slots[i].thread,0,t)) {
	    INIT_LIST_HEAD(&q->slots[i].node);
	    return &q->slots[i];
	}
    }
    return 0; // should never happen
}

static inline void nk_wait_queue_free_entry(nk_wait_queue_t *q, nk_wait_queue_entry_t *e)
{
    (void)__sync_fetch_and_and(&e->thread,0);
}



static inline int nk_wait_queue_enqueue_extended(nk_wait_queue_t *q, nk_thread_t *t, int havelock)
{
    uint8_t flags=0;
    if (!havelock) {
	flags = spin_lock_irq_save(&q->lock);
    }
    nk_wait_queue_entry_t *e = nk_wait_queue_alloc_entry(q,t);
    if (!e) {
	if (!havelock) {
	    spin_unlock_irq_restore(&q->lock,flags);
	}
	return -1;
    } else { 
	list_add_tail(&e->node,&q->list);
	__sync_fetch_and_add(&t->num_wait,1);
	__sync_fetch_and_add(&q->num_wait,1);
	if (!havelock) {
	    spin_unlock_irq_restore(&q->lock,flags);
	}
	return 0;
    }
}



   
static inline nk_thread_t     *nk_wait_queue_dequeue_extended(nk_wait_queue_t *q, int havelock)
{
    uint8_t flags=0;
    nk_wait_queue_entry_t *e=0;
    nk_thread_t *t=0;
    if (!havelock) {
	flags = spin_lock_irq_save(&q->lock);
    }
    if (!list_empty(&q->list)) {
        e  = list_first_entry(&q->list, nk_wait_queue_entry_t, node);
	t = e->thread;
        list_del_init(&e->node);
	__sync_fetch_and_add(&t->num_wait,-1);
	__sync_fetch_and_add(&q->num_wait,-1);
	nk_wait_queue_free_entry(q,e);
    }
    if (!havelock) {
	spin_unlock_irq_restore(&q->lock,flags);
    }
    return t;
}



static inline void nk_wait_queue_remove_specific_extended(nk_wait_queue_t *q, nk_thread_t *t, int havelock)
{
    uint8_t flags=0;
    nk_wait_queue_entry_t *e=0;
    struct list_head *cur, *temp;
    if (!havelock) {
	flags = spin_lock_irq_save(&q->lock);
    }
    list_for_each_safe(cur,temp,&q->list) {
        e  = list_entry(cur, nk_wait_queue_entry_t, node);
	if (e->thread == t) {
	    list_del_init(cur);
	    __sync_fetch_and_add(&t->num_wait,-1);
	    __sync_fetch_and_add(&q->num_wait,-1);
	    nk_wait_queue_free_entry(q,e);
	}
    }
    if (!havelock) {
	spin_unlock_irq_restore(&q->lock,flags);
    }
}

static inline int nk_wait_queue_enqueue_multiple_extended(int count, nk_wait_queue_t **q, nk_thread_t *t, int havelocks)
{
    int i,j, fail=0;
    uint8_t flags;

    if (!havelocks) {
	flags = irq_disable_save();
	for (i=0;i<count;i++) {
	    spin_lock(&q[i]->lock);
	}
    }

    for (i=0;i<count;i++) {
	if (nk_wait_queue_enqueue_extended(q[i],t,1)) {
	    fail=-1;
	    break;
	}
    }

    if (fail) {
	// fail out gracefully
	for (j=0;j<i;j++) {
	    nk_wait_queue_remove_specific_extended(q[i],t,1);
	}
    }

    if (!havelocks) {
	for (i=0;i<count;i++) {
	    spin_unlock(&q[i]->lock);
	}
	irq_enable_restore(flags);
    }

    return fail;
}

static inline int nk_wait_queue_dequeue_multiple_extended(int count, nk_wait_queue_t **q, nk_thread_t *t, int havelocks)
{
    int i,j, fail=0;
    uint8_t flags;

    if (!havelocks) {
	flags = irq_disable_save();
	for (i=0;i<count;i++) {
	    spin_lock(&q[i]->lock);
	}
    }

    for (i=0;i<count;i++) {
	nk_wait_queue_remove_specific_extended(q[i],t,1);
    }

    if (!havelocks) {
	for (i=0;i<count;i++) {
	    spin_unlock(&q[i]->lock);
	}
	irq_enable_restore(flags);
    }

    return fail;
}

static inline int nk_wait_queue_empty(nk_wait_queue_t *q)
{
    return list_empty(&q->list);
}


// Sleep and wake are defined within the matching C file
// since these are unlikely to have performance implications when not inlined

void nk_wait_queue_sleep(nk_wait_queue_t *q);
void nk_wait_queue_sleep_extended(nk_wait_queue_t * q, int (*cond_check)(void *state), void *state);
void nk_wait_queue_sleep_extended_multiple(int num, nk_wait_queue_t **q, int (**cond_check)(void *state), void **state);

void nk_wait_queue_wake_one_extended(nk_wait_queue_t * q, int havelock);
void nk_wait_queue_wake_all_extended(nk_wait_queue_t * q, int havelock);

#define nk_wait_queue_enqueue(q,t) nk_wait_queue_enqueue_extended(q,t,0)
#define nk_wait_queue_dequeue(q) nk_wait_queue_dequeue_extended(q,0)
#define nk_wait_queue_remove_specific(q,t) nk_wait_queue_remove_specific_extended(q,t,0)
#define nk_wait_queue_enqueue_multiple(c,q,t) nk_wait_queue_enqueue_multiple_extended(c,q,t,0)
#define nk_wait_queue_dequeue_multiple(c,q,t) nk_wait_queue_dequeue_multiple_extended(c,q,t,0)
#define nk_wait_queue_wake_one(q) nk_wait_queue_wake_one_extended(q,0)
#define nk_wait_queue_wake_all(q) nk_wait_queue_wake_all_extended(q,0)

int nk_wait_queue_init();  // bsp only
void nk_wait_queue_deinit();

void nk_wait_queue_dump_queues();


#endif
