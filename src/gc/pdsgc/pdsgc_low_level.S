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
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2017, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

// wrapper code so that we can use a private stack for the GC process

#define GPR_SAVE()                              \
pushq %rax      ;                               \
pushq %rbx      ;                               \
pushq %rcx      ;                               \
pushq %rdx      ;                               \
pushq %rsi      ;                               \
pushq %rdi      ;                               \
pushq %rbp  ;               \
pushq %r8       ;                               \
pushq %r9       ;                               \
pushq %r10      ;                               \
pushq %r11      ;                               \
pushq %r12      ;                               \
pushq %r13      ;                               \
pushq %r14      ;                               \
pushq %r15      ;                               \
pushfq          ;

#define GPR_LOAD_NO_RAX()                       \
popfq           ;                               \
popq %r15       ;                               \
popq %r14       ;                               \
popq %r13       ;                               \
popq %r12       ;                               \
popq %r11       ;                               \
popq %r10       ;                               \
popq %r9        ;                               \
popq %r8        ;                               \
popq %rbp   ;               \
popq %rdi       ;                               \
popq %rsi       ;                               \
popq %rdx       ;                               \
popq %rcx       ;                               \
popq %rbx       ;                               \

#define GPR_LOAD()                              \
GPR_LOAD_NO_RAX()                                \
popq %rax       ;

#define GPR_LOAD_SKIP_RAX()                     \
GPR_LOAD_NO_RAX()                                \
leaq 8(%rsp), %rsp ;                            // This instruction must not affect CCs


	
// input:  rdi = stack bottom to switch to
//         rsi = t->rsp to update
//         rdx = function to invoke
//	   rsp = stack bottom to swtich from/return to
.global _nk_gc_pdsgc_stack_wrap
_nk_gc_pdsgc_stack_wrap:
	GPR_SAVE()              // make sure any in-register pointer
	                        // is now on our stack
	movq %rsp, (%rsi)       // update t->rsp so GC can find them
	movq %rsp, -8(%rdi)     // stash old rsp on new stack so we can restore it
	leaq -8(%rdi), %rsp     // switch to the GC stack
	call *%rdx              // invoke gc function
	popq %rsp               // restore to the thread stack
	GPR_LOAD_SKIP_RAX()     // restore our GPRs
	retq

	
	
