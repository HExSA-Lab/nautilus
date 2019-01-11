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
 * Copyright (c) 2016, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */


/*
_nk_real_mode_int is called from long mode and returns to long mode

        -we assume this code and data has been placed at
         NAUT_CONFIG_REAL_MODE_INTERACE_SEGMENT*16
	-the caller must place all arguments into the first 1 MB
	-the caller is assumed to be the ONLY CALLER IN THE ENTIRE SYSTEM
	 FOR THE DURATION OF THIS CALL.  THIS IS NOT REENTERANT
	-the caller must assume all GPRs are smashed, although we will attempt
	 to recover them

	RDI = argument pointer - argument as in include/nautilus/realmode.h

	All registers are saved and restored
*/

LOAD_SEG  = NAUT_CONFIG_REAL_MODE_INTERFACE_SEGMENT
LOAD_ADDR = LOAD_SEG*16

REAL_MODE_ARG_START   = 0x7000  // within the segment, goes up to 0x8000       
REAL_MODE_STACK_START = 0x7000  // within the segment

// Selectors to our long mode GDT that are used for data, etc.
// This what we will reload DS, ES, SS, etc with
// In nautilus, kernel ds = kernel ss = 16
// while kernel cs = 8, and the stub entries for FS/GS are 24
// This code will also save and restore the FS and GS base
// MSR contents
DESC_CODE=8
DESC_DATA=16
DESC_FS=24
DESC_GS=24

FS_BASE_MSR = 0xc0000100
GS_BASE_MSR = 0xc0000101

//
// ENTRY POINT IS HERE
//
.code64
.global _nk_real_mode_int
_nk_real_mode_int:
_seg_start:	          // this is at address 0x0 in the segment

LOCAL_DATA= LOAD_ADDR + (_local_data - .)
RSP   = LOAD_ADDR + (_rsp - .)
GDT   = LOAD_ADDR + (_gdt - .)
GDT64 = LOAD_ADDR + (_gdt64 - .)
GDT16 = LOAD_ADDR + (_gdt16 - .)
IDT64 = LOAD_ADDR + (_idt64 - .)
IDT16 = LOAD_ADDR + (_idt16 - .)
DESC_REAL = LOAD_ADDR + (_desc_real - .)
DESC_LONG = LOAD_ADDR + (_desc_long - .)
DESC_LONG_GDT_OFFSET = (_desc_long - _gdt)
DESC_REAL_GDT_OFFSET = (_desc_real - _gdt)
	
        // stash the registers we will use
	pushq   %rax
	pushq   %rbx
	pushq   %rcx
	pushq   %rdx
	pushq   %rsi
	pushq   %rdi
	pushq   %rbp
	// fs and gs are saved and restored later in real mode
	mov     %es, %ax    // push/pop es not supported in long mode
	push 	%ax
	// we not save ss, ds, and cs, since we
	// assume these are kept stable by the kernel
	// and this is the case in Nautilus

	// we now stash the fs and gs bases which are a special case
	// in long mode.  These do get mangled in the trip through
	// real mode
	mov $FS_BASE_MSR, %rcx
	rdmsr
	push 	%rdx
	push	%rax		
	mov $GS_BASE_MSR, %rcx
	rdmsr
	push 	%rdx
	push 	%rax
	
	
	// Now patch out the int instruction for the vector we want
	// The int instruction's only form takes an immediate operand
	// and since we want to call an arbitrary vector, we
	// must do this with the following self-modifying code
	movb    (%rdi), %al
	movb    %al, LOAD_ADDR+(_int_instr+1-_seg_start)

	// stash the argument structure in our thunk segment
	// so we can see it in real mode
	// 32 bytes, currently, so we will just do 4 quads
	movq  	$(LOAD_ADDR+REAL_MODE_ARG_START), %rbx 
	movq  	(%rdi), %rax
	movq    %rax, 0(%rbx)
	movq  	8(%rdi), %rax
	movq    %rax, 8(%rbx)
	movq    16(%rdi), %rax
	movq	%rax, 16(%rbx)
	movq    24(%rdi), %rax
	movq	%rax, 24(%rbx)
	
	// save rsp in well known location so we can get it back
	movq	%rsp, RSP
	// switch gdt and idt, note that we stash
	// the previous ones in well known locations.
	sgdt	GDT64
	lgdt	GDT16
	sidt	IDT64
	lidt	IDT16

	
	// Push a segment descriptor so we can do a far return
	// to kick us into compatability mode
	// Note  that this is correct despite looking wrong...
	// although a selector is 16 bits,
	// we are using retfq which in long mode assumes both operands
	// are 64 bits
	pushq 	$DESC_REAL_GDT_OFFSET
	pushq   $(_compat - _seg_start)
	// retfq - llvm-as does not like retfq, hence this byte
	.byte 0xcb  	// this is retfq

	// if we survived, we are now in compatability mode
_compat:
	// we now want the assembler to behave as if we are in 16 bit
	// real mode already.  We will soon be there, and the few
	// 32 bit operations we need are doable in this mode as well
.code16
	// turn off paging and protection
	movl   %cr0, %eax
	andl   $0x7FFFFFFE,%eax
	movl   %eax, %cr0

	// and now we are in "unreal" mode
_unreal:	
	// paging and protected mode off at this point, so establish
	// real mode universe (segmemt regs and stack)
	// configure segment registers to reflect the "tiny"
	// model with the context being just this 64K segment
	mov	$LOAD_SEG, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

	// now do a far jump to get CS set
	ljmp	 $LOAD_SEG, $(_real - _seg_start)

	// and we are in real mode
_real:

	// now we will establish the context
	// the arguments (the register state in the argument structure)
	// note that if the user changes cs or ds on us, we are likely
	// to be toast...
	
	movw	REAL_MODE_ARG_START+2, %ax
	movw	REAL_MODE_ARG_START+4, %bx
	movw	REAL_MODE_ARG_START+6, %cx
	movw	REAL_MODE_ARG_START+8, %dx
	movw	REAL_MODE_ARG_START+10, %si
	movw	REAL_MODE_ARG_START+12, %di
	movw	REAL_MODE_ARG_START+14, %sp
	movw	REAL_MODE_ARG_START+16, %bp
	// CS is not restored - you must run in our segment...
	//movw	REAL_MODE_ARG_START+18, %cs
	movw	REAL_MODE_ARG_START+20, %ds
	movw	REAL_MODE_ARG_START+22, %ss
	movw	REAL_MODE_ARG_START+24, %fs
	movw	REAL_MODE_ARG_START+26, %gs
	movw	REAL_MODE_ARG_START+28, %es
_int_instr:
	//2 bytes long, second byte is the vector, which we have patched
	//earlier - there is no register form of this instruction
	int $0xff

	// now we copy out register changes that occured due
	// to the real mode code,
	movw	%ax, REAL_MODE_ARG_START+2
	movw	%bx, REAL_MODE_ARG_START+4
	movw	%cx, REAL_MODE_ARG_START+6
	movw	%dx, REAL_MODE_ARG_START+8
	movw	%si, REAL_MODE_ARG_START+10
	movw	%di, REAL_MODE_ARG_START+12
	movw	%sp, REAL_MODE_ARG_START+14
	movw	%bp, REAL_MODE_ARG_START+16
	movw	%cs, REAL_MODE_ARG_START+18
	movw	%ds, REAL_MODE_ARG_START+20
	movw	%ss, REAL_MODE_ARG_START+22
	movw	%fs, REAL_MODE_ARG_START+24
	movw	%gs, REAL_MODE_ARG_START+26
	movw	%es, REAL_MODE_ARG_START+28
	pushf
	popw    REAL_MODE_ARG_START+30

	// We restore FS and GS here since this
	// is not legal in long mode
	movw    $DESC_FS, %ax
	mov     %ax, %fs
	movw    $DESC_GS, %ax
	mov     %ax, %gs
	
_unreal_again:	

	// begin switch back to long mode by
	// turning paging and protection back on
	// if the real mode code monkeyed with
	// the CRs, we may be toast at this point
	movl	%cr0, %eax
	orl 	$0x80000001, %eax
	movl	%eax, %cr0

	// protection and paging is on again

	// Now restore CS with a long jump
	// gas does not like this in code16, so it is hand-coded here
	// ljmp $DESC_LONG_GDT_OFFSET, $(LOAD_ADDR + _compat_again - _seg_start)
	.byte 0x66
	.byte 0xea
	.4byte LOAD_ADDR + _compat_again -  _seg_start
	.2byte DESC_LONG_GDT_OFFSET


.code64
_compat_again:
	// We are now back in compatability mode
	// Now begin transition back to long mode
	
	// We still have CS loaded as if we are in compat mode
	// so we need to restore the gdt based on that,
	// hence this odd instruction form
	lgdt   	%cs:LOAD_ADDR+(_gdt64-_seg_start)

	// Now we restore our segment registers
	// This assumes the selectors are correctly
	// defined above.  
	mov 	$DESC_DATA, %ax 
	mov    	%ax, %ds
	mov     %ax, %es
	mov     %ax, %ss
	// fs and gs have been restored already within
	// the real mode code

	// Now we switch back to the original stack
	movq    RSP, %rsp

	// We now want to restore CS to point to the
	// entry in the original GDT.   To do this we
	// need another far return
	// Note that this is correct despite looking
	// wrong.   See the comment for the previous retf
	pushq 	$DESC_CODE
	pushq   $(LOAD_ADDR+_long_orig_gdt - _seg_start)
	.byte 0xcb   // this is retfq - see above

_long_orig_gdt:
	// Now we are back to using the original GDT's CS
	// and other segments.
	// Reestablish original IDT
	lidt    IDT64

	// We need an indirect jump to get the CS change
	// to "take", but this may just be my misunderstanding
	movabsq $LOAD_ADDR+(_out-_seg_start), %rax
	jmp     *%rax

	
_long_again:	
_out:
	// And we are back to long mode

	// while gs and fs were restored while we were in
	// real mode - we do need to write their MSrs, though
	// since changes of these registers while in real mode
	// apparently also change their base msrs
	pop	%rax		
	pop 	%rdx
	mov $GS_BASE_MSR, %rcx
	wrmsr
	pop 	%rax
	pop 	%rdx
	mov $FS_BASE_MSR, %rcx
	wrmsr

	// now restore the registers we rode in on

	// We still need to handle es, but the
	// other seg registers were done previously
	pop 	%ax
	mov	%ax, %es

	// Now restore the GPRs
	popq    %rbp
	popq	%rdi
	popq	%rsi
	popq	%rdx
	popq	%rcx
	popq	%rbx
	popq	%rax

	// Copy out the results from the interrupt
	// call - copy from the real mode arguments section
	// of the segment back to where ever the arguments
	// pointer argument (%rdi) indicates
	pushq   %rax
	pushq   %rbx
	movq    $(LOAD_ADDR+REAL_MODE_ARG_START), %rbx
	movq  	(%rbx), %rax
	movq    %rax, 0(%rdi)
	movq  	8(%rbx), %rax
	movq    %rax, 8(%rdi)
	movq    16(%rbx), %rax
	movq	%rax, 16(%rdi)
	movq    24(%rbx), %rax
	movq	%rax, 24(%rdi)

	
	// restore registers we used for this copy operation
	popq 	%rbx
	popq	%rax

	// and we are out of here
	retq
	
// DO NOT CHANGE THE ORDERING OR ALIGNMENT OF ANY OF THIS
.align 16
_local_data:	
.align 16
// Stash of long mode rsp while in real mode
_rsp:
.8byte  0
.align 16
// Stash of long mode gdt while in real mode
_gdt64:
.2byte   0
.8byte   0
.align 16
// Descriptor of gdt used in transition to real mode
_gdt16:
.2byte _gdtend-_gdt-1
.4byte GDT,0
.align 16
// GDT table used in transition to long mode
_gdt:
.4byte 0,0                //null descriptor
_desc_long:	
.4byte 0x0,0x209800      //64 bit long mode cs // 8
_desc_real:
.4byte  0x0000FFFF + (LOAD_ADDR&0xffff)<<16, 0x00009800 + LOAD_ADDR>>16      //16 bit real mode cs (modify base if needed!)
_gdtend:
.align 16
_idt64:
.2byte 0
.8byte 0
.align 16
_idt16:
.2byte 0x3FF
.8byte 0

.global _nk_real_mode_int_END
_nk_real_mode_int_END:

