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
#include <nautilus/nautilus.h>
#include <nautilus/rwlock.h>
#include <nautilus/spinlock.h>
#include <nautilus/intrinsics.h>
#include <nautilus/thread.h>
#include <nautilus/mm.h>

#ifndef NAUT_CONFIG_DEBUG_SYNCH
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

/*
 *
 * TODO: right now we only support a reader-preferred
 * variant
 *
 */

extern void nk_yield(void);

int
nk_rwlock_init (nk_rwlock_t * l)
{
    DEBUG_PRINT("rwlock init (%p)\n", (void*)l);
    l->readers = 0;
    spinlock_init(&l->lock);
    return 0;
}


int 
nk_rwlock_rd_lock (nk_rwlock_t * l)
{
    NK_PROFILE_ENTRY();
    DEBUG_PRINT("rwlock read lock: %p\n", (void*)l);
    int flags = spin_lock_irq_save(&l->lock);
    ++l->readers;
    spin_unlock_irq_restore(&l->lock, flags);
    NK_PROFILE_EXIT();
    return 0;
}


int 
nk_rwlock_rd_unlock (nk_rwlock_t * l) 
{
    NK_PROFILE_ENTRY();
    DEBUG_PRINT("rwlock read unlock: %p\n", (void*)l);
    int flags = spin_lock_irq_save(&l->lock);
    --l->readers;
    spin_unlock_irq_restore(&l->lock, flags);
    NK_PROFILE_EXIT();
    return 0;
}


int 
nk_rwlock_wr_lock (nk_rwlock_t * l)
{
    NK_PROFILE_ENTRY();
    DEBUG_PRINT("rwlock write lock: %p\n", (void*)l);

    while (1) {
        spin_lock(&l->lock);

        if (likely(l->readers == 0)) {
            break;
        } else { 
            spin_unlock(&l->lock);
            /* TODO: we should yield if we're not spread across cores */
        }
    }

    NK_PROFILE_EXIT();
    return 0;
}


int 
nk_rwlock_wr_unlock (nk_rwlock_t * l)
{
    NK_PROFILE_ENTRY();
    DEBUG_PRINT("rwlock write unlock: %p\n", (void*)l);
    spin_unlock(&l->lock);
    NK_PROFILE_EXIT();
    return 0;
}


uint8_t 
nk_rwlock_wr_lock_irq_save (nk_rwlock_t * l)
{
    int flags;
    NK_PROFILE_ENTRY();
    DEBUG_PRINT("rwlock write lock (irq): %p\n", (void*)l);

    while (1) {
        flags = spin_lock_irq_save(&l->lock);

        if (likely(l->readers == 0 )) {
            break;
        } else {
            spin_unlock_irq_restore(&l->lock, flags);
        }
    }

    NK_PROFILE_EXIT();
    return flags;
}


int 
nk_rwlock_wr_unlock_irq_restore (nk_rwlock_t * l, uint8_t flags)
{
    NK_PROFILE_ENTRY();
    DEBUG_PRINT("rwlock write unlock (irq): %p\n", (void*)l);
    spin_unlock_irq_restore(&l->lock, flags);
    NK_PROFILE_EXIT();
    return 0;
}


static void 
reader1 (void * in, void ** out) 
{
    nk_rwlock_t * rl = (nk_rwlock_t*)in;
    int n = 1000;
    int i = 0;
    nk_rwlock_rd_lock(rl);
    i++;
    nk_rwlock_rd_unlock(rl);

}

static void
reader2 (void * in, void ** out)
{
    int n = 2000;
    int i = 0;
    nk_rwlock_t * rl = (nk_rwlock_t*)in;
    nk_rwlock_rd_lock(rl);
    i++;
    nk_rwlock_rd_unlock(rl);
}

static void 
writer (void * in, void ** out)
{
    nk_rwlock_t * rl = (nk_rwlock_t*)in;
    int n = 0;
    uint8_t flags = nk_rwlock_wr_lock_irq_save(rl);
    n++;
    nk_rwlock_wr_unlock_irq_restore(rl, flags);
}


void
nk_rwlock_test (void) 
{
    nk_rwlock_t * rl = NULL;
    rl = malloc(sizeof(nk_rwlock_t));
    if (!rl) {
        ERROR_PRINT("Could not allocate rwlock\n");
        return;
    }

    nk_rwlock_init(rl);

    nk_thread_start(reader1, rl, NULL, 1, TSTACK_DEFAULT, NULL, 1);
    nk_thread_start(reader2, rl, NULL, 1, TSTACK_DEFAULT, NULL, 2);
    nk_thread_start(writer, rl, NULL, 1, TSTACK_DEFAULT, NULL, 3);

}

