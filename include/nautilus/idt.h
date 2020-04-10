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
#ifndef __IDT_H__
#define __IDT_H__

#include <asm/lowlevel.h>

#ifdef __cplusplus 
extern "C" {
#endif

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
#define MF_EXCP 16 /* FPU error */
#define AC_EXCP 17 /* alignment check */
#define MC_EXCP 18 /* machine check exception */
#define XM_EXCP 19 /* SIMD FPU exception */
#define VE_EXCP 20 /* virtualization exception */
#define SE_EXCP 30 /* Security exception */


/* 0s for exceptions that push an error code on the stack */
#define ERR_CODE_EXCP_MASK   ~0x00427d00

#ifndef __ASSEMBLER__

#include <nautilus/naut_types.h>
#include <nautilus/gdt.h>
#include <nautilus/intrinsics.h>
#include <nautilus/naut_string.h>


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
typedef ulong_t excp_err_t;

struct gate_desc64 {
    union {
        struct {
            uint64_t val_lo;
            uint64_t val_hi;
        };
        struct {
            uint16_t target_off_lo;
            uint16_t target_css;
            uint8_t  ist   : 5;
            uint8_t  rsvd0 : 3;
            uint8_t  type  : 4;
            uint8_t  rsvd1 : 1;
            uint8_t  dpl   : 2;
            uint8_t  p     : 1;
            uint16_t target_off_mid;
            uint32_t target_off_hi;
            uint32_t rsvd2;
        } __packed;
    } __packed;
} __packed;


struct idt_desc {
    uint16_t size;
    uint64_t base_addr;
} __packed;


int setup_idt(void);

int idt_assign_entry(ulong_t entry, ulong_t handler_addr, ulong_t state_addr);
int idt_get_entry(ulong_t entry, ulong_t *handler_addr, ulong_t *state_addr);

int idt_find_and_reserve_range(ulong_t numentries, int aligned, ulong_t *first);

int null_excp_handler(excp_entry_t * excp, excp_vec_t vec, void * state_addr);
int null_irq_handler(excp_entry_t * excp, excp_vec_t vector, void * state_addr);

static inline void
write_gate_desc (struct   gate_desc64 * idt,
                 uint32_t gate, 
                 gate_type_t type, 
                 const void      * func,
                 uint32_t    dpl,
                 uint32_t    ist, 
                 uint32_t    seg)
{
    idt[gate].target_off_lo     = ADDR_LO(func);
    idt[gate].target_css        = seg;
    idt[gate].ist               = ist;
    idt[gate].rsvd0             = 0;
    idt[gate].type              = type;
    idt[gate].rsvd1             = 0;
    idt[gate].dpl               = dpl;
    idt[gate].p                 = 1;
    idt[gate].target_off_mid    = ADDR_MI(func);
    idt[gate].target_off_hi     = ADDR_HI(func);
    idt[gate].rsvd2             = 0;
}


static inline void
set_intr_gate (struct gate_desc64 * idt, unsigned gate, const void * func) 
{
    write_gate_desc(idt, gate, GATE_TYPE_INT, func, 0, 0, KERNEL_CS);
}


static inline void 
lidt (const struct idt_desc * d) 
{
        asm volatile ("lidt %0" :: "m" (*d));
}

#endif /* !__ASSEMBLER__ */

#ifdef __cplusplus 
}
#endif


#endif /* __IDT_H__ */
