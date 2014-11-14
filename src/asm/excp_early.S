#include <idt.h>
#include <thread.h>

.code64
.align 8
.globl early_excp_handlers
early_excp_handlers:
    MAKE_EXCP_HANDLER(0)
    MAKE_EXCP_HANDLER(1)
    MAKE_EXCP_HANDLER(2)
    MAKE_EXCP_HANDLER(3)
    MAKE_EXCP_HANDLER(4)
    MAKE_EXCP_HANDLER(5)
    MAKE_EXCP_HANDLER(6)
    MAKE_EXCP_HANDLER(7)
    MAKE_EXCP_HANDLER(8)
    MAKE_EXCP_HANDLER(9)
    MAKE_EXCP_HANDLER(10)
    MAKE_EXCP_HANDLER(11)
    MAKE_EXCP_HANDLER(12)
    MAKE_EXCP_HANDLER(13)
    MAKE_EXCP_HANDLER(14)
    MAKE_EXCP_HANDLER(15)
    MAKE_EXCP_HANDLER(16)
    MAKE_EXCP_HANDLER(17)
    MAKE_EXCP_HANDLER(18)
    MAKE_EXCP_HANDLER(19)
    MAKE_EXCP_HANDLER(20)
    MAKE_EXCP_HANDLER(21)
    MAKE_EXCP_HANDLER(22)
    MAKE_EXCP_HANDLER(23)
    MAKE_EXCP_HANDLER(24)
    MAKE_EXCP_HANDLER(25)
    MAKE_EXCP_HANDLER(26)
    MAKE_EXCP_HANDLER(27)
    MAKE_EXCP_HANDLER(28)
    MAKE_EXCP_HANDLER(29)
    MAKE_EXCP_HANDLER(30)
    MAKE_EXCP_HANDLER(31)

.macro make_irq_handler v
    .globl early_irq_handler_\@
    .type  early_irq_handler_\@, @function
    .align 16
    early_irq_handler_\@ :
    pushq $\v
    jmp early_irq_common
.endm
    
.align 16
.globl early_irq_handlers
early_irq_handlers:

    vector = 32
    .rept NUM_IDT_ENTRIES - 32
    make_irq_handler vector
    vector = vector + 1
    .endr


.globl early_irq_common
.align 8
early_irq_common:

    SAVE_GPRS()

    leaq 128(%rsp), %rdi # pointer to exception struct
    movq 120(%rsp), %rsi # irq num
    movq $handler_table, %rdx
    movq (%rdx, %rsi, 8), %rcx

    callq *%rcx

    testq %rax, %rax
    jnz irq_err

    // we're back from the irq handler
    // do we need to switch to someone else?
    callq nk_need_resched
    testq %rax, %rax
    jz thr_return
    movq %rax, %rdi
    jmp nk_thread_switch_intr_entry 

thr_return:
    RESTORE_GPRS()
    addq $8, %rsp // pop the irq num, don't need it now
    iretq

irq_err:
    hlt

.globl early_excp_common
.align 8
early_excp_common:

    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi 
    pushq %rdi  
    pushq %r8    
    pushq %r9     
    pushq %r10     
    pushq %r11      

    leaq 80(%rsp), %rdi # pointer to exception struct
    movq 72(%rsp), %rsi # excp vector
    movq $handler_table, %rdx
    movq (%rdx, %rsi, 8), %rcx
    cmpq $14, %rsi
    jnz no_pf
    movq %cr2, %rdx

no_pf:
    callq *%rcx
    testq %rax, %rax
    jnz excp_err

    popq %r11
    popq %r10 
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax
    addq $16, %rsp
    iretq

excp_err:
    hlt


.globl handler_table
handler_table:
.rept NUM_EXCEPTIONS
    .quad null_excp_handler
.endr

.rept NUM_IDT_ENTRIES - NUM_EXCEPTIONS
    .quad null_irq_handler
.endr
