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
#include <nautilus/msg_queue.h>
#include <nautilus/list.h>

// This is a trival implementation of classic message queues for threads ONLY
// interrupt handlers can use the "try" functions

// that is NOT intended to be used for anything that requires performance


// this is also a place where we can enhance performance by adding new types
// of message queues


struct nk_msg_queue {
    spinlock_t         lock;
    struct list_head   node; // for the global list of named queues
    uint64_t           refcount;
    char               name[NK_MSG_QUEUE_NAME_LEN];

    nk_thread_queue_t *push_wait_queue;
    nk_thread_queue_t *pull_wait_queue;

    uint64_t           queue_size;
    uint64_t           cur_count;
    uint64_t           cur_push;
    uint64_t           cur_pull;
    void              *msgs[0];
};

#ifndef NAUT_CONFIG_DEBUG_MSG_QUEUES
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("msg_queue: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("msg_queue: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("msg_queue: " fmt, ##args)

static uint64_t   count=0;

static spinlock_t state_lock;

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);

#define QUEUE_LOCK_CONF uint8_t _queue_lock_flags
#define QUEUE_LOCK(q) _queue_lock_flags = spin_lock_irq_save(&(q)->lock)
#define QUEUE_TRY_LOCK(q) spin_try_lock_irq_save(&(q)->lock,&_queue_lock_flags)
#define QUEUE_UNLOCK(q) spin_unlock_irq_restore(&(q)->lock, _queue_lock_flags);
#define QUEUE_UNIRQ(q) irq_enable_restore(_queue_lock_flags);

static struct list_head queue_list;


int nk_msg_queue_init()
{
    INIT_LIST_HEAD(&queue_list);
    spinlock_init(&state_lock);
    INFO("inited\n");
    return 0;
}

void nk_msg_queue_deinit()
{
    if (!list_empty(&queue_list)) { 
	ERROR("Extant queues on deinit\n");
	return;
    }
    spinlock_deinit(&state_lock);
    INFO("deinit\n");
}


struct nk_msg_queue *nk_msg_queue_create(char *name,
					 uint64_t size,
					 nk_msg_queue_type_t type,
					 void *type_chars)
{
    STATE_LOCK_CONF;
    uint64_t mynum = __sync_fetch_and_add(&count,1);
    char buf[NK_MSG_QUEUE_NAME_LEN];

    if (!name) {
	snprintf(buf,NK_MSG_QUEUE_NAME_LEN,"msg_queue%lu",mynum);
	name = buf;
    }

    DEBUG("create %s with size %lu\n",name,size);
    
    struct nk_msg_queue *q = malloc(sizeof(*q)+size*sizeof(void*));

    if (!q) {
	ERROR("Cannot allocate\n");
	return 0;
    }

    memset(q,0,sizeof(*q));

    spinlock_init(&q->lock);
    INIT_LIST_HEAD(&q->node);
    q->refcount = 1;
    q->push_wait_queue = nk_thread_queue_create();
    if (!q->push_wait_queue) {
	free(q);
	ERROR("Failed to allocate push wait queue\n");
	return 0;
    }
    q->pull_wait_queue = nk_thread_queue_create();
    if (!q->pull_wait_queue) {
	nk_thread_queue_destroy(q->push_wait_queue);
	free(q);
	ERROR("Failed to allocate pull wait queue\n");
	return 0;
    }
    q->queue_size = size;
    q->cur_push = 0;
    q->cur_pull = 0;

    strncpy(q->name,name,NK_MSG_QUEUE_NAME_LEN); q->name[NK_MSG_QUEUE_NAME_LEN-1]=0;

    STATE_LOCK();
    list_add_tail(&q->node,&queue_list);
    STATE_UNLOCK();

    DEBUG("created %s size=%lu\n",q->name,q->queue_size);
    
    return q;
}

void nk_msg_queue_attach(struct nk_msg_queue *q)
{
    QUEUE_LOCK_CONF;
    DEBUG("attach to queue %s start\n",q->name);
    QUEUE_LOCK(q);
    q->refcount++;
    QUEUE_UNLOCK(q);
    DEBUG("attach to queue %s end\n",q->name);
}

struct nk_msg_queue *nk_msg_queue_find(char *name)
{
    struct list_head *cur;
    struct nk_msg_queue *target=0;

    DEBUG("find queue with name %s\n",name);
    STATE_LOCK_CONF;
    STATE_LOCK();
    list_for_each(cur,&queue_list) {
	if (!strncasecmp(list_entry(cur,struct nk_msg_queue,node)->name,name,NK_MSG_QUEUE_NAME_LEN)) { 
	    target = list_entry(cur,struct nk_msg_queue, node);
	    break;
	}
    }
    STATE_UNLOCK();
    if (target) {
	DEBUG("find queue with name %s succeeded and attached\n",name);
	nk_msg_queue_attach(target);
    } else {
	DEBUG("find queue with name %s failed\n",name);
    }	
    return target;
}


void nk_msg_queue_dump_queues()
{
    struct list_head *cur;
    struct nk_msg_queue *q=0;

    STATE_LOCK_CONF;
    STATE_LOCK();
    list_for_each(cur,&queue_list) {
	q = list_entry(cur,struct nk_msg_queue, node);
	nk_vc_printf("%s : refcount=%lu cur_count=%lu cur_push=%lu cur_pull=%lu\n",
		     q->name, q->refcount, q->cur_count, q->cur_push, q->cur_pull);
    }
    STATE_UNLOCK();
}



void nk_msg_queue_release(struct nk_msg_queue *q)
{
    QUEUE_LOCK_CONF;

    DEBUG("release queue with name %s\n",q->name);
    
    QUEUE_LOCK(q);
    q->refcount--;
    if (q->refcount>0) {
	QUEUE_UNLOCK(q);
	DEBUG("release queue with name %s - simple release\n",q->name);
	return;
    } else {
	STATE_LOCK_CONF;
	
	STATE_LOCK();
	list_del_init(&q->node);
	STATE_UNLOCK();

	nk_thread_queue_wake_all(q->push_wait_queue);
	nk_thread_queue_destroy(q->push_wait_queue);
	nk_thread_queue_wake_all(q->pull_wait_queue);
	nk_thread_queue_destroy(q->pull_wait_queue);
	QUEUE_UNLOCK(q);
	free(q);
	DEBUG("release queue with name %s - complex release\n",q->name);
    }
}

int nk_msg_queue_full(struct nk_msg_queue *q)
{
    return  q->cur_count==q->queue_size;
}    

int nk_msg_queue_empty(struct nk_msg_queue *q)
{
    return  q->cur_count==0;
}    
    
    

#define SLOT(q,n) ((q)->msgs[(((q)->n)) % ((q)->queue_size)])

// with lock held
static inline int _nk_msg_queue_try_push(struct nk_msg_queue *q, void *m)
{
    //DEBUG("try push %s\n",q->name);
    if (q->cur_count < q->queue_size) {
	SLOT(q,cur_push) = m;
	q->cur_push++;
	q->cur_count++;
	//DEBUG("try push %s done\n",q->name);
	return 0;
    } else {
	//DEBUG("try push %s full\n",q->name);
	return -1;
    }
}

// with lock held
static inline int _nk_msg_queue_try_pull(struct nk_msg_queue *q, void **m)
{
    //DEBUG("try pull %s\n",q->name);
    if (q->cur_count > 0) {
	*m = SLOT(q,cur_pull);
	q->cur_pull++;
	q->cur_count--;
	//DEBUG("try pull %s done\n",q->name);
	return 0;
    } else {
	//DEBUG("try pull %s empty\n",q->name);
	return -1;
    }
}

int  nk_msg_queue_try_push(struct nk_msg_queue *q, void *m)
{
    QUEUE_LOCK_CONF;
    int rc;

    //DEBUG("try push %s\n",q->name);

    if (QUEUE_TRY_LOCK(q)) {
	return -1;
    }
    rc = _nk_msg_queue_try_push(q,m);
    QUEUE_UNLOCK(q);
    if (!rc) {
	//DEBUG("try push %s succeeded\n",q->name);
	nk_thread_queue_wake_one(q->pull_wait_queue);
    } else {
	//DEBUG("try push %s failed\n",q->name);
    }
    return rc;
}
    
int  nk_msg_queue_try_pull(struct nk_msg_queue *q, void **m)
{
    QUEUE_LOCK_CONF;
    int rc;

    //DEBUG("try pull %s\n",q->name);

    if (QUEUE_TRY_LOCK(q)) {
	return -1;
    }
    rc = _nk_msg_queue_try_pull(q,m);
    QUEUE_UNLOCK(q);
    if (!rc) {
	//DEBUG("try pull %s succeeded\n",q->name);
	nk_thread_queue_wake_one(q->push_wait_queue);
    } else {
	//DEBUG("try pull %s failed\n",q->name);
    }
    return rc;
}



void nk_msg_queue_push(struct nk_msg_queue *q, void *m)
{
    QUEUE_LOCK_CONF;

    DEBUG("push begin %s\n",q->name);
 retry:
    QUEUE_LOCK(q);
    if (!_nk_msg_queue_try_push(q,m)) {
	// success is immediate
	QUEUE_UNLOCK(q);
	// we may need to wake up someone trying to pull
	nk_thread_queue_wake_one(q->pull_wait_queue);
 	DEBUG("push end %s\n",q->name);
	return;
    } else {
	DEBUG("push sleep %s\n", q->name);
	// we need to gracefully put ourselves to sleep
	nk_thread_t *t = get_cur_thread();

	// disable preemption early since interrupts may remain on given our locking model
	preempt_disable();

	// onto the msg_queue's wait queue we go
	t->status = NK_THR_WAITING;
	nk_enqueue_entry(q->push_wait_queue,&t->wait_node);

	// and go to sleep - this will also release the lock
	// and reenable preemption
	nk_sched_sleep(&q->lock);

	// We need to restore interrupts
	QUEUE_UNIRQ(q);
	
	DEBUG("push retry %s\n", q->name);
	// now we are awake, so try again
	goto retry;
    }


}

void nk_msg_queue_pull(struct nk_msg_queue *q, void **m)
{
    QUEUE_LOCK_CONF;

    DEBUG("pull begin %s\n",q->name);
 retry:
    QUEUE_LOCK(q);
    if (!_nk_msg_queue_try_pull(q,m)) {
	// success is immediate
	QUEUE_UNLOCK(q);
	// we may need to wake up someone trying to push
	nk_thread_queue_wake_one(q->push_wait_queue);
	DEBUG("pull end %s\n",q->name);
	return;
    } else {
	DEBUG("pull sleep %s\n", q->name);
	
	// we need to gracefully put ourselves to sleep
	nk_thread_t *t = get_cur_thread();

	// disable preemption early since interrupts remain on
	preempt_disable();

	// onto the msg_queue's wait queue we go
	t->status = NK_THR_WAITING;
	nk_enqueue_entry(q->pull_wait_queue,&t->wait_node);

	// and go to sleep - this will also release the lock
	// and reenable preemption
	nk_sched_sleep(&q->lock);

	QUEUE_UNIRQ(q)
	
	DEBUG("pull retry %s\n", q->name);
	
	// now we are awake, so try again
	goto retry;
    }
}

 
// hideous
int nk_msg_queue_push_timeout(struct nk_msg_queue *q, void *m, uint64_t timeout_ns)
{
    uint64_t start = nk_sched_get_realtime();
    uint64_t now;
    
    DEBUG("push timeout begin %s\n",q->name);
    do {
	if (!nk_msg_queue_try_push(q,m)) {
	    // gotcha
	    DEBUG("push timeout end %s\n",q->name);
	    return 0;
	}

	nk_sched_yield(0); // alternatively actually sleep here....
	now = nk_sched_get_realtime();

    } while ((now-start)<timeout_ns);

    // timeout
    DEBUG("push timeout end %s (timeout)\n",q->name);
    return 1;
}

// hideous
int nk_msg_queue_pull_timeout(struct nk_msg_queue *q, void **m, uint64_t timeout_ns)
{
    uint64_t start = nk_sched_get_realtime();
    uint64_t now;
    
    DEBUG("pull timeout begin %s\n",q->name);
    do {
	if (!nk_msg_queue_try_pull(q,m)) {
	    // gotcha
	    DEBUG("pull timeout end %s\n",q->name);
	    return 0;
	}

	nk_sched_yield(0); // alternatively actually sleep here....
	now = nk_sched_get_realtime();

    } while ((now-start)<timeout_ns);

    // timeout
    DEBUG("pull timeout end %s (timeout)\n",q->name);
    return 1;
}

