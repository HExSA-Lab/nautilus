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
 * Copyright (c) 2020, Drew Kersnar <drewkersnar2021@u.northwestern.edu>
 * Copyright (c) 2020, The Interweaving Project <http://interweaving.org>
 *                     The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Drew Kersnar <drewkersnar2021@u.northwestern.edu>
 *          
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */


#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/monitor.h>
#include <nautilus/dr.h>
#include <nautilus/smp.h>
#ifdef NAUT_CONFIG_PROVENANCE
#include <nautilus/provenance.h>
#endif
#include <dev/apic.h>


/*
  Self-contained built-in multicore monitor

  Note that this can be used with the watchdog timer to catch
  long durations of time with interrupts off

  Limits:
    - currently only the debug registers are used
      so at most 4 breakpoints can be set
    - I/O is VGA+PS2 only.  Serial should be added at some point
*/

#define COLOR_FOREGROUND        COLOR_LIGHT_CYAN
#define COLOR_BACKGROUND        COLOR_BLACK
#define COLOR_PROMPT_FOREGROUND COLOR_LIGHT_BROWN
#define COLOR_PROMPT_BACKGROUND COLOR_BLACK


// VGA code repeats here to be self-contained
// future version will also support serial port

#define VGA_BASE_ADDR 0xb8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define CRTC_ADDR 0x3d4
#define CRTC_DATA 0x3d5
#define CURSOR_LOW 0xf
#define CURSOR_HIGH 0xe

static uint8_t vga_x, vga_y;
static uint8_t vga_attr;

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

// Encodes the text and color into a short
static uint16_t
vga_make_entry(char c, uint8_t color)
{
  uint16_t c16 = c;
  uint16_t color16 = color;
  return c16 | color16 << 8;
}

// Encodes the color in a char
static uint8_t
vga_make_color(enum vga_color fg, enum vga_color bg)
{
  static int i = 0;
  return fg | bg << 4;
}

// Writes the encoded text and color to position x, y on the screen
static inline void vga_write_screen(uint8_t x, uint8_t y, uint16_t val)
{
  uint16_t *addr = ((uint16_t *)VGA_BASE_ADDR) + y * VGA_WIDTH + x;
  __asm__ __volatile__(" movw %0, (%1) "
                       :
                       : "r"(val), "r"(addr)
                       :);
}

// clears the screen and fills it with val
static inline void vga_clear_screen(uint16_t val)
{
  nk_low_level_memset_word((void *)VGA_BASE_ADDR, val, VGA_WIDTH * VGA_HEIGHT);
}

static inline void vga_set_cursor(uint8_t x, uint8_t y)
{
  uint16_t pos = y * VGA_WIDTH + x;

  vga_x = x;
  vga_y = y;

  outb(CURSOR_HIGH, CRTC_ADDR);
  outb(pos >> 8, CRTC_DATA);
  outb(CURSOR_LOW, CRTC_ADDR);
  outb(pos & 0xff, CRTC_DATA);
}

static inline void vga_get_cursor(uint8_t *x, uint8_t *y)
{
  *x = vga_x;
  *y = vga_y;
}

static void vga_scrollup(void)
{
  uint16_t *buf = (uint16_t *)VGA_BASE_ADDR;

  nk_low_level_memcpy_word(buf, buf + VGA_WIDTH, VGA_WIDTH * (VGA_HEIGHT - 1));

  nk_low_level_memset_word(buf + VGA_WIDTH * (VGA_HEIGHT - 1), vga_make_entry(' ', vga_attr), VGA_WIDTH);
}

static void vga_putchar(char c)
{
  if (c == '\n')
  {
    vga_x = 0;

    if (++vga_y == VGA_HEIGHT)
    {
      vga_scrollup();
      vga_y--;
    }
  }
  else
  {

    vga_write_screen(vga_x, vga_y, vga_make_entry(c, vga_attr));

    if (++vga_x == VGA_WIDTH)
    {
      vga_x = 0;
      if (++vga_y == VGA_HEIGHT)
      {
        vga_scrollup();
        vga_y--;
      }
    }
  }
  vga_set_cursor(vga_x, vga_y);
}

static void vga_print(char *buf)
{
  while (*buf)
  {
    vga_putchar(*buf);
    buf++;
  }
}

static void vga_puts(char *buf)
{
  vga_print(buf);
  vga_putchar('\n');
}

static inline void vga_copy_out(void *dest, uint32_t n)
{
  nk_low_level_memcpy((void *)dest,(void*)VGA_BASE_ADDR,n);
}

static inline void vga_copy_in(void *src, uint32_t n)
{
  nk_low_level_memcpy((void*)VGA_BASE_ADDR, src, n);
}


// Private output formatting routines since we
// do not want to reply on printf being functional
#define DB(x) vga_putchar(x)
#define DHN(x) vga_putchar(((x & 0xF) >= 10) ? (((x & 0xF) - 10) + 'a') : ((x & 0xF) + '0'))
#define DHB(x) DHN(x >> 4) ; DHN(x);
#define DHW(x) DHB(x >> 8) ; DHB(x);
#define DHL(x) DHW(x >> 16) ; DHW(x);
#define DHQ(x) DHL(x >> 32) ; DHL(x);
#define DS(x) { char *__curr = x; while(*__curr) { DB(*__curr); __curr++; } }

static char screen_saved[VGA_HEIGHT*VGA_WIDTH*2];
static uint8_t cursor_saved_x;
static uint8_t cursor_saved_y;

// Keyboard stuff repeats here to be self-contained

static nk_keycode_t kbd_flags = 0;

// we pull much of the PS2 setup from ps2.h indirectly
// We need to replicate the translation tables

static const nk_keycode_t NoShiftNoCaps[] = {
    KEY_UNKNOWN, ASCII_ESC, '1', '2',                  /* 0x00 - 0x03 */
    '3', '4', '5', '6',                                /* 0x04 - 0x07 */
    '7', '8', '9', '0',                                /* 0x08 - 0x0B */
    '-', '=', ASCII_BS, '\t',                          /* 0x0C - 0x0F */
    'q', 'w', 'e', 'r',                                /* 0x10 - 0x13 */
    't', 'y', 'u', 'i',                                /* 0x14 - 0x17 */
    'o', 'p', '[', ']',                                /* 0x18 - 0x1B */
    '\r', KEY_LCTRL, 'a', 's',                         /* 0x1C - 0x1F */
    'd', 'f', 'g', 'h',                                /* 0x20 - 0x23 */
    'j', 'k', 'l', ';',                                /* 0x24 - 0x27 */
    '\'', '`', KEY_LSHIFT, '\\',                       /* 0x28 - 0x2B */
    'z', 'x', 'c', 'v',                                /* 0x2C - 0x2F */
    'b', 'n', 'm', ',',                                /* 0x30 - 0x33 */
    '.', '/', KEY_RSHIFT, KEY_PRINTSCRN,               /* 0x34 - 0x37 */
    KEY_LALT, ' ', KEY_CAPSLOCK, KEY_F1,               /* 0x38 - 0x3B */
    KEY_F2, KEY_F3, KEY_F4, KEY_F5,                    /* 0x3C - 0x3F */
    KEY_F6, KEY_F7, KEY_F8, KEY_F9,                    /* 0x40 - 0x43 */
    KEY_F10, KEY_NUMLOCK, KEY_SCRLOCK, KEY_KPHOME,     /* 0x44 - 0x47 */
    KEY_KPUP, KEY_KPPGUP, KEY_KPMINUS, KEY_KPLEFT,     /* 0x48 - 0x4B */
    KEY_KPCENTER, KEY_KPRIGHT, KEY_KPPLUS, KEY_KPEND,  /* 0x4C - 0x4F */
    KEY_KPDOWN, KEY_KPPGDN, KEY_KPINSERT, KEY_KPDEL,   /* 0x50 - 0x53 */
    KEY_SYSREQ, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, /* 0x54 - 0x57 */
};

static const nk_keycode_t ShiftNoCaps[] = {
    KEY_UNKNOWN, ASCII_ESC, '!', '@',                  /* 0x00 - 0x03 */
    '#', '$', '%', '^',                                /* 0x04 - 0x07 */
    '&', '*', '(', ')',                                /* 0x08 - 0x0B */
    '_', '+', ASCII_BS, '\t',                          /* 0x0C - 0x0F */
    'Q', 'W', 'E', 'R',                                /* 0x10 - 0x13 */
    'T', 'Y', 'U', 'I',                                /* 0x14 - 0x17 */
    'O', 'P', '{', '}',                                /* 0x18 - 0x1B */
    '\r', KEY_LCTRL, 'A', 'S',                         /* 0x1C - 0x1F */
    'D', 'F', 'G', 'H',                                /* 0x20 - 0x23 */
    'J', 'K', 'L', ':',                                /* 0x24 - 0x27 */
    '"', '~', KEY_LSHIFT, '|',                         /* 0x28 - 0x2B */
    'Z', 'X', 'C', 'V',                                /* 0x2C - 0x2F */
    'B', 'N', 'M', '<',                                /* 0x30 - 0x33 */
    '>', '?', KEY_RSHIFT, KEY_PRINTSCRN,               /* 0x34 - 0x37 */
    KEY_LALT, ' ', KEY_CAPSLOCK, KEY_F1,               /* 0x38 - 0x3B */
    KEY_F2, KEY_F3, KEY_F4, KEY_F5,                    /* 0x3C - 0x3F */
    KEY_F6, KEY_F7, KEY_F8, KEY_F9,                    /* 0x40 - 0x43 */
    KEY_F10, KEY_NUMLOCK, KEY_SCRLOCK, KEY_KPHOME,     /* 0x44 - 0x47 */
    KEY_KPUP, KEY_KPPGUP, KEY_KPMINUS, KEY_KPLEFT,     /* 0x48 - 0x4B */
    KEY_KPCENTER, KEY_KPRIGHT, KEY_KPPLUS, KEY_KPEND,  /* 0x4C - 0x4F */
    KEY_KPDOWN, KEY_KPPGDN, KEY_KPINSERT, KEY_KPDEL,   /* 0x50 - 0x53 */
    KEY_SYSREQ, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, /* 0x54 - 0x57 */
};

static const nk_keycode_t NoShiftCaps[] = {
    KEY_UNKNOWN, ASCII_ESC, '1', '2',                  /* 0x00 - 0x03 */
    '3', '4', '5', '6',                                /* 0x04 - 0x07 */
    '7', '8', '9', '0',                                /* 0x08 - 0x0B */
    '-', '=', ASCII_BS, '\t',                          /* 0x0C - 0x0F */
    'Q', 'W', 'E', 'R',                                /* 0x10 - 0x13 */
    'T', 'Y', 'U', 'I',                                /* 0x14 - 0x17 */
    'O', 'P', '[', ']',                                /* 0x18 - 0x1B */
    '\r', KEY_LCTRL, 'A', 'S',                         /* 0x1C - 0x1F */
    'D', 'F', 'G', 'H',                                /* 0x20 - 0x23 */
    'J', 'K', 'L', ';',                                /* 0x24 - 0x27 */
    '\'', '`', KEY_LSHIFT, '\\',                       /* 0x28 - 0x2B */
    'Z', 'X', 'C', 'V',                                /* 0x2C - 0x2F */
    'B', 'N', 'M', ',',                                /* 0x30 - 0x33 */
    '.', '/', KEY_RSHIFT, KEY_PRINTSCRN,               /* 0x34 - 0x37 */
    KEY_LALT, ' ', KEY_CAPSLOCK, KEY_F1,               /* 0x38 - 0x3B */
    KEY_F2, KEY_F3, KEY_F4, KEY_F5,                    /* 0x3C - 0x3F */
    KEY_F6, KEY_F7, KEY_F8, KEY_F9,                    /* 0x40 - 0x43 */
    KEY_F10, KEY_NUMLOCK, KEY_SCRLOCK, KEY_KPHOME,     /* 0x44 - 0x47 */
    KEY_KPUP, KEY_KPPGUP, KEY_KPMINUS, KEY_KPLEFT,     /* 0x48 - 0x4B */
    KEY_KPCENTER, KEY_KPRIGHT, KEY_KPPLUS, KEY_KPEND,  /* 0x4C - 0x4F */
    KEY_KPDOWN, KEY_KPPGDN, KEY_KPINSERT, KEY_KPDEL,   /* 0x50 - 0x53 */
    KEY_SYSREQ, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, /* 0x54 - 0x57 */
};

static const nk_keycode_t ShiftCaps[] = {
    KEY_UNKNOWN, ASCII_ESC, '!', '@',                  /* 0x00 - 0x03 */
    '#', '$', '%', '^',                                /* 0x04 - 0x07 */
    '&', '*', '(', ')',                                /* 0x08 - 0x0B */
    '_', '+', ASCII_BS, '\t',                          /* 0x0C - 0x0F */
    'q', 'w', 'e', 'r',                                /* 0x10 - 0x13 */
    't', 'y', 'u', 'i',                                /* 0x14 - 0x17 */
    'o', 'p', '{', '}',                                /* 0x18 - 0x1B */
    '\r', KEY_LCTRL, 'a', 's',                         /* 0x1C - 0x1F */
    'd', 'f', 'g', 'h',                                /* 0x20 - 0x23 */
    'j', 'k', 'l', ':',                                /* 0x24 - 0x27 */
    '"', '~', KEY_LSHIFT, '|',                         /* 0x28 - 0x2B */
    'z', 'x', 'c', 'v',                                /* 0x2C - 0x2F */
    'b', 'n', 'm', '<',                                /* 0x30 - 0x33 */
    '>', '?', KEY_RSHIFT, KEY_PRINTSCRN,               /* 0x34 - 0x37 */
    KEY_LALT, ' ', KEY_CAPSLOCK, KEY_F1,               /* 0x38 - 0x3B */
    KEY_F2, KEY_F3, KEY_F4, KEY_F5,                    /* 0x3C - 0x3F */
    KEY_F6, KEY_F7, KEY_F8, KEY_F9,                    /* 0x40 - 0x43 */
    KEY_F10, KEY_NUMLOCK, KEY_SCRLOCK, KEY_KPHOME,     /* 0x44 - 0x47 */
    KEY_KPUP, KEY_KPPGUP, KEY_KPMINUS, KEY_KPLEFT,     /* 0x48 - 0x4B */
    KEY_KPCENTER, KEY_KPRIGHT, KEY_KPPLUS, KEY_KPEND,  /* 0x4C - 0x4F */
    KEY_KPDOWN, KEY_KPPGDN, KEY_KPINSERT, KEY_KPDEL,   /* 0x50 - 0x53 */
    KEY_SYSREQ, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, /* 0x54 - 0x57 */
};

#define KB_KEY_RELEASE 0x80

#define KBD_DATA_REG 0x60
#define KBD_CMD_REG 0x64
#define KBD_STATUS_REG 0x64

static nk_keycode_t kbd_translate_monitor(nk_scancode_t scan)
{
  int release;
  const nk_keycode_t *table = 0;
  nk_keycode_t cur;
  nk_keycode_t flag;

  // update the flags

  release = scan & KB_KEY_RELEASE;
  scan &= ~KB_KEY_RELEASE;

  if (kbd_flags & KEY_CAPS_FLAG)
  {
    if (kbd_flags & KEY_SHIFT_FLAG)
    {
      table = ShiftCaps;
    }
    else
    {
      table = NoShiftCaps;
    }
  }
  else
  {
    if (kbd_flags & KEY_SHIFT_FLAG)
    {
      table = ShiftNoCaps;
    }
    else
    {
      table = NoShiftNoCaps;
    }
  }

  cur = table[scan];

  flag = 0;
  switch (cur)
  {
  case KEY_LSHIFT:
  case KEY_RSHIFT:
    flag = KEY_SHIFT_FLAG;
    break;
  case KEY_CAPSLOCK:
    flag = KEY_CAPS_FLAG;
    break;
  default:
    goto do_noflags;
    break;
  }

  // do_flags:
  if (flag == KEY_CAPS_FLAG)
  {
    if (!release)
    {
      if ((kbd_flags & KEY_CAPS_FLAG))
      {
        // turn off caps lock on second press
        kbd_flags &= ~KEY_CAPS_FLAG;
        flag = 0;
      }
      else
      {
        kbd_flags |= flag;
      }
    }
    return NO_KEY;
  }

  if (release)
  {
    kbd_flags &= ~(flag);
  }
  else
  {
    kbd_flags |= flag;
  }

  return NO_KEY;

do_noflags:
  if (release)
  { // Chose to display on press, if that is wrong it's an easy change
    return kbd_flags | cur;
  }
  else
  {
    return NO_KEY;
  }
}

typedef union ps2_status {
  uint8_t val;
  struct
  {
    uint8_t obf : 1;  // output buffer full (1=> can read from 0x60)
    uint8_t ibf : 1;  // input buffer full (0=> can write to either port)
    uint8_t sys : 1;  // set on successful reset
    uint8_t a2 : 1;   // last address (0=>0x60, 1=>0x64)
    uint8_t inh : 1;  // keyboard inhibit, active low
    uint8_t mobf : 1; // mouse buffer full
    uint8_t to : 1;   // timeout
    uint8_t perr : 1; // parity error
  };
} __packed ps2_status_t;

typedef union ps2_cmd {
  uint8_t val;
  struct
  {
    uint8_t kint : 1; // enable keyboard interrupts
    uint8_t mint : 1; // enable mouse interrupts
    uint8_t sys : 1;  // set/clear sysflag / do BAT
    uint8_t rsv1 : 1;
    uint8_t ken : 1;  // keyboard enable, active low
    uint8_t men : 1;  // mouse enable, active low
    uint8_t xlat : 1; // do scancode translation
    uint8_t rsv2 : 1;
  };
} __packed ps2_cmd_t;

static int ps2_wait_for_key()
{
  volatile ps2_status_t status;
  nk_scancode_t scan;

  do
  {
    status.val = inb(KBD_STATUS_REG);
    // INFO("obf=%d ibf=%d req=%s\n",status.obf, status.ibf, w==INPUT ? "input" : "output");
  } while (!status.obf); // wait for something to be in the output buffer for us to read

  //io_delay();

  scan = inb(KBD_DATA_REG);
  //io_delay();

  return scan;
}


// Private input routines since we cannot rely on
// vc or lower level functionality in the monitor

// writes a line of text into a buffer
static void wait_for_command(char *buf, int buffer_size)
{

  int key;
  int curr = 0;
  while (1)
  {
    key = ps2_wait_for_key();
    uint16_t key_encoded = kbd_translate_monitor(key);
    //vga_clear_screen(vga_make_entry(key, vga_make_color(COLOR_FOREGROUND, COLOR_BACKGROUND)));

    if (key_encoded == '\r')
    {
      // return completed command
      buf[curr] = 0;
      return;
    }

    if (key_encoded != NO_KEY)
    {
      char key_char = (char)key_encoded;

      if (curr < buffer_size - 1)
      {
        vga_putchar(key_char);
        buf[curr] = key_char;
        curr++;
      }
      else
      {
        // they can't type any more
        buf[curr] = 0;
        return;
      }

      //strncat(*buf, &key_char, 1);
      //printk(buf);
    }
  };
}

static int is_hex_addr(char *addr_as_str)
{
  int str_len = strlen(addr_as_str);

  // 64 bit addresses can't be larger than 16 hex digits, 18 including 0x
  if(str_len > 18) {
    return 0;
  }
  
  if((addr_as_str[0] != '0') || (addr_as_str[1] != 'x')) {
    return 0;
  }

  for(int i = 2; i < str_len; i++) {
    char curr = addr_as_str[i];
    if((curr >= '0' && curr <= '9')
      || (curr >= 'A' && curr <= 'F')
      || (curr >= 'a' && curr <= 'f'))  {
      continue;
    } else {
      return 0;
    }
  }
  return 1;
}

static uint64_t get_hex_addr(char *addr_as_str)
{
  
  uint64_t addr = 0;
  int power_of_16 = 1;
  // iterate backwards from the end of the address to the beginning, stopping before 0x
  for(int i = (strlen(addr_as_str) - 1); i > 1; i--) {
    char curr = addr_as_str[i];
    if(curr >= '0' && curr <= '9')  {
      addr += (curr - '0')*power_of_16;
    } 
    else if(curr >= 'A' && curr <= 'F') {
      addr += (curr - 'A' + 10)*power_of_16;
    }
    else if(curr >= 'a' && curr <= 'f') {
      addr += (curr - 'a' + 10)*power_of_16;
    } else {
      // something broken
      ASSERT(false);
    }
    
    power_of_16*=16;
  }
  return addr;

}


static int is_dec_addr(char *addr_as_str)
{
  int str_len = strlen(addr_as_str);

  // // 64 bit addresses can't be larger than 16 hex digits, 18 including 0x
  // if(str_len > 18) {
  //   return 0;
  // } 
  // TODO: check if dec_addr too big

  for(int i = 0; i < str_len; i++) {
    char curr = addr_as_str[i];
    if((curr >= '0' && curr <= '9'))  {
      continue;
    } else {
      return 0;
    }
  }
  return 1;
}

static uint64_t get_dec_addr(char *addr_as_str)
{
  uint64_t addr = 0;
  int power_of_10 = 1;
  // iterate backwards from the end of the address to the beginning, stopping before 0x
  for(int i = (strlen(addr_as_str) - 1); i >= 0; i--) {
    char curr = addr_as_str[i];
    if(curr >= '0' && curr <= '9')  {
      addr += (curr - '0')*power_of_10;
    } else {
      // something broken
      ASSERT(false);
    }
    
    power_of_10*=10;
  }
  return addr;

}

static int is_dr_num(char *addr_as_str)
{
  int str_len = strlen(addr_as_str);

  // TODO: check if dec_addr too big

  for(int i = 0; i < str_len; i++) {
    char curr = addr_as_str[i];
    if((curr >= '0' && curr <= '3'))  {
      continue;
    } else {
      return 0;
    }
  }
  return 1;
}

static uint64_t get_dr_num(char *num_as_str)
{
  
  uint64_t num = 0;
  int power_of_10 = 1;
  // iterate backwards from the end of the address to the beginning, stopping before 0x
  for(int i = (strlen(num_as_str) - 1); i >= 0; i--) {
    char curr = num_as_str[i];
    if(curr >= '0' && curr <= '9')  {
      num += (curr - '0')*power_of_10;
    } else {
      // something broken
      ASSERT(false);
    }
    
    power_of_10*=10;
  }
  return num;

}

// Private tokenizer since we may want to use a non-reentrant version at
// some point.., and also because we don't want to rely on the other
// copy in naut_string, which looks lua-specific
//

static char *
__strtok_r_monitor(char *s, const char *delim, char **last)
{
	char *spanp, *tok;
	int c, sc;

	if (s == NULL && (s = *last) == NULL)
		return (NULL);

	/*
	 * 	 * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
	 * 	 	 */
cont:
	c = *s++;
	for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}

	if (c == 0) {		/* no non-delimiter characters */
		*last = NULL;
		return (NULL);
	}
	tok = s - 1;

	/*
	 * 	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * 	 	 * Note that delim must have one NUL; we stop if we see that, too.
	 * 	 	 	 */
	for (;;) {
		c = *s++;
		spanp = (char *)delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = '\0';
				*last = s;
				return (tok);
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
}

static char *
strtok_monitor(char *s, const char *delim)
{
	static char *last;

	return (__strtok_r_monitor(s, delim, &last));
}

static char *
get_next_word(char *s)
{
  const char delim[3] = " \t";
  return strtok_monitor(s, delim);
}

// Executing user commands

static int execute_quit(char command[])
{
  vga_puts("quit executed");
  return 1;
}

static int execute_help(char command[])
{
  vga_puts("commands:");
  vga_puts("  quit");
  vga_puts("  help");
  vga_puts("  watch [number] [addr]");
  vga_puts("  break [number] [addr]");
  vga_puts("  disable [number]");
  vga_puts("  drinfo");
  return 0;
}

static int execute_watch(char command[])
{
  char* potential_num = get_next_word(NULL);
  char* potential_addr = get_next_word(NULL);
  char* next_word = get_next_word(NULL);

  if (!potential_num || !potential_addr) {
    vga_puts("too few arguments for watch, expected number and address");
    return 0;
  }

  if (next_word) {
    vga_puts("too many arguments for watch, expected number and address");
    return 0;
  }

  if (!is_dr_num(potential_num)) {
    vga_puts("invalid argument for watch, expected number between 0 and 3");
    return 0;
  }

  int dreg_num = get_dr_num(potential_num);


  if (is_hex_addr(potential_addr)) {
    vga_puts("watchpoint set at address:");
    vga_puts(potential_addr);
    uint64_t addr = get_hex_addr(potential_addr);
    set_watchpoint(addr, dreg_num);
  } else if (is_dec_addr(potential_addr)) {
    vga_puts("watchpoint set at address:");
    vga_puts(potential_addr);
    uint64_t addr = get_dec_addr(potential_addr);
    set_watchpoint(addr, dreg_num);
  } else {
    vga_puts("invalid argument for watch, expected address in hexidecimal format");
  }

  return 0;
}

static int execute_break(char command[])
{
  char* potential_num = get_next_word(NULL);
  char* potential_addr = get_next_word(NULL);
  char* next_word = get_next_word(NULL);

  if (!potential_num || !potential_addr) {
    vga_puts("too few arguments for break, expected number and address");
    return 0;
  }

  if (next_word) {
    vga_puts("too many arguments for break, expected number and address");
    return 0;
  }

  if (!is_dr_num(potential_num)) {
    vga_puts("invalid argument for break, expected number between 0 and 3");
    return 0;
  }

  int dreg_num = get_dr_num(potential_num);
  

  if (is_hex_addr(potential_addr)) {
    vga_puts("breakpoint set at address:");
    vga_puts(potential_addr);
    uint64_t addr = get_hex_addr(potential_addr);
    set_breakpoint(addr, dreg_num);
  } else if (is_dec_addr(potential_addr)) {
    vga_puts("breakpoint set at address:");
    vga_puts(potential_addr);
    uint64_t addr = get_dec_addr(potential_addr);
    set_breakpoint(addr, dreg_num);
  } else {
    vga_puts("invalid arguments for break, expected address in hexidecimal format");
  }

  return 0;

}

static int execute_disable(char command[])
{
  char* potential_num = get_next_word(NULL);
  char* next_word = get_next_word(NULL);

  if (!potential_num) {
    vga_puts("too few arguments for disable, expected number");
    return 0;
  }

  if (next_word) {
    vga_puts("too many arguments for disable, expected number");
    return 0;
  }

  if (!is_dr_num(potential_num)) {
    vga_puts("invalid argument for disable, expected number between 0 and 3");
    return 0;
  }

  int dreg_num = get_dr_num(potential_num);

  disable(dreg_num);

  vga_puts("register 0 disabled");

  return 0;
}

static void print_drinfo()
{

  DS("[--------------- Debug Registers ---------------]\n");
  dr6_t dr6;
	dr6.val = read_dr6();

  if (dr6.dr0_detect) {
    DS("dr0 hit\n");
  }
  if (dr6.dr1_detect) {
    DS("dr1 hit\n");
  }
  if (dr6.dr2_detect) {
    DS("dr2 hit\n");
  }
  if (dr6.dr3_detect) {
    DS("dr3 hit\n");
  }

  DS("current cpu: ");
  uint32_t id = my_cpu_id();
  DHL(id);
  DS("\n");


  dr7_t dr7;
	dr7.val = read_dr7();

  #define PRINTDR(type, x) DS(type ": "); DS(#x " = "); DHQ(x); DS("\n");

  uint64_t dr0 = read_dr0();
  uint64_t dr1 = read_dr1();
  uint64_t dr2 = read_dr2();
  uint64_t dr3 = read_dr3();

  // check if registers are enabled
  if (dr7.local0 && dr7.global0) {
    if (dr7.type0 == 0) {
      PRINTDR("breakpoint", dr0);
    } else {
      PRINTDR("watchpoint", dr0);
    }
  } else {
    PRINTDR("unset     ", dr0);  
  }

  if (dr7.local1 && dr7.global1) {
    if(dr7.type1 == 0) {
      PRINTDR("breakpoint", dr1);
    } else {
      PRINTDR("watchpoint", dr1);
    }
  } else {
    PRINTDR("unset     ", dr1);
  }

  if (dr7.local2 && dr7.global2) {
    if (dr7.type2 == 0) {
      PRINTDR("breakpoint", dr2);
    } else {
      PRINTDR("watchpoint", dr2);
    }
  } else {
    PRINTDR("unset     ", dr2);  
  }


  if (dr7.local3 && dr7.global3) {
    if (dr7.type3 == 0) {
      PRINTDR("breakpoint", dr3);
    } else {
      PRINTDR("watchpoint", dr3);
    }
  } else {
    PRINTDR("unset     ", dr3);  
  }

}

static int execute_drinfo(char command[])
{
  print_drinfo();
  return 0;
}


static int execute_potential_command(char command[])
{
  vga_attr = vga_make_color(COLOR_FOREGROUND, COLOR_BACKGROUND);

  int quit = 0;
  char* print_string = "";

  char* word = get_next_word(command);

  if (strcmp(word, "quit") == 0)
  {
    quit = execute_quit(command);
  }
  if (strcmp(word, "help") == 0)
  {
    quit = execute_help(command);
  }
  else if (strcmp(word, "watch") == 0)
  {
    quit = execute_watch(command);
  }
  else if (strcmp(word, "break") == 0)
  {
    quit = execute_break(command);
  }
  else if (strcmp(word, "disable") == 0)
  {
    quit = execute_disable(command);
  }
  else if (strcmp(word, "drinfo") == 0)
  {
    quit = execute_drinfo(command);
  }
  // else if (strcmp(word, "memprint") == 0)
  // {
  //   // do something else
  //   vga_puts("memprint executed");
  // }
  /* more else if clauses for more commands */
  else /* default: */
  {
    vga_puts("command not recognized");
  }
  vga_attr = vga_make_color(COLOR_PROMPT_FOREGROUND, COLOR_PROMPT_BACKGROUND);
  return quit;

}


static int nk_monitor_loop()
{
  int buffer_size = VGA_WIDTH * 2; 
  char buffer[buffer_size];

  // Inner loop is indvidual keystrokes, outer loop handles commands
  int done = 0;
  while (!done) {
    DS("monitor> ");
    wait_for_command(buffer, buffer_size);
    vga_putchar('\n');
    done = execute_potential_command(buffer);
  };
  return 0;
}


static void __attribute__((noinline))
do_backtrace (void ** fp, unsigned depth)
{
  if (!fp || (unsigned long) fp > (unsigned long) per_cpu_get(system)->mem.phys_mem_avail) {
    return;
  }
  
  DHQ((uint64_t) *(fp+1));
  DS(" ");
  DHQ((uint64_t) *fp);
  DS("\n");

  do_backtrace(*fp, depth+1);
}

static void 
monitor_print_regs (struct nk_regs * r)
{
  int i = 0;
  ulong_t cr0 = 0ul;
  ulong_t cr2 = 0ul;
  ulong_t cr3 = 0ul;
  ulong_t cr4 = 0ul;
  ulong_t cr8 = 0ul;
  ulong_t fs  = 0ul;
  ulong_t gs  = 0ul;
  ulong_t sgs = 0ul;
  uint_t  fsi;
  uint_t  gsi;
  uint_t  cs;
  uint_t  ds;
  uint_t  es;
  ulong_t efer;

  #define PRINT3(x, x_extra_space, y, y_extra_space, z, z_extra_space) DS(#x x_extra_space" = " ); DHQ(r->x); DS(", " #y y_extra_space" = " ); DHQ(r->y); DS(", " #z z_extra_space" = " ); DHQ(r->z); DS("\n");
  #define PRINT3CR(x, x_extra_space, y, y_extra_space, z, z_extra_space) DS(#x x_extra_space " = "); DHQ(x); DS(", " #y y_extra_space" = "); DHQ(y); DS(", " #z z_extra_space" = "); DHQ(z); DS("\n");
  #define PRINT2CR(x, x_extra_space, y, y_extra_space) DS(#x x_extra_space " = "); DHQ(x); DS(", " #y y_extra_space " = "); DHQ(y) DS("\n");
  
  DS("[-------------- Register Contents --------------]\n");
  
  PRINT3(rip, "   ",  rflags, "", rbp, "   ");
  PRINT3(rsp, "   ",  rax, "   ", rbx, "   ");
  PRINT3(rsi, "   ",  r8, "    ", r9, "    ");
  PRINT3(r10, "   ",  r11, "   ", r12, "   ");
  PRINT3(r13, "   ",  r14, "   ", r15, "   ");

  asm volatile("movl %%cs, %0": "=r" (cs));
  asm volatile("movl %%ds, %0": "=r" (ds));
  asm volatile("movl %%es, %0": "=r" (es));
  asm volatile("movl %%fs, %0": "=r" (fsi));
  asm volatile("movl %%gs, %0": "=r" (gsi));

  gs  = msr_read(MSR_GS_BASE);
  fs  = msr_read(MSR_FS_BASE);
  gsi = msr_read(MSR_KERNEL_GS_BASE);
  efer = msr_read(IA32_MSR_EFER);

  asm volatile("movq %%cr0, %0": "=r" (cr0));
  asm volatile("movq %%cr2, %0": "=r" (cr2));
  asm volatile("movq %%cr3, %0": "=r" (cr3));
  asm volatile("movq %%cr4, %0": "=r" (cr4));
  asm volatile("movq %%cr8, %0": "=r" (cr8));

  PRINT3CR(cr0, "   ", cr2, "   ", cr3, "   ");
  PRINT2CR(cr4, "   ", cr8, "   ");
}

#ifdef NAUT_CONFIG_PROVENANCE
static void print_prov_info(uint64_t addr) {
	provenance_info* prov_info = nk_prov_get_info(addr);
	if (prov_info != NULL) {
		DS((prov_info->symbol != NULL) ? (char*) prov_info->symbol : "???");
		DS(" ");
		DS((prov_info->section != NULL) ? (char*) prov_info->section : "???");
		if (prov_info->line_info != NULL) {
			// TODO: print line info
		}
		if (prov_info->file_info != NULL) {
			// TODO: print file info
		}
		free(prov_info);
	}
	DS("\n");
}
#endif

static void dump_call(void)
{
  print_drinfo();
  DS("[------------------ Backtrace ------------------]\n");
#ifdef NAUT_CONFIG_PROVENANCE
#define PROV(addr) print_prov_info(addr)
#else
#define PROV(addr) 
#endif

// avoid reliance on backtrace.h
#define BT(k) \
  if (!__builtin_return_address(k)) { \
      goto done; \
  } \
  DHQ(((uint64_t)__builtin_return_address(k))); DS(": ");	\
  PROV((uint64_t)__builtin_return_address(k));	\

  BT(0);
  BT(1);
  BT(2);
  BT(3);
  BT(4);
  BT(6);
  BT(7);
  BT(8);

 done:
  return;
}

static void dump_entry(excp_vec_t vector, excp_entry_t * excp)
{
  struct nk_regs * r = (struct nk_regs*)((char*)excp - 128);
  print_drinfo();
  if (excp) { 
      DS("[---------- Exception/Interrupt State ----------]\n");
      DS("vector: "); DHL(vector);
      DS("      error: "); DHL(excp->error_code);
      DS("\n");
  }
  monitor_print_regs(r);
  DS("[------------------ Backtrace ------------------]\n");
  do_backtrace((void **) r->rbp, 0); 
}


// Global variables to sync dr registers across cores
static int sync_done = 0;
static uint64_t sync_dr0;
static uint64_t sync_dr1;
static uint64_t sync_dr2;
static uint64_t sync_dr3;
static uint64_t sync_dr7;

// some CPU is entering the monitor if this is set
static uint32_t monitor_entry_flag = 0;

// the cpu that caused the entry
static int      monitor_entry_cpu = 0;

// used to handle the stages of synchronizing the cpus on
// entry and exit of monitor
static nk_counting_barrier_t entry, update, exit;

// bring all other CPUs into the monitor by NMIing them
// this will cause an exception entry
static void NMI_other_CPUs()
{
    apic_bcast_nmi(get_cpu()->apic);
}

// update the global state of the debug registers
static void sync_other_CPUs()
{
  sync_dr0 = read_dr0();
  sync_dr1 = read_dr1();
  sync_dr2 = read_dr2();
  sync_dr3 = read_dr3();
  sync_dr7 = read_dr7();
}

// each CPU calls this to sync its debug registers
static void sync_my_DRs()
{
  write_dr0(sync_dr0);
  write_dr1(sync_dr1);
  write_dr2(sync_dr2);
  write_dr3(sync_dr3);
  write_dr7(sync_dr7);
}

// Wait for the main core to be done, then continue
int nk_monitor_sync_entry(void)
{ 
    // let other cpus know I'm here
    nk_counting_barrier(&entry);
    // nothing to do until the winner updates the state
    nk_counting_barrier(&update);
    // update my local state (e.g. debug regs)
    sync_my_DRs();
    // let leader know I'm ready to go
    nk_counting_barrier(&exit);
    
    return 0;
}

int nk_monitor_check(int *cpu)
{
    *cpu = monitor_entry_cpu;
    
    return monitor_entry_flag;
}

// ordinary reques for entry into the monitor
// we just wait our turn and then alert everyone else
static int monitor_init_lock()
{
    while (!__sync_bool_compare_and_swap(&monitor_entry_flag,0,1)) {
	// I lose the entry game to another cpu, so I must process
	// wait until the leader is done before proceeding. After they are done, I can proceed.
	nk_monitor_sync_entry();
	
    }
    
    monitor_entry_cpu = my_cpu_id();
    
    // force other CPUs into the monitor
    NMI_other_CPUs();
    
    nk_counting_barrier(&entry);
    
    return 0; // handle processing...
}


// Main CPU wraps up its use of the monitor and updates everyone
static int monitor_deinit_lock(void)
{
    sync_other_CPUs();
    // let other cpus know the state is now ready
    nk_counting_barrier(&update);
    // reset the entry flag
    __sync_fetch_and_and(&monitor_entry_flag,0);
    // wait on other cpus to use the state
    nk_counting_barrier(&exit);
    return 0;
}

// every entry to the monitor
// returns int flag for later restore
static uint8_t monitor_init(void)
{
    uint8_t intr_flags = irq_disable_save(); // disable interrupts

    monitor_init_lock();
    
    vga_copy_out(screen_saved, VGA_WIDTH*VGA_HEIGHT*2);
    vga_get_cursor(&cursor_saved_x, &cursor_saved_y);
    
    vga_x = vga_y = 0;
    vga_attr = vga_make_color(COLOR_PROMPT_FOREGROUND, COLOR_PROMPT_BACKGROUND);
    vga_clear_screen(vga_make_entry(' ', vga_attr));
    vga_set_cursor(vga_x, vga_y);

    kbd_flags = 0;

    // probably should reset ps2 here...

    return intr_flags;

}

// called right before every exit from the monitor
// takes int flag to restore
static void monitor_deinit(uint8_t intr_flags)
{
    vga_copy_in(screen_saved, VGA_WIDTH*VGA_HEIGHT*2);
    vga_set_cursor(cursor_saved_x, cursor_saved_y);

    monitor_deinit_lock();

    irq_enable_restore(intr_flags);
}

// called once at boot up
void nk_monitor_init()
{
    int num_cpus = nk_get_num_cpus();

    nk_counting_barrier_init(&entry, num_cpus);
    nk_counting_barrier_init(&update, num_cpus);
    nk_counting_barrier_init(&exit, num_cpus);
    
}

// Entrypoints to the monitor

// Entering through the shell command or f9
int nk_monitor_entry()
{
    uint8_t intr_flags = monitor_init();
    
    vga_attr = vga_make_color(COLOR_FOREGROUND, COLOR_BACKGROUND);
    vga_puts("Monitor Entered");
    dump_call();
    vga_attr = vga_make_color(COLOR_PROMPT_FOREGROUND, COLOR_PROMPT_BACKGROUND);
    
    nk_monitor_loop();
    
    monitor_deinit(intr_flags);
    
    return 0;
}

// Entering because we had an unhandled exception
int nk_monitor_excp_entry(excp_entry_t * excp,
			  excp_vec_t vector,
			  void *state)
{
    uint8_t intr_flags = monitor_init();
    
    vga_attr = vga_make_color(COLOR_FOREGROUND, COLOR_BACKGROUND);
    vga_puts("+++ Unhandled Exception Caught by Monitor +++");
    dump_entry(vector,excp);
    vga_attr = vga_make_color(COLOR_PROMPT_FOREGROUND, COLOR_PROMPT_BACKGROUND);

    nk_monitor_loop();
    
    monitor_deinit(intr_flags);
    
    return 0;
}

// Entering because we had an unhandled interrupt
int nk_monitor_irq_entry(excp_entry_t * excp,
			 excp_vec_t vector,
			 void *state)
{
    uint8_t intr_flags = monitor_init();

    vga_attr = vga_make_color(COLOR_FOREGROUND, COLOR_BACKGROUND);
    vga_puts("+++ Unhandled Irq Caught by Monitor +++");
    dump_entry(vector,excp);
    vga_attr = vga_make_color(COLOR_PROMPT_FOREGROUND, COLOR_PROMPT_BACKGROUND);

    nk_monitor_loop();
    
    monitor_deinit(intr_flags);
    
    return 0;
}

// Entering because we panicked
int nk_monitor_panic_entry(char *str)
{
    uint8_t intr_flags = monitor_init();
    
    vga_attr = vga_make_color(COLOR_FOREGROUND, COLOR_BACKGROUND);
    vga_puts("+++ Panic Caught by Monitor +++");
    vga_puts(str);
    dump_call();
    vga_attr = vga_make_color(COLOR_PROMPT_FOREGROUND, COLOR_PROMPT_BACKGROUND);
    
    nk_monitor_loop();
    
    monitor_deinit(intr_flags);
    
    return 0;
}

// Entering because we panicked
int nk_monitor_hang_entry(excp_entry_t * excp,
			  excp_vec_t vector,
			  void *state)
{
    uint8_t intr_flags = monitor_init();
    
    vga_attr = vga_make_color(COLOR_FOREGROUND, COLOR_BACKGROUND);
    vga_puts("+++ Hang Caught by Monitor +++");
    dump_entry(vector,excp);
    vga_attr = vga_make_color(COLOR_PROMPT_FOREGROUND, COLOR_PROMPT_BACKGROUND);
    
    nk_monitor_loop();
    
    monitor_deinit(intr_flags);
    
    return 0;
}

static void* breakpoint_addr[NAUT_CONFIG_MAX_CPUS];
static int breakpoint_drnum[NAUT_CONFIG_MAX_CPUS];

// Entering because we hit a breakpoint or a watchpoint
int nk_monitor_debug_entry(excp_entry_t * excp,
			   excp_vec_t vector,
			   void *state)
{

    uint8_t intr_flags = monitor_init();
    
    dr6_t dr6;
    dr6.val = read_dr6();
    dr7_t dr7;
    dr7.val = read_dr7();

    // if we are in single step mode, we are running one instruction after hitting a breakpoint
    if (dr6.bp_single == 1) {
	// set breakpoint back
	set_breakpoint((uint64_t) breakpoint_addr[my_cpu_id()], breakpoint_drnum[my_cpu_id()]);
	excp->rflags &= ~0b100000000ul;
	
    } else {
	
	vga_attr = vga_make_color(COLOR_FOREGROUND, COLOR_BACKGROUND);
	vga_puts("+++ Debug Exception Caught by Monitor +++");
	dump_entry(vector,excp);
	vga_attr = vga_make_color(COLOR_PROMPT_FOREGROUND, COLOR_PROMPT_BACKGROUND);
	nk_monitor_loop();
	
	
	// track any potential changes in dr7 
	dr7.val = read_dr7();
	// if we hit a breakpoint, disable breakpoint for one step
	if (dr6.dr0_detect && dr7.type0 == 0 && dr7.local0 && dr7.global0) {
	    breakpoint_addr[my_cpu_id()] = (void*) read_dr0();
	    breakpoint_drnum[my_cpu_id()] = 0;
	    disable(0);
	    excp->rflags |= 0b100000000ul;
	}
	if (dr6.dr1_detect && dr7.type1 == 0 && dr7.local1 && dr7.global1) {
	    breakpoint_addr[my_cpu_id()] = (void*) read_dr1();
	    breakpoint_drnum[my_cpu_id()] = 1;
	    disable(1);
	    excp->rflags |= 0b100000000ul;
	}
	if (dr6.dr2_detect && dr7.type2 == 0 && dr7.local2 && dr7.global2) {
	    breakpoint_addr[my_cpu_id()] = (void*) read_dr2();
	    breakpoint_drnum[my_cpu_id()] = 2;
	    disable(2);
	    excp->rflags |= 0b100000000ul;
	}
	if (dr6.dr3_detect && dr7.type3 == 0 && dr7.local3 && dr7.global3) {
	    breakpoint_addr[my_cpu_id()] = (void*) read_dr3();
	    breakpoint_drnum[my_cpu_id()] = 3;
	    disable(3);
	    excp->rflags |= 0b100000000ul;
	}
    
    }
    
    write_dr6(0);
    
    monitor_deinit(0);
    
    return 0;
}

// handle shell command
static int handle_monitor(char *buf, void *priv)
{
    nk_monitor_entry();
    return 0;
}

static struct shell_cmd_impl monitor_impl = {
    .cmd = "monitor",
    .help_str = "monitor",
    .handler = handle_monitor,
};

nk_register_shell_cmd(monitor_impl);


