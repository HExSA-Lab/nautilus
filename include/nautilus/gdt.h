#ifndef __GDT_H__
#define __GDT_H__

#ifndef __ASSEMBLER__

#include <nautilus/intrinsics.h>

struct gdt_desc64 {
    uint16_t limit;
    uint64_t base;
} __packed;

struct gdt_desc32 {
    uint16_t limit;
    uint32_t base;
} __packed;


static inline void
lgdt32 (const struct gdt_desc32 * g)
{
    asm volatile ("lgdt %0" :: "m" (*g));
}

static inline void
lgdt64 (const struct gdt_desc64 * g) 
{
    asm volatile ("lgdt %0" :: "m" (*g)); 
}


#endif /* !__ASSEMBLER__ */

#define KERNEL_CS 8
#define KERNEL_DS 16
#define KERNEL_SS KERNEL_DS

#endif
