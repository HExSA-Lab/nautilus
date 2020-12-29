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
#include <asm/lowlevel.h>
#include <nautilus/gdt.h>
#include <nautilus/thread.h>

/* NOTE: the below offsets and constants are VERY fragile
 * make sure to check assumptions elsewhere when changing them
 */


#define GPIO_OUTPUT 1

// We assume that %gs-based updates are safe in this code since we should only get
// here if the scheduler is running, which implies percpu is running
    
/* 
 * We come in like this:
 * ----------
 * thread_ptr
 * ----------
 * ret addr
 * ----------
 *
 */
.section .text
.code64


ENTRY(nk_thread_switch)
    pushq %rax
    movq 8(%rsp), %rax   /* save return address */
    movq %rax, -24(%rsp) /* move ret addr down (past intr state holder) */
    leaq 16(%rsp), %rsp  /* we don't want to modify RFLAGS inadvertenly, hence the lea */
    movq %rsp, -16(%rsp) /* save RSP */
    pushq $KERNEL_SS     /* save SS */
    leaq -8(%rsp), %rsp
    pushfq               /* save RFLAGS */
    movq -8(%rsp), %rax  /* restore rax */
    pushq $KERNEL_CS     /* push CS seg sel */
    subq $16, %rsp        /* point rsp at ret addr */

    pushq $0             /* push fake intr number */

    SAVE_GPRS()

    incw %gs:8           // increment interrupt nesting level
	                 // pretend we are doing an interrupt-driven switch
	                 // to make the sums work out
	
ENTRY(nk_thread_switch_intr_entry)

#if GPIO_OUTPUT && defined(NAUT_CONFIG_GPIO)
    // GPIO output
    pushq %rdi
    movq $0x8, %rdi   // bit 3 set
    movq $0x0, %rsi    // or it high
    callq nk_gpio_output_mask
    popq %rdi
#endif
	
#ifdef NAUT_CONFIG_PROFILE
    pushq %rdi
    callq nk_thr_switch_prof_enter
    popq %rdi
#endif

	
    movq %gs:0x0, %rax

    movq %rsp, (%rax)   /* save the current stack pointer */

#ifdef NAUT_CONFIG_FPU_SAVE
    /* Save the FPRs */
    movzwq 16(%rax), %rbx
    leaq (%rax, %rbx, 1), %rbx
    fxsave (%rbx)
#endif

// On a thread exit we must avoid saving thread state
// or using the stack at all since at this point the thread and its stack
// might already be deleted.  Interrupts are off, however.
// We also want to save the cost of any state saves
// We should get here via a JUMP
// Note that the first three instructions establish the new stack
// and thread structure	
ENTRY(nk_thread_switch_on_exit)

    movq %rdi, %rax     /* load up pointer to the next thread */

#ifdef NAUT_CONFIG_CACHEPART

    push %rbx          /* RBX will be used to test for CAT */
    movq _nk_cache_part_has_cat, %rbx /* Fetch has_cat from cachepart.c */
    test %rbx, %rbx    /* Set Z flag */
    jz skip_cat        /* if (!has_cat) context switch like normal */
    
    /* Load CAT partition into the right MSR */

    push %rax          /* stash regs we will be using */
    push %rcx
    push %rdx

    movzwq 18(%rax), %rdx /* Fetch CAT partition from thread */
    xorq %rax, %rax    /* Zero out RAX */

    movq $0xc8f, %rcx /* Write CAT MSR (IA32_PQR_ASSOC) number into RCX for use by wrmsr*/
    wrmsr             /* Set the COS into the MSR */

    /* We have now successfully switched to the new CAT partition */

    pop %rdx          /* Restore the regs we used */
    pop %rcx
    pop %rax

    /* Fall back to normal switch operation if !has_cat */
skip_cat:
    pop %rbx            /* Restore register used to test for CAT */

continue_context_switch:

#endif

#ifdef NAUT_CONFIG_ASPACES
    push %rax
    movq 24(%rax), %rdi
    call nk_aspace_switch
    pop %rax
#endif

#ifdef NAUT_CONFIG_HARDWARE_TLS
    push %rax               // save regs we will use
    push %rdx
    push %rcx

    movl 36(%rax), %edx     // get big end of fsbase from thread struct
    movl 32(%rax), %eax     // get little end of fsbase from thread struct
    movq $0xc0000100, %rcx  // get FSBASE MSR number into RCX for use by wrmsr
	
    wrmsr                   // actually change fsbase
	
    pop %rcx                // restore regs we have usd
    pop %rdx
    pop %rax
#endif
	
    movq %rax, %gs:0x0  /* make it the new current thread */
    movq (%rax), %rsp   /* load its stack pointer */

#ifdef NAUT_CONFIG_FPU_SAVE
    /* Restore the FPRs */
    movzwq 16(%rax), %rbx
    leaq (%rax, %rbx, 1), %rbx
    fxrstor (%rbx)
#endif

#ifdef NAUT_CONFIG_PROFILE
    callq nk_thr_switch_prof_exit
#endif

#if GPIO_OUTPUT && defined(NAUT_CONFIG_GPIO)
    movq $0x18, %rdi   // bit 3+4 (irq goes low too)
    notq %rdi         // clear
    movq $0x1, %rsi    // and it low
    callq nk_gpio_output_mask
#endif
	
    RESTORE_GPRS()      /* load the new thread's GPRs */

    leaq 16(%rsp), %rsp /* pop off the vector and the error code */

    decw %gs:8          // decrement interrupt nesting level -
	                // we may now be leaving interrupt context

    iretq               /* and off we go */

// rdi = new thread
// rsi = target address to write
// edx = value to write (as 4 bytes)
ENTRY(nk_thread_switch_exit_helper)
	// reset interrupt nesting
	// the context switch will decrement it to zero
	movw $1,  %gs:8
	// reset preemption disable level
        movw $0, %gs:10
	// note that interrupts are still off

	// We now mark ourselves as toast for the reaper
	movl %edx, (%rsi)
	
	// At this point, we are racing with the reaper on a different core,
	// which might take away our stack.  However, we have interrupts
	// off locally, so as long as *we* don't use the stack, we should be OK
	jmp nk_thread_switch_on_exit

	// and this should never happen...
	retq
	

	
ENTRY(nk_thread_entry) 

#ifdef NAUT_CONFIG_ENABLE_STACK_CHECK
    /* test and see if we've overrun our stack */
    pushq %rax
    pushq %rbx
    movq %gs:0x0, %rax
    movq (%rax), %rbx // RSP
    movq 8(%rax), %rax // stack start
    cmpq %rax, %rbx
    jle bad
    popq %rbx
    popq %rax
#endif

    sti
    retq

#ifdef NAUT_CONFIG_ENABLE_STACK_CHECK
bad:
    movabs $panic_str, %rdi
    movabs $panic, %rax
    callq *%rax
loop:
    hlt
    jmp loop
#endif
    
// RBP and RSP are intentionally not saved here
// 15 quadwords (14 regs + flags)


#define GPR_SAVE()	                        \
pushq %rax	;				\
pushq %rbx	;				\
pushq %rcx	;				\
pushq %rdx	;				\
pushq %rsi	;				\
pushq %rdi	;				\
pushq %rbp  ;               \
pushq %r8	;				\
pushq %r9	;				\
pushq %r10	;				\
pushq %r11	;				\
pushq %r12	;				\
pushq %r13	;				\
pushq %r14	;				\
pushq %r15      ;                               \
pushfq          ;

#define GPR_LOAD_NO_RAX()	                \
popfq           ;                               \
popq %r15       ;                               \
popq %r14	;				\
popq %r13	;				\
popq %r12	;				\
popq %r11	;				\
popq %r10	;				\
popq %r9	;				\
popq %r8	;				\
popq %rbp   ;               \
popq %rdi	;				\
popq %rsi	;				\
popq %rdx	;				\
popq %rcx	;				\
popq %rbx	;				\

#define GPR_LOAD()                              \
GPR_LOAD_NO_RAX()                                \
popq %rax       ;

#define GPR_LOAD_SKIP_RAX()                     \
GPR_LOAD_NO_RAX()                                \
leaq 8(%rsp), %rsp ;                            // This instruction must not affect CCs

#define FPR_SAVE()			// WRITE ME
#define FPR_LOAD()

#define SR_SAVE()           // WRITE ME
#define SR_LOAD()


#define REG_SAVE() GPR_SAVE() SR_SAVE() FPR_SAVE()
#define REG_LOAD() FPR_LOAD() SR_LOAD() GPR_LOAD()

#define REG_LOAD_SKIP_RAX() FPR_LOAD() SR_LOAD() GPR_LOAD_SKIP_RAX()

#define GPR_SAVE_SIZE (15*8)
#define SR_SAVE_SIZE (0)
#define FPR_SAVE_SIZE (0)

#define REG_SAVE_SIZE (GPR_SAVE_SIZE+SR_SAVE_SIZE+FPR_SAVE_SIZE)
	

/*
	The entry point for forking a thread is essentially
	like a context switch away from a thread in that we
	need to capture a snapshot of the caller's register
	state and clone his stack.   We do in two stages
	the first is here, where we snapshot the register
	state on the caller's stack, preceded by another copy
	of the return address.  then we call the internal
	fork routine which will clone the stack, including this
	data.  The launcher will then switch to the cloned
	stack and restore register state and return

*/	

ENTRY(nk_thread_fork)
	REG_SAVE()                  // RBP and RSP will be handled in _thread_fork

	// Note that callee will see us as part of the stack frame
	// of our caller
	callq __thread_fork  ;    // goto C code which will return for us


// Both the parent and child will get back here, albeit on
// different stacks and different rax return values
ENTRY(_fork_return)
	REG_LOAD_SKIP_RAX()         // we want the return produced by _thread_fork
	retq                        // and back we go

/*
	Make it possible to call a function
	that expects an interrupt frame to be sitting
	on the stack from a non-interrupt code path

	foo(struct nk_regs *r)

	nk_interrupt_like_trampoline(foo)
	
*/
ENTRY(nk_interrupt_like_trampoline)
	// we need to create a whole interrupt stack frame
	mov  %ss, %rax
	movq %rax, -8(%rsp)
	movq %rsp, -16(%rsp)	
	leaq -16(%rsp),%rsp
	pushfq
	mov   %cs, %rax
	pushq %rax
	pushq 32(%rsp)  //rip
	pushq $0        // vector
	pushq $0        // error code
	REG_SAVE()      // gprs, etc - also pushes flags
	movq %rdi, %rax
	leaq 8(%rsp), %rdi // we don't need the flags again
	callq *%rax
	REG_LOAD_SKIP_RAX() // pops flags
	// now we have 7 more words of crud to toss
	leaq 56(%rsp), %rsp
	retq			


/*
	Allow C-code to do FP saves/restores

	nk_fp_save(destptr)
	mk_fp_restore(srcptr)
	
*/
ENTRY(nk_fp_save)
	fxsave (%rdi)
	ret

ENTRY(nk_fp_restore)
	fxrstor (%rdi)
	ret
	
panic_str:
.ascii "Stack corruption detected\12\0"
