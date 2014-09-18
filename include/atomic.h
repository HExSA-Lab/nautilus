#ifndef __ATOMIC_H__
#define __ATOMIC_H__

#define atomic_add(var, val)  __sync_fetch_and_add((volatile typeof(var)*)&(var), (val))
#define atomic_inc(var)       atomic_add(var, 1)
#define atomic_cmpswap(var, old, new) __sync_val_compare_and_swap((volatile typeof(var)*)&(var), (old), (new))


#endif
