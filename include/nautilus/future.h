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

#ifndef __NK_FUTURE
#define __NK_FUTURE

#include <nautilus/list.h>
#include <nautilus/waitqueue.h>

typedef struct nk_future {
    enum {
	NK_FUTURE_FREE=0,
	NK_FUTURE_IN_PROGRESS,
	NK_FUTURE_DONE
    }                state;
    void            *result;                   
    nk_wait_queue_t *waitqueue;    // only used if there can be blocking waits
    struct list_head node;         // used by allocator when future is free,
                                   // can be used by user otherwise
} nk_future_t;

#define FU_INFO(fmt, args...) INFO_PRINT("future: " fmt, ##args)
#define FU_ERROR(fmt, args...) ERROR_PRINT("future: " fmt, ##args)
#ifdef NAUT_CONFIG_DEBUG_FUTURES
#define FU_DEBUG(fmt, args...) DEBUG_PRINT("future: " fmt, ##args)
#else
#define FU_DEBUG(fmt, args...)
#endif
#define FU_WARN(fmt, args...)  WARN_PRINT("future: " fmt, ##args)

nk_future_t * nk_future_alloc();
void          nk_future_free(nk_future_t *f);

// user can recycle a future themselves, if they are smarter than
// the allocator
// there must be no waiters in the wait queue, nor racing, before this 
static inline int nk_future_recycle(nk_future_t *f)
{
    FU_DEBUG("recycle %p\n",f);
    f->state = NK_FUTURE_IN_PROGRESS;
    f->result = 0;
    return 0;
}

// see if future is available (no wait)
// < 0 => error
// = 0 => done, have result
// > 0 => not done yet
static inline int nk_future_check(volatile nk_future_t *f, void **result)
{
    FU_DEBUG("check %p (%s) state=%d\n",f,f->waitqueue->name,f->state);
    switch (f->state) {
    case NK_FUTURE_DONE:
	*result = f->result;
	return 0;
	break;
    case NK_FUTURE_IN_PROGRESS:
	return 1;
	break;
    default:
	return -1;
	break;
    }
}

static inline void nk_future_finish(nk_future_t *f, void *result)
{
    FU_DEBUG("finish %p\n",f);
    
    f->result = result;
    f->state = NK_FUTURE_DONE;
    // probably barrier here
    nk_wait_queue_wake_all(f->waitqueue);
}

typedef enum {
    NK_FUTURE_WAIT_SPIN,
    NK_FUTURE_WAIT_BLOCK
} nk_future_wait_t;

// user should just call nk_future_wait (below)
int nk_future_wait_block(nk_future_t *f, void **result);

static inline int nk_future_wait(nk_future_t *f, nk_future_wait_t wtype, void **result)
{
    if (wtype==NK_FUTURE_WAIT_SPIN) {
	int rc;
	FU_DEBUG("spinning wait on %p\n",f);
	do {
	    rc = nk_future_check(f,result);
	} while (rc>0);
	FU_DEBUG("spinning wait on %p done rc = %d result = %p\n",f, rc, *result);
	return rc ? -1 : 0;
    } else {
	return nk_future_wait_block(f,result);
    }
}

// call on BSP after waitqueues are available
int nk_future_init();

#endif
