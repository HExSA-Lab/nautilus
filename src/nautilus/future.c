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
 * Copyright (c) 2019, Peter Dinda
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/future.h>

#define NUM_SEED_FUTURES NAUT_CONFIG_MAX_CPUS

static spinlock_t state_lock;

// number of the next future
static uint64_t         future_num=0;

// number of available  futures in the pool
// ideally will be per-cpu
static uint64_t         future_free_count=0;
static struct list_head future_free_list;

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);


// must be called with state lock held, or on init
// does explicit allocation but does not
static nk_future_t * _nk_future_alloc()
{
    char buf[NK_WAIT_QUEUE_NAME_LEN];

    snprintf(buf,NK_WAIT_QUEUE_NAME_LEN,"future%lu",future_num++);

    FU_DEBUG("base alloc wq name %s\n",buf);
    
    nk_wait_queue_t *wq = nk_wait_queue_create(buf);
    if (!wq) {
	FU_ERROR("failed to allocate wq\n");
	return 0;
    }
    
    nk_future_t *f = malloc(sizeof(*f));

    if (!f) {
	FU_ERROR("Failed to allocate future\n");
	free(wq);
	return 0;
    }

    memset(f,0,sizeof(*f));

    f->waitqueue = wq;

    INIT_LIST_HEAD(&f->node);

    f->state = NK_FUTURE_IN_PROGRESS;

    FU_DEBUG("allocation returns %p (%s)\n",f,f->waitqueue->name);
    
    return f;
}

nk_future_t * nk_future_alloc()
{
    STATE_LOCK_CONF;

    FU_DEBUG("alloc\n");
    
    STATE_LOCK();

    if (list_empty(&future_free_list)) {
	STATE_UNLOCK();
	return _nk_future_alloc();
    }

    nk_future_t *f = list_first_entry(&future_free_list, struct nk_future, node);
    list_del_init(&f->node);
    future_free_count--;

    STATE_UNLOCK();

    f->state = NK_FUTURE_IN_PROGRESS;

    FU_DEBUG("fast alloc returns %p (%s)\n",f, f->waitqueue->name);
    
    return f;
}
	

//
// Note that this never shrinks the pool
//
void nk_future_free(nk_future_t *f)
{
    STATE_LOCK_CONF;

    f->state = NK_FUTURE_FREE;
    f->result = 0;
    
    STATE_LOCK();
    
    list_add(&f->node,&future_free_list);
    future_free_count++;

    STATE_UNLOCK();
}

static int cond_check(void *s)
{
    void *result_temp;
    return nk_future_check((nk_future_t *)s,&result_temp) != 1;
}

int nk_future_wait_block(nk_future_t *f, void **result)
{
    int rc;
    FU_DEBUG("start blocking wait on %p (%s)\n",f,f->waitqueue->name);
    while ((rc=nk_future_check(f,result))==1) {
	nk_wait_queue_sleep_extended(f->waitqueue,cond_check,f);
    }
    FU_DEBUG("end blocking wait on %p (%s) rc = %d result = %p\n",f,f->waitqueue->name,rc, *result);
    return rc;
}


// ideally this would create a per-cpu pool, but for
// now we create one global pool
int nk_future_init()
{
    int i;
    INIT_LIST_HEAD(&future_free_list);
    spinlock_init(&state_lock);
    // seed the pool
    for (i=0;i<NUM_SEED_FUTURES;i++) {
	nk_future_t *f = _nk_future_alloc();
	f->state=NK_FUTURE_FREE;
	list_add(&f->node,&future_free_list);
    }
    FU_INFO("inited (seeded pool with %d futures)\n", i);

    return 0;
}
