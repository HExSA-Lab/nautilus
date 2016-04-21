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
#include <nautilus/spinlock.h>
#include <dev/vga.h>

static struct {
  size_t row;
  size_t col;
  uint8_t color;
  volatile uint16_t * fb;
  spinlock_t lock;
} phi_term;


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


static uint32_t
phi_cons_read_reg(uint32_t off)
{
   uint32_t* addr = (uint32_t*)PHI_FB_CTRL_REG_ADDR;
   return *(volatile uint32_t*)(addr+off);
}


static void 
phi_cons_write_reg (uint32_t off, uint32_t val)
{
    uint32_t* addr = (uint32_t*)PHI_FB_CTRL_REG_ADDR;
    *(volatile uint32_t*)(addr+off) = val;
}


static void
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
phi_cons_set_cursor (uint8_t x, uint8_t y)
{
    uint16_t pos = y*VGA_WIDTH + x;

    phi_term.col = x;
    phi_term.row = y;

    // TODO: write to the faux cursor register
}


void
phi_cons_get_cursor (uint8_t * x, uint8_t * y)
{
    *x = phi_term.col;
    *y = phi_term.row;
}


static void 
phi_cons_scrollup (void)
{
    int i;
    volatile uint16_t *buf = phi_term.fb;

    for (i = 0; i < VGA_WIDTH*(VGA_HEIGHT-1); i++) {
        buf[i] = buf[i+VGA_WIDTH];
    }

    for (i = VGA_WIDTH*(VGA_HEIGHT-1); i < VGA_WIDTH*VGA_HEIGHT; i++) {
        buf[i] = vga_make_entry(' ', phi_term.color);
    }

    phi_cons_notify_scrollup();
}


static void 
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


static void 
phi_cons_write_fb_and_notify (uint16_t x, uint16_t y, char c, uint8_t color)
{
    const size_t index = y * VGA_WIDTH + x;
    phi_term.fb[index] = vga_make_entry(c, color);
    phi_cons_notify_char_write(x, y);
}


void 
phi_cons_putchar (char c)
{
    if (c == '\n') {
        phi_term.col = 0;

        phi_cons_notify_line_draw(phi_term.row);

        if (++phi_term.row == VGA_HEIGHT) {
            phi_cons_scrollup();
            phi_term.row--;
        }
    } else {

        // KCH: NOTE: We may not want to notify here
        phi_cons_write_fb_and_notify(phi_term.col, phi_term.row, c, phi_term.color);

        if (++phi_term.col == VGA_WIDTH) {
            phi_term.col = 0;

            phi_cons_notify_line_draw(phi_term.row);
            
            if (++phi_term.row == VGA_HEIGHT) {
                phi_cons_scrollup();
                phi_term.row--;
            }
        }
    }

    phi_cons_set_cursor(phi_term.col, phi_term.row);
}


void 
phi_cons_print (char *buf)
{
    while (*buf) {
        phi_cons_putchar(*buf);
        buf++;
    }
}


void
phi_cons_clear_screen (void)
{
    size_t x,y;

    for (y = 0; y < VGA_HEIGHT; y++) {
        for (x = 0; x < VGA_WIDTH; x++) {
            const size_t idx = y * VGA_WIDTH + x;
            phi_term.fb[idx] = vga_make_entry(' ', phi_term.color);
        }
    }

    phi_cons_notify_redraw();
}


void
phi_cons_init (void)
{
    phi_term.row   = 0;
    phi_term.col   = 0;
    phi_term.fb    = (volatile uint16_t*)VGA_BASE_ADDR;
    phi_term.color = vga_make_entry(COLOR_LIGHT_GREY, COLOR_BLACK);

    spinlock_init(&(phi_term.lock));

    size_t x,y;

    for (y = 0; y < VGA_HEIGHT; y++) {
        for (x = 0; x < VGA_WIDTH; x++) {
            const size_t idx = y * VGA_WIDTH + x;
            phi_term.fb[idx] = vga_make_entry(' ', phi_term.color);
        }
    }

    phi_cons_notify_redraw();
}

    
