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
 * Copyright (c) 2018, Peter Dinda <pdinda@northwestern.edu>
 *                     Mike Leonard <MichaelLeonard2018@u.northwestern.edu>
 *                     John Albers  <JohnAlbers2018@u.northwestern.edu>
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Peter Dinda <pdinda@northwestern.edu>
 *          Mike Leonard <MichaelLeonard2018@u.northwestern.edu>
 *          John Albers  <JohnAlbers2018@u.northwestern.edu>
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */


// Assume Gem5, full system simulation, x86, with kernel pointed at nautilus.bin

// In this startup mode, we are already in long mode, and we have no multiboot support
	
#include <asm/lowlevel.h>

.code64
.section .boot

GLOBAL(nautilus_entry)

    // make sure we have a stack
    movq $boot_stack_end, %rsp

    // do our own paging setup
    call paging_longmode_setup

	
    /* now our long mode GDT */
    movq $gdtr64, %rax
    lgdt (%rax)

    // do the 64 bit version of the ljmp dance to set CS
    subq $16, %rsp
    movq $0x8, 8(%rsp)
    movabsq $cs_set, %rax
    movq %rax, (%rsp)
    lretq

    // now set the other segment regs
cs_set:	
    movq $0x10, %rax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs

    // make sure we have caches on - make sure we have ~CD and ~NWT
	 
    movq %cr0, %rax
    movq $0x60000000, %rbx
    notq %rbx
    andq %rbx, %rax	
    movq %rax, %cr0
	
    // gem5 passes a pointer to the real-mode data (e.g. e820)
    // via rsi (rdi?)
	
    movq $main, %rax
    callq *%rax

    /* shouldn't get here */
    cli
    jmp .


	
// must be code 64
paging_longmode_setup:

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

    // We do not do PAE, LME, PG, !CD, etc, since we are already in long mode
	
    ret
     

.align 8
gdt64:
    .quad 0x0000000000000000 /* null */
    .quad 0x00af9a000000ffff /* code (note lme bit) */
    .quad 0x00af92000000ffff /* data (most entries don't matter) */

.align 8
GLOBAL(gdtr64)
    .word 23
    .quad gdt64

