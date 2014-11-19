#ifndef __ATOMIC_H__
#define __ATOMIC_H__

#define atomic_add(var, val)  __sync_fetch_and_add((volatile typeof(var)*)&(var), (val)) /* note: returns the old value */
#define atomic_sub(var, val)  __sync_fetch_and_sub((volatile typeof(var)*)&(var), (val)) /* note: returns the old value */
#define atomic_or(var, val)  __sync_fetch_and_or((volatile typeof(var)*)&(var), (val)) /* note: returns the old value */
#define atomic_xor(var, val)  __sync_fetch_and_xor((volatile typeof(var)*)&(var), (val)) /* note: returns the old value */
#define atomic_and(var, val)  __sync_fetch_and_and((volatile typeof(var)*)&(var), (val)) /* note: returns the old value */
#define atomic_nand(var, val)  __sync_fetch_and_nand((volatile typeof(var)*)&(var), (val)) /* note: returns the old value */

#define atomic_inc(var)       atomic_add((var), 1) 
#define atomic_dec(var)       atomic_sub((var), 1)

/* these return the *NEW* value */
#define atomic_inc_val(var)  (atomic_add((var), 1) + 1)
#define atomic_dec_val(var)  (atomic_sub((var), 1) - 1)

#define atomic_cmpswap(var, old, new) __sync_val_compare_and_swap((volatile typeof(var)*)&(var), (old), (new))


#endif
