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

#include <nautilus/cpu.h>
#include <nautilus/dev.h>
#include <dev/vga.h>

#define VGA_BASE_ADDR 0xb8000

#define CRTC_ADDR 0x3d4
#define CRTC_DATA 0x3d5
#define CURSOR_LOW 0xf
#define CURSOR_HIGH 0xe

#define ATTR_CTRL_FLIP_FLOP 0x3da
#define ATTR_CTRL_ADDR_AND_DATA_WRITE 0x3c0
#define ATTR_CTRL_DATA_READ 0x3c1
#define ATTR_MODE_CTRL 0x10

uint16_t 
vga_make_entry (char c, uint8_t color)
{
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | color16 << 8;
}

 
 
uint8_t 
vga_make_color (enum vga_color fg, enum vga_color bg) 
{
    return fg | bg << 4;
}

static uint8_t vga_x, vga_y;
static uint8_t vga_attr;

inline void vga_set_cursor(uint8_t x, uint8_t y)
{
  uint16_t pos = y*VGA_WIDTH+x;

  vga_x = x;
  vga_y = y;

  outb(CURSOR_HIGH,CRTC_ADDR);
  outb(pos>>8, CRTC_DATA);
  outb(CURSOR_LOW,CRTC_ADDR);
  outb(pos&0xff, CRTC_DATA);

}

inline void vga_get_cursor(uint8_t *x, uint8_t *y)
{
  *x = vga_x;
  *y = vga_y;
}

// force attributes to be interpreted as colors instead
// of blink/bold/italic/underline/etc
static void disable_blink()
{
  uint8_t val;

  // waits are architecturally specified at 250 us

  // Reset interaction mode back to address mode
  inb(ATTR_CTRL_FLIP_FLOP);
  udelay(300); // wait for attribute controller 

  // now do a read - select our address
  // the 0x20 here indicates we want to continue to use
  // the regular VGA pallete which the bios has already setup for
  // us
  outb(ATTR_MODE_CTRL | 0x20,ATTR_CTRL_ADDR_AND_DATA_WRITE);
  // flip flop will now be in write mode (reads will not affect it)

  udelay(300); // wait for attribute controller 

  // will not affect flip flop
  val = inb(ATTR_CTRL_DATA_READ);

  val &= ~0x08; // turn off bit 3, which is blinking enable

  udelay(300); // wait for attribute controller

  // write it back
  outb(val,ATTR_CTRL_ADDR_AND_DATA_WRITE);
  // FF should now be back to address mode

  udelay(300); // wait for attribute controller

}

void vga_init_screen()
{
  disable_blink();
  //enable cursor as a block
  outb(0x0a,CRTC_ADDR); outb((inb(CRTC_DATA) & 0xc0) | 0, CRTC_DATA);
  outb(0x0b,CRTC_ADDR); outb((inb(CRTC_DATA) & 0xe0) | 15, CRTC_DATA);
}


void vga_early_init()
{
  vga_x=vga_y=0;
  vga_attr = 0x08;
  vga_clear_screen(vga_make_entry(' ', vga_attr));
  vga_set_cursor(vga_x,vga_y);
}

static struct nk_dev_int ops = {
    .open=0,
    .close=0,
};

void vga_init()
{
    nk_dev_register("vga",NK_DEV_GENERIC,0,&ops,0);
}

void vga_scrollup (void) 
{
  uint16_t *buf = (uint16_t*) VGA_BASE_ADDR;

  nk_low_level_memcpy_word(buf,buf+VGA_WIDTH,VGA_WIDTH*(VGA_HEIGHT-1));

  nk_low_level_memset_word(buf+VGA_WIDTH*(VGA_HEIGHT-1),vga_make_entry(' ', vga_attr),VGA_WIDTH);
 
}

void vga_putchar(char c)
{
  if (c == '\n') {
    vga_x = 0;
    
    if (++vga_y == VGA_HEIGHT) {
      vga_scrollup();
      vga_y--;
    }
  } else {

    vga_write_screen(vga_x, vga_y, vga_make_entry(c,vga_attr));

    if (++vga_x == VGA_WIDTH) {
      vga_x = 0;
      if (++vga_y == VGA_HEIGHT) {
	vga_scrollup();
	vga_y--;
      }
    }
  }
  vga_set_cursor(vga_x,vga_y);
}
  
void vga_print(char *buf)
{
  while (*buf) { 
    vga_putchar(*buf);
    buf++;
  }
}

void vga_puts(char *buf)
{
  vga_print(buf);
  vga_putchar('\n');
}

