#include <nautilus.h>
#include <rwlock.h>
#include <spinlock.h>
#include <intrinsics.h>
#include <thread.h>
#include <lib/liballoc.h>

/*
 *
 * TODO: right now we only support a reader-preferred
 * variant
 *
 */

extern void yield(void);

int
rwlock_init (rwlock_t * l)
{
    l->readers = 0;
    spinlock_init(&l->lock);
    return 0;
}


int 
rwlock_rd_lock (rwlock_t * l)
{
    int flags = spin_lock_irq_save(&l->lock);
    ++l->readers;
    spin_unlock_irq_restore(&l->lock, flags);
    return 0;
}


int 
rwlock_rd_unlock (rwlock_t * l) 
{
    int flags = spin_lock_irq_save(&l->lock);
    /* TODO: cond_signal/broadcast */
    --l->readers;
    spin_unlock_irq_restore(&l->lock, flags);
    return 0;
}


int 
rwlock_wr_lock (rwlock_t * l)
{
    while (1) {
        spin_lock(&l->lock);

        if (likely(l->readers == 0)) {
            break;
        } else { 
            spin_unlock(&l->lock);
            /* TODO: use a condvar here */
            yield();
        }
    }

    return 0;
}


int 
rwlock_wr_unlock (rwlock_t * l)
{
    spin_unlock(&l->lock);
    return 0;
}


uint8_t 
rwlock_wr_lock_irq_save (rwlock_t * l)
{
    int flags;

    while (1) {
        flags = spin_lock_irq_save(&l->lock);

        if (likely(l->readers == 0 )) {
            break;
        } else {
            spin_unlock_irq_restore(&l->lock, flags);
            /* TODO: use a condvar here */
            yield();
        }
    }

    return flags;
}


int 
rwlock_wr_unlock_irq_restore (rwlock_t * l, uint8_t flags)
{
    spin_unlock_irq_restore(&l->lock, flags);
    return 0;
}


static void 
reader1 (void * in, void ** out) 
{
    rwlock_t * rl = (rwlock_t*)in;
    int n = 1000;
    int i = 0;
    rwlock_rd_lock(rl);
    i++;
    rwlock_rd_unlock(rl);

}

static void
reader2 (void * in, void ** out)
{
    int n = 2000;
    int i = 0;
    rwlock_t * rl = (rwlock_t*)in;
    rwlock_rd_lock(rl);
    i++;
    rwlock_rd_unlock(rl);
}

static void 
writer (void * in, void ** out)
{
    rwlock_t * rl = (rwlock_t*)in;
    int n = 0;
    uint8_t flags = rwlock_wr_lock_irq_save(rl);
    n++;
    rwlock_wr_unlock_irq_restore(rl, flags);
}


void
rwlock_test (void) 
{
    rwlock_t * rl = NULL;
    rl = malloc(sizeof(rwlock_t));
    if (!rl) {
        ERROR_PRINT("Could not allocate rwlock\n");
        return;
    }

    rwlock_init(rl);

    thread_start(reader1, rl, NULL, 1, TSTACK_DEFAULT, NULL, 1);
    thread_start(reader2, rl, NULL, 1, TSTACK_DEFAULT, NULL, 2);
    thread_start(writer, rl, NULL, 1, TSTACK_DEFAULT, NULL, 3);

}

