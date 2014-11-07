
#include <asm/lowlevel.h>
#include <gdt.h>

.macro save_gprs
    movq %rax, -8(%rsp)
    movq %rbx, -16(%rsp)
    movq %rcx, -24(%rsp)
    movq %rdx, -32(%rsp)
    movq %rsi, -40(%rsp)
    movq %rdi, -48(%rsp)
    movq %rbp, -56(%rsp)
    movq %r8,  -64(%rsp)
    movq %r9,  -72(%rsp)
    movq %r10, -80(%rsp)
    movq %r11, -88(%rsp)
    movq %r12, -96(%rsp)
    movq %r13, -104(%rsp)
    movq %r14, -112(%rsp)
    movq %r15, -120(%rsp)
    subq $120, %rsp
.endm

.macro restore_gprs
    movq (%rsp), %r15
    movq 8(%rsp), %r14
    movq 16(%rsp), %r13
    movq 24(%rsp), %r12
    movq 32(%rsp), %r11
    movq 40(%rsp), %r10
    movq 48(%rsp), %r9
    movq 56(%rsp), %r8
    movq 64(%rsp), %rbp
    movq 72(%rsp), %rdi
    movq 80(%rsp), %rsi
    movq 88(%rsp), %rdx
    movq 96(%rsp), %rcx
    movq 104(%rsp), %rbx
    movq 112(%rsp), %rax
    addq $136, %rsp  /* we skip intr number and err code, so this is 120+16 */
.endm
    

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
ENTRY(thread_switch)
    pushq %rax
    movq 8(%rsp), %rax   /* save return address */
    movq %rax, -24(%rsp)  /* move ret addr down (past intr state holder) */
    addq $16, %rsp
    movq %rsp, -16(%rsp)  /* save RSP */
    pushq $KERNEL_DS              /* save SS */
    subq $8, %rsp
    pushfq               /* save RFLAGS */
    movq -8(%rsp), %rax  /* restore rax */
    pushq $KERNEL_CS     /* push CS seg sel */
    subq $8, %rsp        /* point rsp at ret addr */

    pushq $0             /* push fake err code and intr number */
    pushq $0

    save_gprs

    movq %gs:0x0, %rax
    movq %rsp, (%rax)   /* save the current stack pointer */


    movq %rdi, %rax     /* load up pointer to the next thread */
    movq %rax, %gs:0x0 /* make it the new current thread */
    movq (%rax), %rsp       /* load its stack pointer */

    restore_gprs        /* load the new thread's GPRs */

    iretq /* and off we go */

ENTRY(thread_entry) 
    sti
    retq
    
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
leaq 8(%rsp), %rsp     ;                     // This instruction must not affect CCs

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

ENTRY(thread_fork)
    cli                         // turn interrupts off
	REG_SAVE()                  // RBP and RSP will be handled in _thread_fork

	// Note that callee will see us as part of the stack frame
	// of our caller
	callq _thread_fork  ;    // goto C code which will return for us


// Both the parent and child will get back here, albeit on
// different stacks and different rax return values
ENTRY(_fork_return)
	REG_LOAD_SKIP_RAX()         // we want the return produced by _thread_fork
    sti                         // turn interrupts back on (this is actually unnecessary in the child but oh well)
	retq                        // and back we go

