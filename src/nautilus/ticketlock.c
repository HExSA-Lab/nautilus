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
