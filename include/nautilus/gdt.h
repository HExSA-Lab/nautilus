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
