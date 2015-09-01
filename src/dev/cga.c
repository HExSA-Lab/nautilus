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
#include <nautilus/nautilus.h>
#include <nautilus/cga.h>
#include <nautilus/naut_string.h>
#include <nautilus/spinlock.h>
#include <nautilus/cpu.h>

static struct term_info {
    size_t    row;
    size_t    col;
    uint8_t   color;
    volatile uint16_t * buf;
    spinlock_t lock;
} term;


static uint16_t 
make_vgaentry (char c, uint8_t color)
{
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | color16 << 8;
}


/* BEGIN XEON PHI SPECIFIC */

#ifdef NAUT_CONFIG_XEON_PHI

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

static inline uint32_t
phi_cons_read_reg(uint32_t off)
{
   uint32_t* addr = (uint32_t*)PHI_FB_CTRL_REG_ADDR;
   return *(volatile uint32_t*)(addr+off);
}


static inline void 
phi_cons_write_reg (uint32_t off, uint32_t val)
{
    uint32_t* addr = (uint32_t*)PHI_FB_CTRL_REG_ADDR;
    *(volatile uint32_t*)(addr+off) = val;
}


static void
wait_for_out_cmpl (void)
{
    /* wait for host to tell us that the char is in the screen */
    while (phi_cons_read_reg(OUTPUT_DRAWN_REG_OFFSET) == 0);

    phi_cons_write_reg(OUTPUT_DRAWN_REG_OFFSET, 0);
}


static void
phi_notify_redraw (void)
{
    phi_cons_write_reg(OUTPUT_AVAIL_REG_OFFSET, TYPE_SCREEN_REDRAW);

    wait_for_out_cmpl();
}


/* tell host to save the first line in its buffer if it needs to */
static void
phi_notify_scrollup (void)
{
    phi_cons_write_reg(OUTPUT_AVAIL_REG_OFFSET, TYPE_SCROLLUP);

    wait_for_out_cmpl();
}


static void 
phi_notify_char_write (uint16_t x, uint16_t y)
{
    uint32_t coords = (uint32_t)(x | y << 16);

    /* where did we draw the char? */
    phi_cons_write_reg(CHAR_REG_OFFSET, coords);

    /* we have output ready to be drawn */
    phi_cons_write_reg(OUTPUT_AVAIL_REG_OFFSET, TYPE_CHAR_DRAWN);

    wait_for_out_cmpl();
}

static void 
phi_notify_line_draw (unsigned row)
{
    /* which row did we write to? */
    phi_cons_write_reg(LINE_REG_OFFSET, row);

    /* we have output ready to be drawn */
    phi_cons_write_reg(OUTPUT_AVAIL_REG_OFFSET, TYPE_LINE_DRAWN);

    wait_for_out_cmpl();
}

static void 
phi_write_fb_and_notify (uint16_t x, uint16_t y, char c, uint8_t color)
{
    const size_t index = y * VGA_WIDTH + x;
    term.buf[index] = make_vgaentry(c, color);
    phi_notify_char_write(x, y);
}

#endif /* !NAUT_CONFIG_XEON_PHI */
    

/* END XEON PHI SPECIFIC */


static void
hide_cursor (void)
{
    //outw(0x200a, 0x3d4);
}
 
 
static uint8_t 
make_color (enum vga_color fg, enum vga_color bg) 
{
    return fg | bg << 4;
}


void term_setpos (size_t x, size_t y)
{
    uint8_t flags = spin_lock_irq_save(&(term.lock));
    term.row = y;
    term.col = x;
    spin_unlock_irq_restore(&(term.lock), flags);
}


void term_getpos (size_t* x, size_t* y)
{
    *x = term.col;
    *y = term.row;
}


void 
term_init (void)
{
    term.row = 0;
    term.col = 0;
    term.color = make_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    term.buf = (uint16_t*) VGA_BASE_ADDR;
    spinlock_init(&(term.lock));
    size_t y;
    size_t x;
#ifndef NAUT_CONFIG_HVM_HRT
    for ( y = 0; y < VGA_HEIGHT; y++ )
    {
        for ( x = 0; x < VGA_WIDTH; x++ )
        {
            const size_t index = y * VGA_WIDTH + x;
            term.buf[index] = make_vgaentry(' ', term.color);
        }
    }
#endif

#ifdef NAUT_CONFIG_XEON_PHI
    phi_notify_redraw();
#endif

    hide_cursor();
}
 

uint8_t 
term_getcolor (void)
{
    return term.color;
}


void 
term_setcolor (uint8_t color)
{
    uint8_t flags = spin_lock_irq_save(&(term.lock));
    term.color = color;
    spin_unlock_irq_restore(&(term.lock), flags);
}
 

void 
debug_putc (char c)
{ 
    outb(c, 0xc0c0);
}


void 
debug_puts (const char * s) 
{
    while (*s) {
        debug_putc(*s);
        s++;
    }
    debug_putc('\n');
}


/* NOTE: should be holding a lock while in this function */
void 
term_putc (char c, uint8_t color, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;
    term.buf[index] = make_vgaentry(c, color);
}
 

inline void
term_clear (void) 
{
    uint8_t flags = spin_lock_irq_save(&(term.lock));
    size_t i;
    for (i = 0; i < VGA_HEIGHT*VGA_WIDTH; i++) {
        term.buf[i] = make_vgaentry(' ', term.color);
    }

#ifdef NAUT_CONFIG_XEON_PHI
    phi_notify_redraw();
#endif

    spin_unlock_irq_restore(&(term.lock), flags);
}


/* NOTE: should be holding a lock while in this function */
static void 
term_scrollup (void) 
{
    int i;
    int n = (((VGA_HEIGHT-1)*VGA_WIDTH*2)/sizeof(long));
    int lpl = (VGA_WIDTH*2)/sizeof(long);
    long * pos = (long*)term.buf;
    
#ifdef NAUT_CONFIG_XEON_PHI
    phi_notify_scrollup();
#endif

    for (i = 0; i < n; i++) {
        *pos = *(pos + lpl);
        ++pos;
    }

    size_t index = (VGA_HEIGHT-1) * VGA_WIDTH;
    for (i = 0; i < VGA_WIDTH; i++) {
        term.buf[index++] = make_vgaentry(' ', term.color);
    }

#ifdef NAUT_CONFIG_XEON_PHI
    phi_notify_redraw();
#endif

}


void 
putchar (char c)
{
    uint8_t flags = spin_lock_irq_save(&(term.lock));
    
    if (c == '\n') {
        term.col = 0;

#ifdef NAUT_CONFIG_XEON_PHI
        phi_notify_line_draw(term.row);
#endif

        if (++term.row == VGA_HEIGHT) {
            term_scrollup();
            term.row--;
        }
        spin_unlock_irq_restore(&(term.lock), flags);
        return;
    }

    term_putc(c, term.color, term.col, term.row);

    if (++term.col == VGA_WIDTH) {
        term.col = 0;
#ifdef NAUT_CONFIG_XEON_PHI
        phi_notify_line_draw(term.row);
#endif
        if (++term.row == VGA_HEIGHT) {
            term_scrollup();
            term.row--;
        }
    }

    spin_unlock_irq_restore(&(term.lock), flags);
}


int puts
(const char *s)
{
    while (*s)
    {
        putchar(*s);
        s++;
    }
    putchar('\n');
    return 0;
}
 

static void 
get_cursor (unsigned row, unsigned col) 
{
    return;
}



void 
term_print (const char* data)
{
    int i;
    size_t datalen = strlen(data);
    for (i = 0; i < datalen; i++) {
        putchar(data[i]);
    }
    
}
