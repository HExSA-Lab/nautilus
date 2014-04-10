.code32
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
    .long 12
    .long nautilus_entry
    .long 0

    /* tags end */
    .word 0, 0
    .long 8
hdr_end:

.section .boot
.globl nautilus_entry

/**
 * our entry point into the kernel, this is where
 * GRUB2 will drop us
 */
nautilus_entry:

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
    movl $boot_stack, %esp

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

    movq $boot_stack, %rcx
    addq $0xffffffff80000000, %rcx
    movq %rcx, %rsp

    /* this is for when we unmap the lower mappings */
    movq $gdtr64high, %rax
    lgdt (%rax)

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
    movl %eax, 0xff8(%ebx) /* higher half mapping */

    /* PDPT[0] -> PDT */
    movl $pd, %eax
    orl $0x3, %eax
    movl $pdpt, %ebx
    movl %eax, (%ebx)
    movl %eax, 0xff0(%ebx)  /* higher half mapping */

    /* PDT[0] -> PT */
    movl $pt, %eax
    orl $0x3, %eax
    movl $pd, %ebx
    movl %eax, (%ebx)

    /* identity map the first 2 MB */
    movl $512, %ecx
    movl $pt, %edx
    movl $0x3, %eax
.write_pte:
    movl %eax, (%edx)
    addl $0x1000, %eax
    addl $0x8, %edx
    loop .write_pte

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
    movl %eax, %cr0

    ret
     
.align 8

gdt32:
    .quad 0x0000000000000000 /* null */
    .quad 0x00cf9a000000ffff /* code */
    .quad 0x00cf92000000ffff /* data */

gdt64:
    .quad 0x0000000000000000 /* null */
    .quad 0x00a09a0000000000 /* code (note lme bit) */
    .quad 0x00a0920000000000 /* data (most entries don't matter) */

.align 8

gdtr32:
    .word 23
    .long gdt32

gdtr64:
    .word 23
    .long gdt64
    .long 0

gdtr64high:
    .word 23
    .quad gdt64 + 0xffffffff80000000
