#include <nautilus/nautilus.h>
#include <nautilus/condvar.h>
#include <nautilus/spinlock.h>
#include <nautilus/queue.h>
#include <nautilus/thread.h>
#include <nautilus/errno.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <nautilus/atomic.h>

#include <dev/apic.h>
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
nk_condvar_wait (nk_condvar_t * c, spinlock_t * l)
{
    NK_PROFILE_ENTRY();
    DEBUG_PRINT("Condvar wait on (%p) mutex=%p\n", (void*)c, (void*)l);

    atomic_inc(c->nwaiters);

    /* now we can unlock the mutex and go to sleep */
    spin_unlock(l);
    nk_thread_queue_sleep(c->wait_queue);

    atomic_dec(c->nwaiters);

    /* reacquire lock */
    spin_lock(l);
    NK_PROFILE_EXIT();
    return 0;
}


int 
nk_condvar_signal (nk_condvar_t * c)
{
    uint8_t flags;
    NK_PROFILE_ENTRY();
    flags = irq_disable_save();
    DEBUG_PRINT("Condvar signaling on (%p)\n", (void*)c);
    if (unlikely(nk_thread_queue_wake_one(c->wait_queue) != 0)) {
        ERROR_PRINT("Could not signal on condvar\n");
        return -1;
    }
    /* broadcast a timer interrupt to everyone */
    apic_bcast_ipi(per_cpu_get(apic), 0xf0);
    irq_enable_restore(flags);
    NK_PROFILE_EXIT();
    return 0;
}


int
nk_condvar_bcast (nk_condvar_t * c)
{
    uint8_t flags;
    NK_PROFILE_ENTRY();
    flags = irq_disable_save();
    DEBUG_PRINT("Condvar broadcasting on (%p)\n", (void*)c);
    if (unlikely(nk_thread_queue_wake_all(c->wait_queue) != 0)) {
        ERROR_PRINT("Could not broadcast on condvar\n");
        return -1;
    }
    /* broadcast a timer interrupt to everyone */
    apic_bcast_ipi(per_cpu_get(apic), 0xf0);
    irq_enable_restore(flags);
    NK_PROFILE_EXIT();
    return 0;
}


static void 
test1 (void * in, void ** out) 
{
    nk_condvar_t * c = (nk_condvar_t*)in;
    spinlock_t lock;
    spinlock_init(&lock);
    printk("test 1 is starting to wait on condvar\n");
    spin_lock(&lock);

    nk_condvar_wait(c, &lock);

    spin_unlock(&lock);

    printk("test one is out of wait on condvar\n");
}

static void 
test2 (void * in, void ** out) 
{
    nk_condvar_t * c = (nk_condvar_t*)in;
    spinlock_t lock;
    spinlock_init(&lock);
    printk("test 2 is starting to wait on condvar\n");
    spin_lock(&lock);

    nk_condvar_wait(c, &lock);

    spin_unlock(&lock);

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


