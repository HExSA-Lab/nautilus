#include <asm/lowlevel.h>
#include <smp.h>
#include <gdt.h>

.section .text
.code16
ENTRY(init_smp_boot)
    cli
    wbinvd

    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %ss

    mov $AP_INFO_AREA, %edx
    mov (%edx), %ebx  // this is our stack address

    mov %ax, %ss
    add $0x1000, %bx // stack grows down
    mov %bx, %sp


    movw $0x17, 32(%edx)  // 16-bit limit
    leal 8(%edx), %eax    // 32-bit base
    movl %eax, 34(%edx)
    leal 32(%edx), %eax
    lgdt (%eax)

    // turn on protected mode
    mov %cr0, %eax
    orl  $0x1, %eax
    mov %eax, %cr0

    // use same cr3 as BSP
    mov 74(%edx), %eax 
    mov %eax, %cr3

    // enable PAE
    mov %cr4, %eax
    or $(1<<5), %eax
    mov %eax, %cr4


    ljmp $KERNEL_CS, $(AP_TRAMPOLINE_ADDR + .pmode_is_go - init_smp_boot)


.code32
.pmode_is_go:

    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %ss


    // enable lme bit in MSR
    movl $0xc0000080, %ecx
    rdmsr
    orl $(1<<8), %eax
    wrmsr

    // enable paging
    mov %cr0, %eax
    orl $(1<<31), %eax
    movl %eax, %cr0

    movl $AP_INFO_AREA, %edx
    movw $0x17, 64(%edx)  // 16-bit limit
    leal 40(%edx), %eax   // 64-bit base
    movl %eax, 66(%edx)
    leal 64(%edx), %eax
    lgdt (%eax)

    ljmp $KERNEL_CS, $(AP_TRAMPOLINE_ADDR + .lmode_is_go - init_smp_boot)

.code64
.lmode_is_go:

    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss


    movq $AP_INFO_AREA, %rdx
    movq 82(%rdx), %rdi
    movq 90(%rdx), %rsi

    // goodbye!
    callq *%rsi

    l0:
        pause
        jmp l0


.globl end_smp_boot
end_smp_boot:

