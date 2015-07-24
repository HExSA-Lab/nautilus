.code64
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

    /* HRT-specific */
    .word 0xf00d, 1 /* bootloader can ignore it if it's not Palacios (flags bit 0 set) */
    .long 12
    .long 0     /* HRT flags */

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

    //movq $boot_stack_end, %rsp

    /* multiboot info is still in rdi and rsi */
    movq $main, %rax
    callq *%rax

    /* shouldn't get here */
    cli
    jmp .


.align 8
gdt64:
    .quad 0x0000000000000000 /* null */
    .quad 0x00a09a0000000000 /* code (note lme bit) */
    .quad 0x00a0920000000000 /* data (most entries don't matter) */

.align 8
GLOBAL(gdtr64)
    .word 23
    .quad gdt64
