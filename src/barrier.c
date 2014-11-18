#include <nautilus.h>
#include <barrier.h>
#include <cpu.h>
#include <atomic.h>
#include <intrinsics.h>
#include <thread.h>

#include <lib/liballoc.h>

#ifndef NAUT_CONFIG_DEBUG_SYNCH
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

int 
nk_barrier_init (nk_barrier_t * barrier, uint32_t count) 
{
    int ret = 0;
    memset(barrier, 0, sizeof(nk_barrier_t));
    spinlock_init(&barrier->lock);

    if (unlikely(count == 0)) {
        ERROR_PRINT("Barrier count must be greater than 0\n");
        return -1;
    }


    DEBUG_PRINT("Initializing barier, barrier at %p, count=%u\n", (void*)barrier, count);
    barrier->init_count = count;
    barrier->remaining  = count;

    return 0;
}


int 
nk_barrier_destroy (nk_barrier_t * barrier)
{
    int res;

    if (!barrier) {
        return -1;
    }

    DEBUG_PRINT("Destroying barrier (%p)\n", (void*)barrier);

    spin_lock(&barrier->lock);
    
    if (likely(barrier->remaining == barrier->init_count)) {
        res = 0;
    } else {
        /* someone is still waiting at the barrier? */
        ERROR_PRINT("Someone still waiting at barrier, cannot destroy\n");
        res = -1;
    }
    spin_unlock(&barrier->lock);

    return res;
}


int 
nk_barrier_wait (nk_barrier_t * barrier) 
{
    int res = 0;

    DEBUG_PRINT("Thread (%p) entering barrier (%p)\n", (void*)get_cur_thread(), (void*)barrier);

    spin_lock(&barrier->lock);

    if (--barrier->remaining == 0) {
        res = NK_BARRIER_LAST;
        while (atomic_cmpswap(barrier->notify, 0, 1) == 0);
    } else {
        spin_unlock(&barrier->lock);
        PAUSE_WHILE(barrier->notify != 1);
    }

    DEBUG_PRINT("Thread (%p) exiting barrier (%p)\n", (void*)get_cur_thread(), (void*)barrier);
    
    register unsigned init_count = barrier->init_count;

    if (atomic_inc_val(barrier->remaining) == init_count) {
        spin_unlock(&barrier->lock);
    }
    
    return res;
}


static void
barrier_func1 (void * in, void ** out)
{
    nk_barrier_t * b = (nk_barrier_t *)in;
    nk_barrier_wait(b);
}


static void
barrier_func2 (void * in, void ** out)
{
    uint64_t n = 100;
    nk_barrier_t * b = (nk_barrier_t *)in;
    while (--n) {
        io_delay();
    }

    nk_barrier_wait(b);
}


/* 
 *
 * NOTE: this assumes that there are at least 3 CPUs on 
 * the machine
 *
 */
void nk_barrier_test(void)
{
    nk_barrier_t * b;
    b = malloc(sizeof(nk_barrier_t));
    if (!b) {
        ERROR_PRINT("could not allocate barrier\n");
        return;
    }

    nk_barrier_init(b, 3);
    nk_thread_start(barrier_func1, b, NULL, 1, TSTACK_DEFAULT, NULL, 1);
    nk_thread_start(barrier_func2, b, NULL, 1, TSTACK_DEFAULT, NULL, 2);

    nk_barrier_wait(b);

    printk("Barrier test successful\n");
    nk_barrier_destroy(b);
    free(b);
}

