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
 * Copyright (c) 2017, Peter Dindae <pdinda@northwestern.edu>
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Peter Dinda <pdinda@northwestern.edu>
 *          Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/*
	Our format is that of a multiboot2 kernel with our
	MB64 extension for HRTs.   We are a relocatable
	64 bit "HRT_EXE" built with position-independence
	The kernel can load us at any page-aligned address.
	We do not have any notion of a highhalf/lowhalf.
*/		
	
.code64
.section .mbhdr
.align 8

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
    // This will be an offset from the begining of the file
    // and the 64 bit version will be included in the MB64 header
    .long __nk_exec_entry
    .long 0


    /* HRT-specific */
    .word 0xf00d, 1       /* loader will ignore otherwise */
    .long 56              /* length of tag in bytes */
    .quad 0x3             /* HRT flags - reloc+exe - no meory prefs*/
    .quad 0               /* GPA Memory to initially map - ignored */
    .quad 0               /* GVA Offset - ignored */
    .quad __nk_exec_entry /* GVA Entry 0 the *offset* of our entry point */
    .quad 0               /* Desired GPA of communication page - ignored */
    .byte 0               /* Desired interrupt vector for communication - ignored */
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 /*padding */

    /* tags end */
    .word 0, 0
    .long 8
hdr_end:

