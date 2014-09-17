#ifndef __ATOMIC_H__
#define __ATOMIC_H__

#define atomic_add(var, val)  __sync_fetch_and_add((volatile typeof(var)*)&(var), (val))
#define atomic_inc(var)       atomic_add(var, 1)


#endif
