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
    
    movq %rsp, %rdi

    #if NAUT_CONFIG_FIBER_FSAVE

    /* Move -1 into rax and rdx so all FPRs are saved */
    movq $-1, %rax
    movq $-1, %rdx

    /* align stack to 64 bytes */
    subq $0x1000, %rsp
    andq $-1024, %rsp

    /* place new stack ptr into 2nd argument register */
    movq %rsp, %rsi

    /* Save FPRs onto stack with xsave */
    XSAVE 0x0(%rsp)

    #endif

    callq __nk_fiber_fork

// Once the fiber is done forking, it will restore all of its GPRs except
// for %rax. This is because the address of the new fiber will be placed
// into %rax (and returned to the caller of fork)
ENTRY(_nk_fiber_fork_exit)
    movq %rax, %r15
    callq nk_fiber_current
    movq 0x0(%rax), %rsp
    movq %r15, %rax
    FIBER_RESTORE_GPRS_NOT_RAX()
    //FIBER_RESTORE_GPRS()
    retq

/******* New ASM stubs *******/

ENTRY(nk_fiber_yield)
    /* Push all GPRs onto stack */
    FIBER_SAVE_GPRS()

    /* Move new stack pointer into first argument register */
    movq %rsp, %rdi

    #if NAUT_CONFIG_FIBER_FSAVE

    /* Move -1 into rax and rdx so all FPRs are saved */
    movq $-1, %rax
    movq $-1, %rdx

    /* align stack to 64 bytes */
    subq $0x1000, %rsp
    andq $-1024, %rsp

    /* place new stack ptr into 2nd argument register */
    movq %rsp, %rsi

    /* Save FPRs onto stack with xsave */
    XSAVE 0x0(%rsp)

    #endif
    
    /* call into C code to perform yield internals */
    callq _nk_fiber_yield 
    /* This never returns, so not ret required*/

ENTRY(_nk_fiber_context_switch)
    #if NAUT_CONFIG_FIBER_FSAVE

    /* Grab position of FPRs from fiber struct */
    movq 0x10(%rdi), %rsp

    /* move -1 into rax and rdx to restore all FPRs */
    movq $-1, %rax
    movq $-1, %rax

    /* restore all FPRs from stack w/ xrstor */
    XRSTOR 0x0(%rsp)

    #endif
   
    /* changes stack ptr to new fiber's stack */
    movq 0x0(%rdi), %rsp 

    /* Pop ALL GPRs off new fiber's stack */
    FIBER_RESTORE_GPRS()
    
    /* return to new fiber's last instruction */
    retq

ENTRY(_nk_fiber_context_switch_early)
    // No need to restore FPRs, they didn't get changed

    // changes stack ptr to new fiber's stack
    movq 0x0(%rdi), %rsp 
    
    // Pop callee saved GPRs off new fiber's stack
    FIBER_RESTORE_GPRS_EARLY()

    // return to new fiber's last instruction
    retq

ENTRY(nk_fiber_yield_to)
    // Push all GPRs onto stack
    FIBER_SAVE_GPRS()

    /* Move new stack pointer into 4th argument register */
    /* 3rd arg register will be overwritten for xsave */
    movq %rsp, %rcx

    #if NAUT_CONFIG_FIBER_FSAVE

    /* Move -1 into rax and rdx so all FPRs are saved */
    movq $-1, %rax
    movq $-1, %rdx

    /* align stack to 64 bytes */
    subq $0x1000, %rsp
    andq $-1024, %rsp

    /* place new stack ptr into 3rd argument register */
    movq %rsp, %rdx

    /* Save FPRs onto stack with xsave */
    XSAVE 0x0(%rsp)

    #endif

    callq _nk_fiber_yield_to
    /* This never returns, so not ret required*/

ENTRY(_nk_fiber_join_yield)
    // Push all GPRs onto stack
    FIBER_SAVE_GPRS()

    // Move new stack pointer into first argument register
    movq %rsp, %rdi

    #if NAUT_CONFIG_FIBER_FSAVE

    /* Move -1 into rax and rdx so all FPRs are saved */
    movq $-1, %rax
    movq $-1, %rdx

    /* align stack to 64 bytes */
    subq $0x1000, %rsp
    andq $-1024, %rsp

    /* place new stack ptr into 2nd argument register */
    movq %rsp, %rsi

    /* Save FPRs onto stack with xsave */
    XSAVE 0x0(%rsp)

    #endif
    
    // call into C code to perform yield internals
    callq __nk_fiber_join_yield
    /* This never returns, so not ret required*/

#if NAUT_CONFIG_FIBER_FSAVE
ENTRY(_nk_fiber_fp_save)
    pushq %rax
    pushq %rdx
    pushq %r15
    movq 0x0(%rdi), %r15
    movq $-1, %rax
    movq $-1, %rdx
    subq $0x1000, %r15
    andq $-1024, %r15
    XSAVE 0x0(%r15)
    movq %r15, 0x10(%rdi)
    popq %r15
    popq %rdx
    popq %rax
	ret
#endif


/* Code from old implementation of yield (for later reference) */

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
