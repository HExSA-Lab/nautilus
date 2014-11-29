#include <nautilus/nautilus.h>
#include <nautilus/rwlock.h>
#include <nautilus/spinlock.h>
#include <nautilus/intrinsics.h>
#include <nautilus/thread.h>
#include <lib/liballoc.h>

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
    l->readers = 0;
    spinlock_init(&l->lock);
    return 0;
}


int 
nk_rwlock_rd_lock (nk_rwlock_t * l)
{
    int flags = spin_lock_irq_save(&l->lock);
    ++l->readers;
    spin_unlock_irq_restore(&l->lock, flags);
    return 0;
}


int 
nk_rwlock_rd_unlock (nk_rwlock_t * l) 
{
    int flags = spin_lock_irq_save(&l->lock);
    /* TODO: cond_signal/broadcast */
    --l->readers;
    spin_unlock_irq_restore(&l->lock, flags);
    return 0;
}


int 
nk_rwlock_wr_lock (nk_rwlock_t * l)
{
    DEBUG_PRINT("rwlock_wr_lock\n");

    while (1) {
        spin_lock(&l->lock);

        if (likely(l->readers == 0)) {
            break;
        } else { 
            spin_unlock(&l->lock);
            /* TODO: use a condvar here */
            nk_yield();
        }
    }

    return 0;
}


int 
nk_rwlock_wr_unlock (nk_rwlock_t * l)
{
    spin_unlock(&l->lock);
    return 0;
}


uint8_t 
nk_rwlock_wr_lock_irq_save (nk_rwlock_t * l)
{
    int flags;

    while (1) {
        flags = spin_lock_irq_save(&l->lock);

        if (likely(l->readers == 0 )) {
            break;
        } else {
            spin_unlock_irq_restore(&l->lock, flags);
            /* TODO: use a condvar here */
            nk_yield();
        }
    }

    return flags;
}


int 
nk_rwlock_wr_unlock_irq_restore (nk_rwlock_t * l, uint8_t flags)
{
    spin_unlock_irq_restore(&l->lock, flags);
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

