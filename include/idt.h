#ifndef __IDT_H__
#define __IDT_H__

#include <types.h>
#include <gdt.h>
#include <string.h>

#define NUM_IDT_ENTRIES 256
#define NUM_EXCEPTIONS  32

#define ADDR_LO(x) ((ulong_t)(x) & 0xFFFF)
#define ADDR_MI(x) (((ulong_t)(x) >> 16) & 0xFFFF)
#define ADDR_HI(x) ((ulong_t)(x) >> 32)


enum {
    GATE_TYPE_INT  = 0xe,
    GATE_TYPE_TRAP = 0xf,
    GATE_TYPE_CALL = 0xc,
    GATE_TYPE_TASK = 0x5,
};

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
} __attribute__((packed));


struct idt_desc {
    uint16_t size;
    uint64_t base_addr;
} __attribute__((packed));


int setup_idt(void);

static inline void
write_gate_desc (struct   gate_desc64 * idt,
                 int      gate, 
                 uint32_t type, 
                 void     * func,
                 uint32_t dpl,
                 uint32_t ist, 
                 uint32_t seg)
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


#endif
