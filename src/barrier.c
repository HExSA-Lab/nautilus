#include <nautilus.h>
#include <barrier.h>
#include <cpu.h>
#include <atomic.h>
#include <thread.h>

#include <lib/liballoc.h>


int 
barrier_init (barrier_t * barrier, uint32_t count) 
{
    int ret = 0;
    memset(barrier, 0, sizeof(barrier_t));
    spinlock_init(&barrier->lock);

    if (count == 0) {
        ERROR_PRINT("Barrier count must be greater than 0\n");
        return -1;
    }

    barrier->init_count = count;
    barrier->remaining  = count;

    return 0;
}


int 
barrier_wait (barrier_t * barrier) 
{
    int res = 0;
    int flags;

    flags = spin_lock_irq_save(&barrier->lock);

    if (--barrier->remaining == 0) {
        res = BARRIER_LAST;
        atomic_cmpswap(barrier->notify, 0, 1);
    } else {
        spin_unlock_irq_restore(&barrier->lock, flags);
        PAUSE_WHILE(barrier->notify == 0);
    }
    
    register unsigned init_count = barrier->init_count;

    if (atomic_inc_val(barrier->remaining) == init_count) {
        spin_unlock_irq_restore(&barrier->lock, flags);
    }
    
    return res;
}


static void
barrier_func1 (void * in, void ** out)
{
    barrier_t * b = (barrier_t *)in;
    barrier_wait(b);
}


static void
barrier_func2 (void * in, void ** out)
{
    uint64_t n = 100;
    barrier_t * b = (barrier_t *)in;
    while (--n) {
        io_delay();
    }

    barrier_wait(b);
}


/* 
 *
 * NOTE: this assumes that there are at least 3 CPUs on 
 * the machine
 *
 */
void barrier_test(void)
{
    barrier_t * b;
    b = malloc(sizeof(barrier_t));
    if (!b) {
        ERROR_PRINT("could not allocate barrier\n");
        return;
    }

    barrier_init(b, 3);
    thread_start(barrier_func1, b, NULL, 1, TSTACK_DEFAULT, NULL, 1);
    thread_start(barrier_func2, b, NULL, 1, TSTACK_DEFAULT, NULL, 2);

    barrier_wait(b);

    printk("Barrier test successful\n");
}

