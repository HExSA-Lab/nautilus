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

.code32
.section .mbhdr
.align 8

#include <asm/lowlevel.h>

/**
 * This header makes us multi-boot compliant
 */
multiboot_hdr:
    .long 0xe85250d6
    .long 0
    .long hdr_end - multiboot_hdr
    .long -(0xe85250d6 + 0 + (hdr_end - multiboot_hdr))

    /* BEGIN TAGS */

    /* sections */
    .word 2, 0
    .long 24
    .long multiboot_hdr
    .long _loadStart
    .long _loadEnd
    .long _bssEnd

    /* entry point */
    .word 3, 0
    .long 16
    .long nautilus_entry
    .long 0

    /* tags end */
    .word 0, 0
    .long 8
hdr_end:

.section .boot

/**
 * our entry point into the kernel, this is where
 * GRUB2 will drop us
 */
 GLOBAL(nautilus_entry)

    /* get GRUB multiboot info and stash it */
    movl %ebx, %edi
    movl %eax, %esi

    /* don't trust GRUB's protected-mode GDT */
    movl $gdtr32, %eax
    lgdt (%eax)
    ljmp $0x8, $.gdt1_is_go


.gdt1_is_go:
    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %ss
    movl $boot_stack_end-1, %esp

    call paging_longmode_setup

    /* now our long mode GDT */
    movl $gdtr64, %eax
    lgdt (%eax)

    ljmp $0x8, $.gdt2_is_go

.code64
.gdt2_is_go:
    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    movq $boot_stack_end, %rsp

    /* multiboot info is still in rdi and rsi */
    movq $main, %rax
    callq *%rax

    /* shouldn't get here */
    cli
    jmp .

.code32
paging_longmode_setup:

    /* PML4[0] -> PDPT */
    movl $pdpt, %eax
    orl $0x3, %eax       /* entry is present, rw */
    movl $pml4, %ebx
    movl %eax, (%ebx)

    // PDPT[0] -> PDT 
    movl $pd, %eax
    orl $0x3, %eax
    movl $pdpt, %ebx
    movl %eax, (%ebx)

    // Identity map the first GB
    movl $512, %ecx
    movl $pd, %edx
    movl $0x83, %eax // set PS bit also (PDE -> 2MB page)
.write_pde:
    movl %eax, (%edx)
    addl $0x200000, %eax
    addl $0x8, %edx
    loop .write_pde

    /* put pml4 address in cr3 */
    movl $pml4, %eax
    movl %eax, %cr3

    /* enable PAE */
    mov %cr4, %eax
    or $(1<<5), %eax
    mov %eax, %cr4

    /* enable lme bit in MSR */
    movl $0xc0000080, %ecx
    rdmsr
    orl $(1<<8), %eax
    wrmsr

    /* paging enable */
    movl %cr0, %eax
    orl $(1<<31), %eax

    /* make sure we're in "normal cache mode" */
    movl $~(3<<29), %ebx
    andl %ebx, %eax

    movl %eax, %cr0

    ret
     
.align 8
gdt32:
    .quad 0x0000000000000000 /* null */
    .quad 0x00cf9a000000ffff /* code */
    .quad 0x00cf92000000ffff /* data */

.align 8
gdt64:
    .quad 0x0000000000000000 /* null */
    .quad 0x00af9a000000ffff /* code (note lme bit) */
    .quad 0x00af92000000ffff /* data (most entries don't matter) */


.align 8
gdtr32:
    .word 23
    .long gdt32

.align 8
GLOBAL(gdtr64)
    .word 23
    .quad gdt64

