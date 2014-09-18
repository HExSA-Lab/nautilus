#include <spinlock.h>

#include <irq.h>

inline void 
spinlock_init (volatile spinlock_t * lock) 
{
    *lock = 0;
}


inline void
spin_lock (volatile spinlock_t * lock)
{
    while (__sync_lock_test_and_set(lock, 1)) {
        asm volatile ("pause");
    }
}


inline uint8_t
spin_lock_irq_save (volatile spinlock_t * lock)
{
    uint8_t flags = irq_disable_save();
    while (__sync_lock_test_and_set(lock, 1)) {
        asm volatile ("pause");
    }
    return flags;
}


inline void
spin_lock_nopause (volatile spinlock_t * lock)
{
    while (__sync_lock_test_and_set(lock, 1)) {
        /* nothing */
    }
}


inline uint8_t
spin_lock_irq_save_nopause (volatile spinlock_t * lock)
{
    uint8_t flags = irq_disable_save();
    while (__sync_lock_test_and_set(lock, 1)) {
        /* nothing */
    }
    return flags;
}


inline void
spin_unlock (volatile spinlock_t * lock)
{
    __sync_lock_release(lock);
}


inline void
spin_unlock_irq_restore (volatile spinlock_t * lock, uint8_t flags)
{
    __sync_lock_release(lock);
    irq_enable_restore(flags);
}
