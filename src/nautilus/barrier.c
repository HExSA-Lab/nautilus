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
#include <nautilus/barrier.h>
#include <nautilus/cpu.h>
#include <nautilus/atomic.h>
#include <nautilus/errno.h>
#include <nautilus/intrinsics.h>
#include <nautilus/thread.h>
#include <nautilus/mm.h>


#ifndef NAUT_CONFIG_DEBUG_BARRIER
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

static inline void
bspin_lock (volatile int * lock)
{
        while (__sync_lock_test_and_set(lock, 1));
}

static inline void
bspin_unlock (volatile int * lock)
{
        __sync_lock_release(lock);
}

/* 
 * this is where cores will come in 
 * to arrive at a barrier. We're in interrupt
 * context here, but that's fine since we
 * wan't interrupts off in the wait anyhow
 */
static void
barrier_xcall_handler (void * arg) 
{
    nk_core_barrier_arrive();
}


/*
 * nk_barrier_init
 *
 * initialize a thread barrier. This 
 * version is more or less a POSIX barrier
 *
 * @barrier: the barrier to initialize
 * @count: the number of participants
 *
 * returns 0 on succes, -EINVAL on error
 *
 */
int 
nk_barrier_init (nk_barrier_t * barrier, uint32_t count) 
{
    int ret = 0;
    memset(barrier, 0, sizeof(nk_barrier_t));
    barrier->lock = 0;

    if (unlikely(count == 0)) {
        ERROR_PRINT("Barrier count must be greater than 0\n");
        return -EINVAL;
    }


    DEBUG_PRINT("Initializing barier, barrier at %p, count=%u\n", (void*)barrier, count);
    barrier->init_count = count;
    barrier->remaining  = count;

    return 0;
}


/*
 * nk_barrier_destroy
 *
 * destroy a thread barrier
 *
 * @barrier: the barrier to destroy
 *
 * returns 0 on succes, -EINVAL on error
 *
 */
int 
nk_barrier_destroy (nk_barrier_t * barrier)
{
    int res;

    if (!barrier) {
        return -EINVAL;
    }

    DEBUG_PRINT("Destroying barrier (%p)\n", (void*)barrier);

    bspin_lock(&barrier->lock);
    
    if (likely(barrier->remaining == barrier->init_count)) {
        res = 0;
    } else {
        /* someone is still waiting at the barrier? */
        ERROR_PRINT("Someone still waiting at barrier, cannot destroy\n");
        res = -EINVAL;
    }
    bspin_unlock(&barrier->lock);

    return res;
}


/*
 * nk_barrier_wait
 *
 * wait at a thread barrier
 *
 * @barrier: the barrier to wait at
 *
 * returns 0 to all threads but the last. The last thread
 * out of the barrier will return NK_BARRIER_LAST. This
 * is useful for having one thread in charge of cleaning 
 * the barrier up. Again, similar to POSIX
 *
 */
int 
nk_barrier_wait (nk_barrier_t * barrier) 
{
    int res = 0;

    DEBUG_PRINT("Thread (%p) entering barrier (%p)\n", (void*)get_cur_thread(), (void*)barrier);

    bspin_lock(&barrier->lock);

    if (--barrier->remaining == 0) {
        res = NK_BARRIER_LAST;
        atomic_cmpswap(barrier->notify, 0, 1);
    } else {
        bspin_unlock(&barrier->lock);
        BARRIER_WHILE(barrier->notify != 1);
    }

    DEBUG_PRINT("Thread (%p) exiting barrier (%p)\n", (void*)get_cur_thread(), (void*)barrier);
    
    register unsigned init_count = barrier->init_count;

    if (atomic_inc_val(barrier->remaining) == init_count) {
        atomic_cmpswap(barrier->notify, 1, 0); 
        bspin_unlock(&barrier->lock);
    }
    
    return res;
}


/* 
 * The below functions are for CORES. *NOT* 
 * threads. The behavior is undefined if 
 * You use these on two threads running on the same core!
 * DO NOT DO IT
 *
 */


/*
 * nk_core_barrier_raise
 *
 * Raises a barrier for all other 
 * cores in the system. The model here is the core calling
 * this function will *not* wait at the barrier (unless it's
 * already been rasised) and is instead expected to 
 * wait() on the other cores to arrive, do some things, 
 * then lower() the barrier. This core (or some core not
 * waiting at the barrier) must call barier_lower(). The
 * cores will *not* automatically be released when they all
 * arrive at the barrier. This is the main difference 
 * from the thread barrier above.
 *
 * returns 0 on success, -EINVAL on error
 *
 */
int 
nk_core_barrier_raise (void) 
{
    nk_barrier_t * barrier = per_cpu_get(system)->core_barrier;
    uint8_t iownit = 0;
    uint8_t flags;
    unsigned i;
    int res = 0;

    DEBUG_PRINT("Core %u raising core barrier\n", my_cpu_id());

    flags = spin_lock_irq_save(&barrier->lock);

    if (barrier->active == 0) {
        barrier->active     = 1;
        barrier->notify     = 0;
        barrier->remaining  = barrier->init_count; // num cores
        iownit = 1;
    }

    spin_unlock_irq_restore(&barrier->lock, flags);

    if (iownit) {

        // we will not wait at the barrier, but we'll
        // decrement the waiting count
        atomic_dec(barrier->remaining);

        cpu_id_t me = my_cpu_id();

        // force other cores to wait at the barrier
        for (i = 0; i < per_cpu_get(system)->num_cpus; i++) {

            if (i == me) {
                continue;
            }

            if (smp_xcall(i,
                        barrier_xcall_handler,
                        NULL, // no need for args
                        0)    // blocking would be catastrophic here
                    != 0) {
                ERROR_PRINT("Could not force cpu %u to wait at barrier\n", i);
                return -EINVAL;
            }

        }

    } else {

        // if someone else raised one already,
        // we'll just wait on it. This is probably
        // an error though
        nk_core_barrier_arrive();
        res = -EINVAL;

    }

    return res;
}


/*
 * nk_core_barrier_lower
 *
 * lower a barrier, allowing cores waiting at it
 * to proceed
 *
 * returns 0 on success, -EINVAL on error
 *
 */
int
nk_core_barrier_lower (void)
{
    nk_barrier_t * barrier = per_cpu_get(system)->core_barrier;

    DEBUG_PRINT("Core %u lowering barrier\n", my_cpu_id());

    if (!barrier->active) {
        return -EINVAL;
    }

    while (atomic_cmpswap(barrier->notify, 0, 1) == 0);
    barrier->active = 0;

    return 0;
}


/* 
 * nk_core_barrier_wait
 *
 * wait for all other cores to arrive
 * at the core barrier. This should
 * be called before lowering a barrier
 *
 * returns 0 on success, -EINVAL on error
 */
int
nk_core_barrier_wait (void)
{
    nk_barrier_t * barrier = per_cpu_get(system)->core_barrier;

    DEBUG_PRINT("Core %u waiting on other cores to arrive at core barrier\n", my_cpu_id());

    if (!barrier->active) {
        return -EINVAL;
    }

    while (barrier->remaining != 0) {
        nk_yield();
    }

    return 0;
}


/*
 * nk_core_barrier_arrive
 *
 *
 * waits at the core barrier. similar to nk_barrier_wait 
 * but we don't spin, we yield. We will only 
 * be released when an external agent notifies all
 * the cores
 *
 * returns 0 on success, -EINVAL on error
 *
 */
int 
nk_core_barrier_arrive (void)
{
    nk_barrier_t * barrier = per_cpu_get(system)->core_barrier;

    if (!barrier->active) {
        return -EINVAL;
    }

    DEBUG_PRINT("Core %u arriving at core barrier\n", my_cpu_id());

    atomic_dec(barrier->remaining);

    while (barrier->notify != 1) {
        nk_yield();
    }

    return 0;
}

/***** BARRIER TESTS ******/

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
 * NOTE: this test assumes that there are at least 3 CPUs on 
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

