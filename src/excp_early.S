#include <idt.h>

.code64
.align 8
.globl early_excp_handlers
early_excp_handlers:
    MAKE_IDT_HANDLER(0)
    MAKE_IDT_HANDLER(1)
    MAKE_IDT_HANDLER(2)
    MAKE_IDT_HANDLER(3)
    MAKE_IDT_HANDLER(4)
    MAKE_IDT_HANDLER(5)
    MAKE_IDT_HANDLER(6)
    MAKE_IDT_HANDLER(7)
    MAKE_IDT_HANDLER(8)
    MAKE_IDT_HANDLER(9)
    MAKE_IDT_HANDLER(10)
    MAKE_IDT_HANDLER(11)
    MAKE_IDT_HANDLER(12)
    MAKE_IDT_HANDLER(13)
    MAKE_IDT_HANDLER(14)
    MAKE_IDT_HANDLER(15)
    MAKE_IDT_HANDLER(16)
    MAKE_IDT_HANDLER(17)
    MAKE_IDT_HANDLER(18)
    MAKE_IDT_HANDLER(19)
    MAKE_IDT_HANDLER(20)
    MAKE_IDT_HANDLER(21)
    MAKE_IDT_HANDLER(22)
    MAKE_IDT_HANDLER(23)
    MAKE_IDT_HANDLER(24)
    MAKE_IDT_HANDLER(25)
    MAKE_IDT_HANDLER(26)
    MAKE_IDT_HANDLER(27)
    MAKE_IDT_HANDLER(28)
    MAKE_IDT_HANDLER(29)
    MAKE_IDT_HANDLER(30)
    MAKE_IDT_HANDLER(31)

.globl early_excp_common
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
    movq $excp_handler_table, %rdx
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


.globl excp_handler_table
excp_handler_table:
.rept NUM_EXCEPTIONS
    .quad null_excp_handler
.endr
