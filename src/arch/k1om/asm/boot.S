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
    
.code64
    /* get GRUB multiboot info and stash it */
    movl %ebx, %edi
    movl %eax, %esi

    movq $boot_stack_end, %rsp
    
    call paging_setup

    /* multiboot info is still in rdi and rsi */
    movq $main, %rax

    movabsq $0x007a7a70, %r14
    callq .tmp
.tmp:
    popq %rcx
    movq %rcx, (%r14)
    callq *%rax

    /* shouldn't get here */
    cli
    jmp .

paging_setup:

    /* PML4[0] -> PDPT */
    movq $pdpt, %rax
    orq $0x3, %rax       /* entry is present, rw */
    movq $pml4, %rbx
    movq %rax, (%rbx)

    // PDPT[0] -> PDT 
    movq $pd, %rax
    orq $0x3, %rax
    movq $pdpt, %rbx
    movq %rax, (%rbx)

    // Identity map the first GB
    movq $512, %rcx
    movq $pd, %rdx
    movq $0x83, %rax // set PS bit also (PDE -> 2MB page)
.write_pde:
    movq %rax, (%rdx)
    addq $0x200000, %rax
    addq $0x8, %rdx
    loop .write_pde

    /* put pml4 address in cr3 */
    movq $pml4, %rax
    movq %rax, %cr3

    /* enable PAE */
    movq %cr4, %rax
    orq $(1<<5), %rax
    movq %rax, %cr4

    /* enable lme bit in MSR */
    movq $0xc0000080, %rcx
    rdmsr
    orq $(1<<8), %rax
    wrmsr

    /* paging enable */
    movq %cr0, %rax
    orl $(1<<31), %eax
    movq %rax, %cr0

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
