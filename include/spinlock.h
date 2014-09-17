#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include <types.h>

typedef uint32_t spinlock_t;


static inline void 
spinlock_init (volatile spinlock_t * lock) 
{
    *lock = 0;
}


static inline void
spin_lock (volatile spinlock_t * lock)
{
    while (__sync_lock_test_and_set(lock, 1)) {
        asm volatile ("pause");
    }
}


static inline void
spin_lock_nopause (volatile spinlock_t * lock)
{
    while (__sync_lock_test_and_set(lock, 1)) {
        /* nothing */
    }
}


static inline void
spin_unlock (volatile spinlock_t * lock)
{
    __sync_lock_release(lock);
}


#endif
