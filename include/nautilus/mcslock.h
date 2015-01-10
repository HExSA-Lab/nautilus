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
