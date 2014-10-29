#include <nautilus.h>
#include <condvar.h>
#include <spinlock.h>
#include <queue.h>
#include <thread.h>
#include <errno.h>
#include <cpu.h>

#include <lib/liballoc.h>


int
condvar_init (condvar_t * c)
{
    c->wait_queue = thread_queue_create();
    if (!c->wait_queue) {
        ERROR_PRINT("Could not create wait queue for cond var\n");
        return -EINVAL;
    }

    spinlock_init(&c->lock);
    c->nwaiters = 0;

    return 0;
}


int
condvar_destroy (condvar_t * c)
{
    int flags = spin_lock_irq_save(&c->lock);
    if (c->nwaiters != 0) {
        spin_unlock_irq_restore(&c->lock, flags);
        return -EINVAL;
    }

    thread_queue_destroy(c->wait_queue);
    c->nwaiters = 0;
    spin_unlock_irq_restore(&c->lock, flags);
    return 0;
}


uint8_t
condvar_wait (condvar_t * c, spinlock_t * l, uint8_t flags)
{

    /* we're about to modify shared condvar state, lock it */
    uint8_t cflags = spin_lock_irq_save(&c->lock);
        ++c->nwaiters;
    spin_unlock_irq_restore(&c->lock, cflags);

    /* now we can unlock the mutex and go to sleep */
    spin_unlock_irq_restore(l, flags);
    thread_queue_sleep(c->wait_queue);

    /* we need to modify shared state again */
    cflags = spin_lock_irq_save(&c->lock);
    --c->nwaiters;
    spin_unlock_irq_restore(&c->lock, cflags); 

    /* release the mutex */
    return spin_lock_irq_save(l);
}


int 
condvar_signal (condvar_t * c)
{
    thread_queue_wake_one(c->wait_queue);
    return 0;
}


int
condvar_bcast (condvar_t * c)
{
    thread_queue_wake_all(c->wait_queue);
    return 0;
}


static void 
test1 (void * in, void ** out) 
{
    condvar_t * c = (condvar_t*)in;
    spinlock_t lock;
    spinlock_init(&lock);
    printk("test 1 is starting to wait on condvar\n");
    int flags = spin_lock_irq_save(&lock);

    condvar_wait(c, &lock, flags);

    spin_unlock_irq_restore(&lock, flags);

    printk("test one is out of wait on condvar\n");
}

static void 
test2 (void * in, void ** out) 
{
    condvar_t * c = (condvar_t*)in;
    spinlock_t lock;
    spinlock_init(&lock);
    printk("test 2 is starting to wait on condvar\n");
    int flags = spin_lock_irq_save(&lock);

    condvar_wait(c, &lock, flags);

    spin_unlock_irq_restore(&lock, flags);

    printk("test 2 is out of wait on condvar\n");
}

static void
test3 (void * in, void ** out)
{
    unsigned long long n = 1024*1024*100;
    condvar_t * c = (condvar_t*)in;
    printk("test 3 signalling cond var\n");
    condvar_signal(c);
    printk("test 3 signalled first time\n");

    while (--n){
        io_delay();
    }

    printk("test 3 signalling 2nd time\n");
    condvar_signal(c);

    while (condvar_destroy(c) < 0) {
        yield();
    }
    free(c);
}


void 
condvar_test (void)
{

    condvar_t * c = malloc(sizeof(condvar_t));
    if (!c) {
        ERROR_PRINT("Could not allocate condvar\n");
        return;
    }

    condvar_init(c);

    thread_start(test1, c, NULL, 1, TSTACK_DEFAULT, NULL, 1);
    thread_start(test2, c, NULL, 1, TSTACK_DEFAULT, NULL, 2);
    thread_start(test3, c, NULL, 1, TSTACK_DEFAULT, NULL, 3);

}


