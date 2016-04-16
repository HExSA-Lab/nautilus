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
#include <arch/k1om/xeon_phi.h>

void 
phi_card_is_up (void)
{
    /* TODO: SCIF */
}


#define PHI_FB_CTRL_REG_ADDR 0xb8fa0

#define OUTPUT_AVAIL_REG_OFFSET 0x0
#define OUTPUT_DRAWN_REG_OFFSET 0x1
#define CHAR_REG_OFFSET         0x2
#define CURSOR_REG_OFFSET       0x3
#define LINE_REG_OFFSET         0x4

typedef enum {
    TYPE_NO_UPDATE = 0,
    TYPE_CHAR_DRAWN,
    TYPE_LINE_DRAWN,
    TYPE_CURSOR_UPDATE,
    TYPE_SCREEN_REDRAW,
    TYPE_CONSOLE_SHUTDOWN,
    TYPE_SCROLLUP,
    TYPE_INVAL
} update_type_t;

uint32_t
phi_cons_read_reg(uint32_t off)
{
   uint32_t* addr = (uint32_t*)PHI_FB_CTRL_REG_ADDR;
   return *(volatile uint32_t*)(addr+off);
}


void 
phi_cons_write_reg (uint32_t off, uint32_t val)
{
    uint32_t* addr = (uint32_t*)PHI_FB_CTRL_REG_ADDR;
    *(volatile uint32_t*)(addr+off) = val;
}


void
phi_cons_wait_for_out_cmpl (void)
{
    /* wait for host to tell us that the char is in the screen */
    while (phi_cons_read_reg(OUTPUT_DRAWN_REG_OFFSET) == 0);

    phi_cons_write_reg(OUTPUT_DRAWN_REG_OFFSET, 0);
}


void
phi_cons_notify_redraw (void)
{
    phi_cons_write_reg(OUTPUT_AVAIL_REG_OFFSET, TYPE_SCREEN_REDRAW);

    phi_cons_wait_for_out_cmpl();
}


/* tell host to save the first line in its buffer if it needs to */
void
phi_cons_notify_scrollup (void)
{
    phi_cons_write_reg(OUTPUT_AVAIL_REG_OFFSET, TYPE_SCROLLUP);

    phi_cons_wait_for_out_cmpl();
}


void 
phi_cons_notify_char_write (uint16_t x, uint16_t y)
{
    uint32_t coords = (uint32_t)(x | y << 16);

    /* where did we draw the char? */
    phi_cons_write_reg(CHAR_REG_OFFSET, coords);

    /* we have output ready to be drawn */
    phi_cons_write_reg(OUTPUT_AVAIL_REG_OFFSET, TYPE_CHAR_DRAWN);

    phi_cons_wait_for_out_cmpl();
}

void 
phi_cons_notify_line_draw (unsigned row)
{
    /* which row did we write to? */
    phi_cons_write_reg(LINE_REG_OFFSET, row);

    /* we have output ready to be drawn */
    phi_cons_write_reg(OUTPUT_AVAIL_REG_OFFSET, TYPE_LINE_DRAWN);

    phi_cons_wait_for_out_cmpl();
}

void 
phi_cons_write_fb_and_notify (uint16_t x, uint16_t y, char c, uint8_t color)
{
    const size_t index = y * VGA_WIDTH + x;
    term.buf[index] = make_vgaentry(c, color);
    phi_cons_notify_char_write(x, y);
}

    
