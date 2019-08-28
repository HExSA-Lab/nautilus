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
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2016, Peter Dinda <pdinda@u.northwestern.edu>
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter Dinda <pdinda@northestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __X64_VGA__
#define __X64_VGA__

#include <nautilus/naut_types.h>
#include <nautilus/naut_string.h>

#define VGA_BASE_ADDR 0xb8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

enum vga_color
{
    COLOR_BLACK = 0,
    COLOR_BLUE = 1,
    COLOR_GREEN = 2,
    COLOR_CYAN = 3,
    COLOR_RED = 4,
    COLOR_MAGENTA = 5,
    COLOR_BROWN = 6,
    COLOR_LIGHT_GREY = 7,
    COLOR_DARK_GREY = 8,
    COLOR_LIGHT_BLUE = 9,
    COLOR_LIGHT_GREEN = 10,
    COLOR_LIGHT_CYAN = 11,
    COLOR_LIGHT_RED = 12,
    COLOR_LIGHT_MAGENTA = 13,
    COLOR_LIGHT_BROWN = 14,
    COLOR_WHITE = 15,
};

uint16_t vga_make_entry (char c, uint8_t attr);
uint8_t  vga_make_color (enum vga_color fg, enum vga_color bg);
void     vga_set_cursor(uint8_t x, uint8_t y);
void     vga_get_cursor(uint8_t *x, uint8_t *y);

void     vga_early_init();
void     vga_init();

void     vga_init_screen();

static inline void vga_write_screen(uint8_t x, uint8_t y, uint16_t val)
{
    uint16_t *addr = ((uint16_t *)VGA_BASE_ADDR)+y*VGA_WIDTH+x;
    __asm__ __volatile__ (" movw %0, (%1) " : : "r"(val),"r"(addr) : );
}

static inline void vga_clear_screen(uint16_t val)
{
  nk_low_level_memset_word((void*)VGA_BASE_ADDR,val,VGA_WIDTH*VGA_HEIGHT);
}

void vga_scrollup(void);
void vga_putchar(char c);
void vga_print(char *buf);
void vga_puts(char *buf);

static inline void vga_copy_out(void *dest, uint32_t n)
{
  nk_low_level_memcpy((void *)dest,(void*)VGA_BASE_ADDR,n);
}

static inline void vga_copy_in(void *src, uint32_t n)
{
  nk_low_level_memcpy((void*)VGA_BASE_ADDR, src, n);
}



#endif



