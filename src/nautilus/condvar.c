#include <nautilus/nautilus.h>
#include <nautilus/condvar.h>
#include <nautilus/spinlock.h>
#include <nautilus/queue.h>
#include <nautilus/thread.h>
#include <nautilus/errno.h>
#include <nautilus/cpu.h>

#include <lib/liballoc.h>

#ifndef NAUT_CONFIG_DEBUG_SYNCH
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

int
nk_condvar_init (nk_condvar_t * c)
{
    DEBUG_PRINT("Condvar init\n");

    c->wait_queue = nk_thread_queue_create();
    if (!c->wait_queue) {
        ERROR_PRINT("Could not create wait queue for cond var\n");
        return -EINVAL;
    }

    spinlock_init(&c->lock);
    c->nwaiters = 0;

    return 0;
}


int
nk_condvar_destroy (nk_condvar_t * c)
{
    DEBUG_PRINT("Destroying condvar (%p)\n", (void*)c);

    int flags = spin_lock_irq_save(&c->lock);
    if (c->nwaiters != 0) {
        spin_unlock_irq_restore(&c->lock, flags);
        return -EINVAL;
    }

    nk_thread_queue_destroy(c->wait_queue);
    c->nwaiters = 0;
    spin_unlock_irq_restore(&c->lock, flags);
    return 0;
}


uint8_t
nk_condvar_wait (nk_condvar_t * c, spinlock_t * l, uint8_t flags)
{
    DEBUG_PRINT("Condvar wait on (%p) mutex=%p\n", (void*)c, (void*)l);

    /* we're about to modify shared condvar state, lock it */
    uint8_t cflags = spin_lock_irq_save(&c->lock);
        ++c->nwaiters;
    spin_unlock_irq_restore(&c->lock, cflags);

    /* now we can unlock the mutex and go to sleep */
    spin_unlock_irq_restore(l, flags);
    nk_thread_queue_sleep(c->wait_queue);

    /* we need to modify shared state again */
    cflags = spin_lock_irq_save(&c->lock);
    --c->nwaiters;
    spin_unlock_irq_restore(&c->lock, cflags); 

    /* release the mutex */
    return spin_lock_irq_save(l);
}


int 
nk_condvar_signal (nk_condvar_t * c)
{
    DEBUG_PRINT("Condvar signaling on (%p)\n", (void*)c);
    if (nk_thread_queue_wake_one(c->wait_queue) != 0) {
        ERROR_PRINT("Could not signal on condvar\n");
        return -1;
    }
    return 0;
}


int
nk_condvar_bcast (nk_condvar_t * c)
{
    DEBUG_PRINT("Condvar broadcasting on (%p)\n", (void*)c);
    if (nk_thread_queue_wake_all(c->wait_queue) != 0) {
        ERROR_PRINT("Could not broadcast on condvar\n");
        return -1;
    }
    return 0;
}


static void 
test1 (void * in, void ** out) 
{
    nk_condvar_t * c = (nk_condvar_t*)in;
    spinlock_t lock;
    spinlock_init(&lock);
    printk("test 1 is starting to wait on condvar\n");
    int flags = spin_lock_irq_save(&lock);

    nk_condvar_wait(c, &lock, flags);

    spin_unlock_irq_restore(&lock, flags);

    printk("test one is out of wait on condvar\n");
}

static void 
test2 (void * in, void ** out) 
{
    nk_condvar_t * c = (nk_condvar_t*)in;
    spinlock_t lock;
    spinlock_init(&lock);
    printk("test 2 is starting to wait on condvar\n");
    int flags = spin_lock_irq_save(&lock);

    nk_condvar_wait(c, &lock, flags);

    spin_unlock_irq_restore(&lock, flags);

    printk("test 2 is out of wait on condvar\n");
}

static void
test3 (void * in, void ** out)
{
    unsigned long long n = 1024*1024*100;
    nk_condvar_t * c = (nk_condvar_t*)in;
    printk("test 3 signalling cond var\n");
    nk_condvar_signal(c);
    printk("test 3 signalled first time\n");

    while (--n){
        io_delay();
    }

    printk("test 3 signalling 2nd time\n");
    nk_condvar_signal(c);

    while (nk_condvar_destroy(c) < 0) {
        nk_yield();
    }
    free(c);
}


void 
nk_condvar_test (void)
{

    nk_condvar_t * c = malloc(sizeof(nk_condvar_t));
    if (!c) {
        ERROR_PRINT("Could not allocate condvar\n");
        return;
    }

    nk_condvar_init(c);

    nk_thread_start(test1, c, NULL, 1, TSTACK_DEFAULT, NULL, 1);
    nk_thread_start(test2, c, NULL, 1, TSTACK_DEFAULT, NULL, 2);
    nk_thread_start(test3, c, NULL, 1, TSTACK_DEFAULT, NULL, 3);

}


