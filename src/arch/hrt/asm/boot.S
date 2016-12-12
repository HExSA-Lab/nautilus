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
.code64
.section .mbhdr
.align 8

#include <asm/lowlevel.h>
#include <arch/hrt/hrt.h>

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
    .long multiboot_hdr - NAUT_CONFIG_HRT_HIHALF_OFFSET
    .long _loadStart - NAUT_CONFIG_HRT_HIHALF_OFFSET
    .long _loadEnd - NAUT_CONFIG_HRT_HIHALF_OFFSET
    .long _bssEnd - NAUT_CONFIG_HRT_HIHALF_OFFSET

    /* entry point */
    .word 3, 0
    .long 16
    .long nautilus_entry - NAUT_CONFIG_HRT_HIHALF_OFFSET
    .long 0


/* TODO: slap these into a NAUT_CONFIG */

    /* HRT-specific */
    .word 0xf00d, 1                         /* bootloader can ignore it if it's not Palacios (flags bit 0 set) */
    .long 56                                /* length of tag in bytes */
    .quad HRT_FLAGS                         /* HRT flags (0x200 = want 2 MB Pages) */
    .quad NAUT_CONFIG_HRT_GPA_MAP_REQ       /* GPA Memory to initially map (8 GB)    */
    .quad NAUT_CONFIG_HRT_HIHALF_OFFSET     /* GVA Offset - address where GPA 0 is mapped */
    .quad nautilus_entry                    /* GVA Entry 0=> use MB entry */
    .quad NAUT_CONFIG_HRT_COMM_PAGE         /* Desired GPA of communication page */
    .byte NAUT_CONFIG_HVM_INT_VEC           /* Desired interrupt vector for communication */
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 /*padding */

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
    movq %rbx, %rdi
    movq %rax, %rsi

    /* multiboot info is still in rdi and rsi */
    movabs $main, %rax
    callq *%rax

    /* shouldn't get here */
    cli
    jmp .


.align 8
gdt64:
    .quad 0x0000000000000000 /* null */
    .quad 0x00af9a000000ffff /* code (note lme bit) */
    .quad 0x00af92000000ffff /* data (most entries don't matter) */

.align 8
GLOBAL(gdtr64)
    .word 23
    .quad gdt64
