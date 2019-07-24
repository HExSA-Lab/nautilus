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
 * %rdi: ptr to fiber we're switching to (if starting fiber)
 * Remaining GPR Values (%rbp through %r15)
 * -------------------
 * 
 * Full stack state can be found in FIBER_SAVE_GPRS() 
 * macro in include/nautilus/fiber.h
 *
 * This means we can switch to the desired fiber by 
 * placing f_to->rsp into %rsp, popping off the registers,
 * and executing retq to start executing the fiber routine.
 *
 * Before we do that, we must save the current state onto 
 * the current stack. Once we save the GPRS, the fiber we're 
 * switching away from will have a stack like this:
 *
 *--------------------------
 * addr of next instruction in routine
 * -------------------------
 * GPR Values (%rax through %rsi)
 * %rdi: not necessarily still the curr_fiber
 * Remaining GPR Values (%rbp through %r15)
 * -------------------------
 * 
 */

.section .text
.code64


ENTRY(_nk_fiber_context_switch)
    // Saves GPRs of fiber we're switching away from
    // Saves them onto current stack
    FIBER_SAVE_GPRS()
//    SAVE_FPRS()
    
    // puts new %rsp value into f_from's stack ptr
    movq %rsp, 0x0(%rdi)

    // switches to the stack of the fiber we're switching to
    movq 0x0(%rsi), %rsp
    
    // Restores registers from new stack
    FIBER_RESTORE_GPRS()
//    RESTORE_FPRS()
    
    // brings us back to new fiber's routine
    retq

// We don't care about saving old fiber's stack since it's exiting
// We simply switch to the new fiber's stack and ret to it's routine
ENTRY(_nk_exit_switch)
    movq 0x0(%rdi), %rsp
    // RESTORE_FPRS()
    FIBER_RESTORE_GPRS()
    retq

// Entry for the fork. This allows us to save the GRPs of the fiber we're
// forking. We need to do this because GPRs will be modified when we begin
// the forking procedure
ENTRY(nk_fiber_fork)
    FIBER_SAVE_GPRS()
    callq __nk_fiber_fork

// Once the fiber is done forking, it will restore all of its GPRs except
// for %rax. This is because the address of the new fiber will be placed
// into %rax (and returned to the caller of fork)
ENTRY(_nk_fiber_fork_exit)
    FIBER_RESTORE_GPRS_NOT_RAX()
    //FIBER_RESTORE_GPRS()
    retq

/*
ENTRY(_nk_fiber_context_switch)
    // Saves GPRs without modifying %rsp
    FIBER_SAVE_GPRS()
//    SAVE_FPRS()
    // puts new %rsp value into f_from's stack ptr
    movq %rax, 0x0(%rdi)
    // puts f_to's stack ptr in %rax
    // this allows us to not change %rsp until the very end
    movq 0x0(%rsi), %rax
    FIBER_RESTORE_GPRS()
//    RESTORE_FPRS()
    retq

ENTRY(_nk_exit_switch)
    movq 0x0(%rdi), %rax
    // RESTORE_FPRS()
    FIBER_RESTORE_GPRS()
    retq

ENTRY(nk_fiber_fork)
    FIBER_SAVE_GPRS()
    movq %rax, %rsp
    callq __nk_fiber_fork

ENTRY(_nk_fiber_fork_exit)
    FIBER_RESTORE_GPRS_NOT_RAX()
    //FIBER_RESTORE_GPRS()
    retq
*/
