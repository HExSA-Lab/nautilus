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
#ifndef __XEON_PHI_H__
#define __XEON_PHI_H__

#include <nautilus/naut_types.h>

#define PHI_SBOX_BASE   0x8007D0000ULL
#define PHI_BOOT_OK_REG 0xAB28


void phi_card_is_up(void);

static inline uint32_t
phi_sbox_read (uint32_t offset)
{
    return *(volatile uint32_t*)((uint32_t*) PHI_SBOX_BASE + offset);
}

static inline void
phi_sbox_write (uint32_t offset, uint32_t val)
{
    *(volatile uint32_t*)((uint32_t*)PHI_SBOX_BASE + offset) = val;
}

void phi_cons_get_cursor(uint8_t * x, uint8_t * y);
void phi_cons_set_cursor(uint8_t x, uint8_t y);
void phi_cons_notify_scrollup(void);
void phi_cons_notify_line_draw(unsigned row);
void phi_cons_notify_redraw(void);

void phi_cons_clear_screen(void);
void phi_cons_init(void);
void phi_cons_print(char *buf);
void phi_cons_putchar(char c);


#endif
