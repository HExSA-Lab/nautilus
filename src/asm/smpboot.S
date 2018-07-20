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
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu> (Gem5 compatability + debug)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <asm/lowlevel.h>
#include <nautilus/smp.h>
#include <nautilus/gdt.h>

/*

	We are copied to the trampoline address and then
	started using an IPI.  We start in real mode.

	AP_TRAMPOLINE_ADDR = 0xf000 linear => CS:IP=0xf00:0 at entry

	We have stack starting at

	AP_BOOT_STACK_ADDR = 0x1000 linear
	
	We have information from the BSP at

	AP_INFO_AREA = 0x2000 linear

	The info area has pointers to a stack region and to to
	desired 32 and 64 bit GDTs, CR3, etc.
	
	For data/stack addresses, we will use real mode selectors set at zero,
	hence the operand addresses are the same => 0x1000 linear => 0x0:0x1000
	
	For code addresses, we use CS:IP as above.

	The ES selector is a special case, at least in Gem5, but possibly elsewhere.  It
	cannot be zero as there is an early "null segment" check in the very next ifetch
	after enabling protection.  Since we don't use ES here ourselves, we just force it
	to be non-zero.

	We quickly transition to protected mode at which point this nuttiness ceases.
*/
	
.section .text
.code16
ENTRY(init_smp_boot)
    cli            // transitions require interrupts off
    wbinvd         // get cache in a known state

    mov %cs, %ax   // avoid es being zero as there is an indirect null seg check on it
    mov %ax, %es   // at least in gem5, possibly other hardware
	
    xor %ax, %ax   // zero out other segment regs we will actually
    mov %ax, %ds   // use so that we can behave like we have a linear
    mov %ax, %ss   // 16-bit address space. 
	
    mov $AP_INFO_AREA, %edx   // find the info the BSP is providing
    mov (%edx), %ebx          // fetch our desired stack address
	
    add $0x1000, %bx          // stack grows down, so compute topmost stack addr

    mov %bx, %sp              // and now we have a stack

    // Compute the base and limit for the 32 bit GDT descriptor based
    // on the address at which it has been mapped	
    movw $0x17, 32(%edx)  // set 16-bit limit
    leal 8(%edx), %eax    // compute 32-bit base
    movl %eax, 34(%edx)   // set 32-bit base
    leal 32(%edx), %eax   // compute address of descriptor
    lgdt (%eax)           // install 32 bit GDT descriptor

    // turn on protected mode
    mov %cr0, %eax
    orl  $0x1, %eax
    mov %eax, %cr0

    // technically, at this point the current cached descriptors set up
    // during real mode should still "work" until we change them.  ES is
    // an oddball, though - we will die in the next ifetch if ES is zero
    // because it is somehow involved in the fetch, at least in Gem5

    // Now set the code selector (CS)
    ljmp $KERNEL_CS, $(AP_TRAMPOLINE_ADDR + .pmode_is_go - init_smp_boot)

.code32
.pmode_is_go:

    // If we survived set all the other selectors
	
    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %ss
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
	
    // Install page tables - use the same CR3 as BSP
    mov 74(%edx), %eax 
    mov %eax, %cr3

    // enable PAE - required in long mode
    mov %cr4, %eax
    or $(1<<5), %eax
    mov %eax, %cr4

    // enable lme bit in EFER to begin transition to long mode
    movl $0xc0000080, %ecx
    rdmsr
    orl $(1<<8), %eax
    wrmsr

    // enable paging
    mov %cr0, %eax
    orl $(1<<31), %eax
    movl %eax, %cr0

    // Compute the base and limit for the 64 bit GDT descriptor based
    // on the address at which it has been mapped	
    movl $AP_INFO_AREA, %edx
    movw $0x17, 64(%edx)  // set 16-bit limit
    leal 40(%edx), %eax   // compute 64-bit base
    movl %eax, 66(%edx)   // set 32-bit base
    leal 64(%edx), %eax   // compute address of descriptor
    lgdt (%eax)           // install 64 bit GDT descriptor

    // Set the code selector (CS)
    ljmp $KERNEL_CS, $(AP_TRAMPOLINE_ADDR + .lmode_is_go - init_smp_boot)

.code64
.lmode_is_go:

    // if we survived, set all the other selectors
	
    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs

    // Fetch the desired C entry point and argument from the information
    // passed by the BSP
    movq $AP_INFO_AREA, %rdx
    movq 82(%rdx), %rdi
    movq 90(%rdx), %rsi

    // make sure we have caches on - make sure we have ~CD and ~NWT
	 
    movq %cr0, %rax
    movq $0x60000000, %rbx
    notq %rbx
    andq %rbx, %rax	
    movq %rax, %cr0
	
    // Enter the C code with our argument
    callq *%rsi

    // Stall forever if it ever returns
	
l0:
	hlt
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
