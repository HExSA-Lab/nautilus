#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/naut_types.h>

#define SPINLOCK_INITIALIZER 0

typedef uint32_t spinlock_t;

void 
spinlock_init (volatile spinlock_t * lock);

void
spinlock_deinit (volatile spinlock_t * lock);

void
spin_lock (volatile spinlock_t * lock);


uint8_t
spin_lock_irq_save (volatile spinlock_t * lock);


void
spin_lock_nopause (volatile spinlock_t * lock);

uint8_t
spin_lock_irq_save_nopause (volatile spinlock_t * lock);


void
spin_unlock (volatile spinlock_t * lock);


void
spin_unlock_irq_restore (volatile spinlock_t * lock, uint8_t flags);

#ifdef __cplusplus
}
#endif

#endif
