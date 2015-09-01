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
