#ifndef __IDT_H__
#define __IDT_H__

#include <asm/lowlevel.h>

#define NUM_IDT_ENTRIES 256
#define NUM_EXCEPTIONS  32

#define DE_EXCP 0 /* Divide Error */
#define DB_EXCP 1 /* RESERVED */
#define NMI_INT 2 /* NMI interrupt */
#define BP_EXCP 3 /* breakpoint exception */
#define OF_EXCP 4 /* overflow */
#define BR_EXCP 5 /* bound range exceeded */
#define UD_EXCP 6 /* undefined opcode */
#define NM_EXCP 7 /* device not available */
#define DF_EXCP 8 /* double fault */
#define CP_EXCP 9 /* coprocessor segment overrun */
#define TS_EXCP 10 /* invalid TSS */
#define NP_EXCP 11 /* segment not present */
#define SS_EXCP 12 /* stack-segment fault */
#define GP_EXCP 13 /* GPF */
#define PF_EXCP 14 /* page fault */
#define MF_EXCP 15 /* FPU error */
#define AC_EXCP 16 /* alignment check */
#define MC_EXCP 17 /* machine check exception */
#define XM_EXCP 18 /* SIMD FPU exception */
#define VE_EXCP 19 /* virtualization exception */


/* 0s for exceptions that push an error code on the stack */
#define ERR_CODE_EXCP_MASK   ~0x00027d00

#define MAKE_IDT_HANDLER(x)       \
.align 2;                         \
early_excp_handler_##x:;          \
.if (ERR_CODE_EXCP_MASK >> x) & 1;\
    pushq $0;                     \
.else;                            \
    GEN_NOP(NOP_2BYTE);           \
.endif;                           \
pushq $##x;                       \
jmp early_excp_common;


#ifndef __ASSEMBLER__

#include <types.h>
#include <gdt.h>
#include <intrinsics.h>
#include <string.h>


extern const uint8_t early_excp_handlers[NUM_EXCEPTIONS][10];

#define ADDR_LO(x) ((ulong_t)(x) & 0xFFFF)
#define ADDR_MI(x) (((ulong_t)(x) >> 16) & 0xFFFF)
#define ADDR_HI(x) ((ulong_t)(x) >> 32)


typedef enum {
    GATE_TYPE_INT  = 0xe,
    GATE_TYPE_TRAP = 0xf,
    GATE_TYPE_CALL = 0xc,
    GATE_TYPE_TASK = 0x5,
} gate_type_t;


struct excp_entry_state {
    ulong_t error_code;
    ulong_t rip;
    ulong_t cs;
    ulong_t rflags;
    ulong_t rsp;
    ulong_t ss;
} __packed;

typedef struct excp_entry_state excp_entry_t;
typedef ulong_t excp_vec_t;

struct gate_desc64 {
    union {
        struct {
            uint64_t val_lo;
            uint64_t val_hi;
        };
        struct {
            uint16_t target_off_lo;
            uint16_t target_css;
            uint8_t  ist  : 2;
            uint8_t  rsvd0 : 5;
            uint8_t  type : 5;
            uint8_t  dpl  : 2;
            uint8_t  p    : 1;
            uint16_t target_off_mid;
            uint32_t target_off_hi;
            uint32_t rsvd1;
        };
    };
} __packed;


struct idt_desc {
    uint16_t size;
    uint64_t base_addr;
} __packed;


int setup_idt(void);
int null_excp_handler(void);

static inline void
write_gate_desc (struct   gate_desc64 * idt,
                 gate_type_t gate, 
                 uint32_t    type, 
                 void      * func,
                 uint32_t    dpl,
                 uint32_t    ist, 
                 uint32_t    seg)
{
    struct gate_desc64 d;

    d.target_off_lo     = ADDR_LO(func);
    d.target_css        = seg;
    d.ist               = ist;
    d.rsvd0             = 0;
    d.type              = type;
    d.dpl               = dpl;
    d.p                 = 1;
    d.target_off_mid    = ADDR_MI(func);
    d.target_off_hi     = ADDR_HI(func);

    /* COPY IT */
    memcpy(&idt[gate], &d, sizeof(struct gate_desc64));
}


static inline void
set_intr_gate (struct gate_desc64 * idt, int gate, void * func) 
{
    write_gate_desc(idt, gate, GATE_TYPE_INT, func, 0, 0, KERNEL_CS);
}


static inline void 
lidt (const struct idt_desc * d) 
{
        asm volatile ("lidt %0" :: "m" (*d));
}

#endif /* !__ASSEMBLER__ */


#endif /* __IDT_H__ */
