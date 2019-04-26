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
 * Copyright (c) 2019, Michael Cuevas <cuevas@u.northwestern.edu>
 * Copyright (c) 2019, Enrico Deiana <ead@u.northwestern.edu>
 * Copyright (c) 2019, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Michael Cuevas <cuevas@u.northwestern.edu>
 *          Enrico Deiana <ead@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <asm/lowlevel.h>
#include <nautilus/fiber.h>

/* 
 * Fiber we're switching to has a stack set up like this:
 * 
 * -------------------
 * ret addr to next instruction in fiber's routine
 * (or to _fiber_wrapper if we're starting fiber)
 * -------------------
 * GPR Values (%rax through %rsi)
 * %rdi: ptr to fiber we're switching to
 * Remaining GPR Values (%rbp through %r15)
 * -------------------
 * 
 * Full stack state can be found in FIBER_SAVE_GPRS() 
 * macro in include/nautilus/fiber.h
 *
 * Once we save the GPRS, the fiber we're switching
 * away from will have a stack like this:
 *
 *--------------------------
 * addr of next instruction in routine
 * -------------------------
 * GPR Values (%rax through %rsi)
 * %rdi: ptr to fiber we're switching away from
 * Remaining GPR Values (%rbp through %r15)
 * -------------------------
 * 
 */

.section .text
.code64


ENTRY(nk_fiber_context_switch)
    FIBER_SAVE_GPRS()
//    SAVE_FPRS()
    movq %rsp, 0x0(%rdi)
    movq 0x0(%rsi), %rsp
// Could we keep track of which floating point registers need to be saved
// using unused registers (we talked about during last meeting) and then 
// using a bitmask to figure out which registers to save? 
//    RESTORE_FPRS()
    FIBER_RESTORE_GPRS()
    retq

ENTRY(_nk_exit_switch)
    movq 0x0(%rdi), %rsp
    // RESTORE_FPRS()
    FIBER_RESTORE_GPRS()
    retq

ENTRY(nk_fiber_fork)
    FIBER_SAVE_GPRS()
    callq __nk_fiber_fork

ENTRY(_nk_fiber_fork_exit)
    FIBER_RESTORE_GPRS_NOT_RAX()
    //FIBER_RESTORE_GPRS()
    retq

