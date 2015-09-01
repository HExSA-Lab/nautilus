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
#include <nautilus/nautilus.h>
#include <nautilus/ticketlock.h>


void
nk_ticket_lock_init (nk_ticket_lock_t * l)
{
    l->val = 0;
}


void
nk_ticket_lock_deinit (nk_ticket_lock_t * l)
{
    l->val = 0;
}


inline void __always_inline
nk_ticket_lock (nk_ticket_lock_t * l) 
{
    NK_PROFILE_ENTRY();

    asm volatile ("movw $1, %%ax\n\t"
                  "lock xaddw %%ax, %[_users]\n\t"
                  "1:\n\t"
                  "cmpw %%ax, %[_ticket]\n\t"
                  "jne 1b"
                  : /* no outputs */
                  : [_users] "m" (l->lock.users),
                    [_ticket] "m" (l->lock.ticket)
                  : "ax", "memory");

    NK_PROFILE_EXIT();
}


inline void __always_inline
nk_ticket_unlock (nk_ticket_lock_t * l)
{
    NK_PROFILE_ENTRY();

#ifndef NAUT_CONFIG_XEON_PHI
    asm volatile ("mfence\n\t"
#else 
    asm volatile (
#endif
                  "addw $1, %[_ticket]"
                  : /* no outputs */
                  : [_ticket] "m" (l->lock.ticket)
                  : "memory");

    NK_PROFILE_EXIT();
}


static inline uint32_t
cmpxchg32 (void * m, uint32_t old, uint32_t new)
{
    uint32_t ret;
    asm volatile ("movl %[_old], %%eax\n\t"
                  "cmpxchgl %[_new], %[_m]\n\t"
                  "movl %%eax, %[_out]"
                  : [_out] "=r" (ret)
                  : [_old] "r" (old),
                    [_new] "r" (new),
                    [_m]   "m" (m)
                  : "rax", "memory");
    return ret;
}


inline int __always_inline
nk_ticket_trylock (nk_ticket_lock_t * l)
{
    NK_PROFILE_ENTRY();

    uint16_t me = l->lock.users;
    uint16_t menew = me + 1;
    uint32_t cmp = ((uint32_t) me << 16) + me;
    uint32_t cmpnew = ((uint32_t) menew << 16) + me;

    if (cmpxchg32(&(l->val), cmp, cmpnew) == cmp) {
        return 0;
    }

    NK_PROFILE_EXIT();

    return -1;
}
