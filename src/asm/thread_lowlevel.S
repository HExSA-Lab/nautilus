
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
    
