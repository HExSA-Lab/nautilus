
.section .smp_boot
.code16
.globl init_smp_boot
init_smp_boot:
    cli
    movl $smp_gdtr32, %eax
    lgdt (%eax)
    movl %cr0, %eax
    or $1, %al               /* set PE bit (protection enable) */
    movl %eax, %cr0

    ljmp $0x8, $.pmode_is_go

.pmode_is_go:
    hlt
    




smp_gdt32:
    .quad 0x0000000000000000 /* null */
    .quad 0x00cf9a000000ffff /* code */
    .quad 0x00cf92000000ffff /* data */

smp_gdtr32:
    .word 23
    .long smp_gdt32


