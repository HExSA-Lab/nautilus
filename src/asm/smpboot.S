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
#include <nautilus/smp.h>
#include <nautilus/gdt.h>

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
        jmp l0


.globl end_smp_boot
end_smp_boot:


/*
 * we come in with two arguments:
 * rdi = new rsp
 * rsi = new rbp
 * rdx = cpu_ptr
 * we leave with a new stack
 */
ENTRY(smp_ap_stack_switch)
     pushq %rcx           // let's not clobber regs
     movq 8(%rsp), %rcx   // save the old stack's ret addr (to get back to C)
     movq %rsi, %rbp      // set our new frame pointer
     popq %rsi            // we can now reuse rsi for the old value of rcx
     movq %rdi, %rsp      // switch stacks
     pushq %rcx           // push our return address back to C
     movq %rsi, %rcx      // restore rcx
     movq %rdx, %rax      // return the old cpu pointer
     retq
