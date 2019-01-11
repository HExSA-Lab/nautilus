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
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/naut_types.h>
#include <nautilus/intrinsics.h>
#include <nautilus/atomic.h>
#include <nautilus/cpu.h>
#include <nautilus/cpu_state.h>
#include <nautilus/instrument.h>

#define SPINLOCK_INITIALIZER 0

typedef uint32_t spinlock_t;

void 
spinlock_init (volatile spinlock_t * lock);

void
spinlock_deinit (volatile spinlock_t * lock);

static inline void
spin_lock (volatile spinlock_t * lock) 
{
    NK_PROFILE_ENTRY();
    
    while (__sync_lock_test_and_set(lock, 1)) {
	// spin away
    }

    NK_PROFILE_EXIT();
}

// returns zero on successful lock acquisition, -1 otherwise
static inline int
spin_try_lock(volatile spinlock_t *lock)
{
    return  __sync_lock_test_and_set(lock,1) ? -1 : 0 ;
}

static inline uint8_t
spin_lock_irq_save (volatile spinlock_t * lock)
{
    uint8_t flags = irq_disable_save();
    PAUSE_WHILE(__sync_lock_test_and_set(lock, 1));
    return flags;
}

static inline int
spin_try_lock_irq_save(volatile spinlock_t *lock, uint8_t *flags)
{
    *flags = irq_disable_save();
    if (__sync_lock_test_and_set(lock,1)) {
	irq_enable_restore(*flags);
	return -1;
    } else {
	return 0;
    }
}

void
spin_lock_nopause (volatile spinlock_t * lock);

uint8_t
spin_lock_irq_save_nopause (volatile spinlock_t * lock);

static inline void 
spin_unlock (volatile spinlock_t * lock) 
{
    NK_PROFILE_ENTRY();
    __sync_lock_release(lock);
    NK_PROFILE_EXIT();
}

static inline void
spin_unlock_irq_restore (volatile spinlock_t * lock, uint8_t flags)
{
    __sync_lock_release(lock);
    irq_enable_restore(flags);
}



#ifdef NAUT_CONFIG_USE_TICKETLOCKS
#include <nautilus/ticketlock.h>
// this expects the struct, not the pointer to it
#define NK_LOCK_GLBINIT(l) 
#define NK_LOCK_T         nk_ticket_lock_t
#define NK_LOCK_INIT(l)   nk_ticket_lock_deinit(l)
#define NK_LOCK(l)        nk_ticket_lock(l)
#define NK_TRY_LOCK(l)    nk_ticket_trylock(l)
#define NK_UNLOCK(l)      nk_ticket_unlock(l)
#define NK_LOCK_DEINIT(l) nk_ticket_lock_deinit(l)
#else
// this expects the struct, not the pointer to it
#define NK_LOCK_GLBINIT(l) ((l) = SPINLOCK_INITIALIZER) 
#define NK_LOCK_T         spinlock_t
#define NK_LOCK_INIT(l)   spinlock_init(l)
#define NK_LOCK(l)        spin_lock(l)
#define NK_TRY_LOCK(l)    spin_try_lock(l)
#define NK_UNLOCK(l)      spin_unlock(l)
#define NK_LOCK_DEINIT(l) spinlock_deinit(l)
#endif

#ifdef __cplusplus
}
#endif

#endif
