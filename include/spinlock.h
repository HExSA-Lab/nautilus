#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <naut_types.h>

typedef uint32_t spinlock_t;


inline void 
spinlock_init (volatile spinlock_t * lock);


inline void
spin_lock (volatile spinlock_t * lock);


inline uint8_t
spin_lock_irq_save (volatile spinlock_t * lock);


inline void
spin_lock_nopause (volatile spinlock_t * lock);

inline uint8_t
spin_lock_irq_save_nopause (volatile spinlock_t * lock);


inline void
spin_unlock (volatile spinlock_t * lock);


inline void
spin_unlock_irq_restore (volatile spinlock_t * lock, uint8_t flags);

#ifdef __cplusplus
}
#endif

#endif
