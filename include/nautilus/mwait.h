#ifndef __MWAIT_H__
#define __MWAIT_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * addr: the addr to wait on
 *       be sure that the range is well-known, and matches
 *       the data area indicated by CPUID
 * 
 * ECX = hints
 * EDX = hints
 *
 */
static inline void
nk_monitor (addr_t addr, uint32_t ecx, uint32_t edx)
{
    asm volatile ("monitor" 
                  : /* no outputs */
                  : "a" (addr),
                    "c" (ecx),
                    "d" (edx));

}

/* 
 * wakeups may be caused by:
 * External interrupts: NMI, SML, INIT, BINIT, MCERR
 * Faults, Aborts including Machine Check
 * Architectural TLB invalidations, including writes to CR0, CR3, CR4 and certain MSR writes
 * Voluntary transitions due to fast system call and far calls
 *
 */
static inline void 
nk_mwait (uint32_t eax, uint32_t ecx)
{
    asm volatile ("mwait"
                  : /* no outputs */
                  : "a" (eax),
                    "c" (ecx));
                    
}

int nk_mwait_init(void);


#ifdef __cplusplus
}
#endif

#endif
