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


static inline void*
xchg64 (void ** dst, void * newval)
{
    void * ret = newval;
    asm volatile("xchgq %0, %1"
                 : "+r" (ret), "+m" (*dst)
                 : 
                 : "memory", "cc");
    return ret;
}

#endif
