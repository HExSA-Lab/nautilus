#ifndef __BENCHMARK_H__
#define __BENCHMARK_H__

#ifdef __USER

#define _GNU_SOURCE
#define PRINT printf
#define THREAD_T        pthread_t
#define FUNC_HDR        (void * in)
#define FUNC_TYPE       void*
#define RETURN          return NULL
#define JOIN_FUNC(x, y) pthread_join(x, y)
#define LOCK_T          pthread_spinlock_t
#define LOCK_INIT(x)    pthread_spin_init(x, 0)
#define LOCK_DEINIT(x)  pthread_spin_destroy(x)
#define LOCK(x)         pthread_spin_lock(x)
#define UNLOCK(x)       pthread_spin_unlock(x)
#define MUTEX_T         pthread_mutex_t 
#define MUTEX_INIT(x)   pthread_mutex_init(x, NULL)
#define MUTEX_LOCK(x)   pthread_mutex_lock(x)
#define MUTEX_UNLOCK(x) pthread_mutex_unlock(x)
#define MUTEX_DEINIT(x) pthread_mutex_destroy(x)
#define COND_T          pthread_cond_t
#define COND_INIT(x)    pthread_cond_init(x, NULL)
#define COND_WAIT(x, m) pthread_cond_wait(x, m)
#define COND_SIG(x)     pthread_cond_signal(x)
#define COND_BCAST(x)   pthread_cond_broadcast(x)
#define BARRIER_T       pthread_barrier_t
#define BARRIER_WAIT(x) pthread_barrier_wait(x)
#define BARRIER_INIT(b, c) pthread_barrier_init(b, NULL, c)
#define BARRIER_DESTROY(b) pthread_barrier_destroy(b)
#define YIELD()         pthread_yield()
#define DELAY(x)        usleep(x)
#define GETCPU()        sched_getcpu()

typedef unsigned long ulong_t;

#else

#define PRINT           printk
#define THREAD_T        nk_thread_id_t
#define LOCK_T          spinlock_t
#define FUNC_HDR        (void * in, void ** out)
#define FUNC_TYPE       void
#define RETURN          return
#define JOIN_FUNC(x, y) nk_join(x, y)
#define LOCK_INIT(x)    spinlock_init(x)
#define LOCK_DEINIT(x)  spinlock_deinit(x)
#define LOCK(x)         spin_lock(x)
#define UNLOCK(x)       spin_unlock(x)
#define MUTEX_T         spinlock_t
#define MUTEX_INIT(x)   spinlock_init(x)
#define MUTEX_LOCK(x)   spin_lock(x)
#define MUTEX_UNLOCK(x) spin_unlock(x)
#define MUTEX_DEINIT(x) spinlock_deinit(x)
#define COND_T          nk_condvar_t
#define COND_INIT(x)    nk_condvar_init(x)
#define COND_WAIT(x, m) nk_condvar_wait(x, m)
#define COND_SIG(x)     nk_condvar_signal(x)
#define COND_BCAST(x)   nk_condvar_bcast(x)
#define BARRIER_T       nk_barrier_t
#define BARRIER_WAIT(x) nk_barrier_wait(x)
#define BARRIER_INIT(b, c) nk_barrier_init(b, c)
#define BARRIER_DESTROY(b) nk_barrier_destroy(b)
#define YIELD()         nk_yield()
#define DELAY(x)        udelay(x)
#define GETCPU()        my_cpu_id()

#endif

static inline void 
bset (unsigned int nr, unsigned long * addr)
{
        asm volatile("lock bts %1, %0"
            : "+m" (*(volatile long *)(addr)) : "Ir" (nr) : "memory");
}

#endif
