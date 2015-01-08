#include <nautilus/spinlock.h>
#include <nautilus/irq.h>

void 
spinlock_init (volatile spinlock_t * lock) 
{
    *lock = 0;
}


void
spinlock_deinit (volatile spinlock_t * lock) 
{
    *lock = 0;
}


inline void __always_inline
spin_lock (volatile spinlock_t * lock)
{
    NK_PROFILE_ENTRY();
    
    PAUSE_WHILE(__sync_lock_test_and_set(lock, 1));

    NK_PROFILE_EXIT();
}


uint8_t
spin_lock_irq_save (volatile spinlock_t * lock)
{
    //NK_PROFILE_ENTRY();
    uint8_t flags = irq_disable_save();
    PAUSE_WHILE(__sync_lock_test_and_set(lock, 1));
    return flags;
    //NK_PROFILE_EXIT();
}


void
spin_lock_nopause (volatile spinlock_t * lock)
{
    while (__sync_lock_test_and_set(lock, 1)) {
        /* nothing */
    }
}


uint8_t
spin_lock_irq_save_nopause (volatile spinlock_t * lock)
{
    uint8_t flags = irq_disable_save();
    while (__sync_lock_test_and_set(lock, 1)) {
        /* nothing */
    }
    return flags;
}


inline void __always_inline
spin_unlock (volatile spinlock_t * lock)
{
    NK_PROFILE_ENTRY();
    __sync_lock_release(lock);
    NK_PROFILE_EXIT();
}


void
spin_unlock_irq_restore (volatile spinlock_t * lock, uint8_t flags)
{
    //NK_PROFILE_ENTRY();
    __sync_lock_release(lock);
    irq_enable_restore(flags);
    //NK_PROFILE_EXIT();
}
