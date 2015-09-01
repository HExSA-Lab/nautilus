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
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __MCS_LOCK_H__
#define __MCS_LOCK_H__

#include <nautilus/cpu.h>
#include <nautilus/atomic.h>
#include <nautilus/smp.h>

struct nk_mcs_lock {
    struct nk_mcs_lock * next;
    int locked;
};

typedef struct nk_mcs_lock nk_mcs_lock_t;
    
//void nk_mcs_lock(nk_mcs_lock_t * l, nk_mcs_lock_t * me);
//void nk_mcs_unlock(nk_mcs_lock_t * l, nk_mcs_lock_t * me);
//int nk_mcs_trylock(nk_mcs_lock_t * l, nk_mcs_lock_t * me);

static inline void 
nk_mcs_lock (nk_mcs_lock_t * l, nk_mcs_lock_t * me)
{
    nk_mcs_lock_t * last;

    me->next   = NULL;
    me->locked = 0;

    last = xchg64((void**)&(l->next), me);

    /* did we get it? */
    if (likely(!last)) {
        return;

    /* someone else locked it */
    } else {

        *(volatile nk_mcs_lock_t**)(&(last->next)) = me;

        PAUSE_WHILE(me->locked != 1);
    }
}


static inline void 
nk_mcs_unlock (nk_mcs_lock_t * l, nk_mcs_lock_t * me)
{
    nk_mcs_lock_t * next = me->next;

    if (likely(!me->next)) {

        if (likely(atomic_cmpswap(l->next, me, NULL) == me)) {
            return;
        }

        mbarrier();

        PAUSE_WHILE(!(next = me->next));

    }

    asm volatile ("" ::: "memory");

    *(volatile int*)(&(me->next->locked)) = 1;
}


static inline int
nk_mcs_trylock (nk_mcs_lock_t * l, nk_mcs_lock_t * me)
{
    nk_mcs_lock_t * last;

    me->next   = NULL;
    me->locked = 0;

    last = atomic_cmpswap(l->next, NULL, me);

    return (!last) ? 0 : -1;
}

#endif
