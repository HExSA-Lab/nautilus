#include <asm/lowlevel.h>
#include <gdt.h>
#include <thread.h>

/* NOTE: the below offsets and constants are VERY fragile
 * make sure to check assumptions elsewhere when changing them
 */
    

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
    subq $8, %rsp        /* point rsp at ret addr */

    pushq $0             /* push fake intr number */

    SAVE_GPRS()

ENTRY(nk_thread_switch_intr_entry)

    movq %gs:0x0, %rax
    movq %rsp, (%rax)   /* save the current stack pointer */

    /* Save the FPRs */
    movzwq 16(%rax), %rbx
    movq (%rax, %rbx, 1), %rbx
    fxsave (%rbx)

    movq %rdi, %rax     /* load up pointer to the next thread */
    movq %rax, %gs:0x0  /* make it the new current thread */
    movq (%rax), %rsp   /* load its stack pointer */

    /* Restore the FPRs */
    movzwq 16(%rax), %rbx
    movq (%rax, %rbx, 1), %rbx
    fxrstor (%rbx)

    RESTORE_GPRS()      /* load the new thread's GPRs */

    leaq 8(%rsp), %rsp  /* pop off the irq number */

    iretq               /* and off we go */




ENTRY(nk_thread_entry) 
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

    sti
    retq

bad:
    movq $panic_str, %rdi
    callq panic
loop:
    hlt
    jmp loop
    
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

ENTRY(nk_thread_fork)
    cli                         // turn interrupts off
	REG_SAVE()                  // RBP and RSP will be handled in _thread_fork

	// Note that callee will see us as part of the stack frame
	// of our caller
	callq __thread_fork  ;    // goto C code which will return for us


// Both the parent and child will get back here, albeit on
// different stacks and different rax return values
ENTRY(_fork_return)
	REG_LOAD_SKIP_RAX()         // we want the return produced by _thread_fork
    sti                         // turn interrupts back on (this is actually unnecessary in the child but oh well)
	retq                        // and back we go


panic_str:
.ascii "Stack corruption detected\12\0"
