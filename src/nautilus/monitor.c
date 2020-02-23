
//#include <stdio.h>

//#include <nautilus/naut_types.h>
#include <nautilus/nautilus.h>

// #include <nautilus/thread.h>
// #include <nautilus/waitqueue.h>
// #include <nautilus/timer.h>
// #include <nautilus/scheduler.h>
// #include <nautilus/msg_queue.h>
// #include <nautilus/list.h>

#include <nautilus/shell.h>
#include <nautilus/monitor.h>


// Graphics stuff

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

#define DB(x) vga_putchar(x)
#define DHN(x) vga_putchar(((x & 0xF) >= 10) ? (((x & 0xF) - 10) + 'a') : ((x & 0xF) + '0'))
#define DHB(x) DHN(x >> 4) ; DHN(x);
#define DHW(x) DHB(x >> 8) ; DHB(x);
#define DHL(x) DHW(x >> 16) ; DHW(x);
#define DHQ(x) DHL(x >> 32) ; DHL(x);
#define DS(x) { char *__curr = x; while(*__curr) { DB(*__curr); __curr++; } }

#include <nautilus/dr.h>

static char screen_saved[VGA_HEIGHT*VGA_WIDTH*2];
static uint8_t cursor_saved_x;
static uint8_t cursor_saved_y;

// Keyboard stuff

// static const int scan_code_to_key[] = {
//     0, 0, '1', '2',   /* 0x00 - 0x03 */
//     '3', '4', '5', '6',                 /* 0x04 - 0x07 */
//     '7', '8', '9', '0',                 /* 0x08 - 0x0B */
//     '-', '=', 0, '\t',           /* 0x0C - 0x0F */
//     'q', 'w', 'e', 'r',                 /* 0x10 - 0x13 */
//     't', 'y', 'u', 'i',                 /* 0x14 - 0x17 */
//     'o', 'p', '[', ']',                 /* 0x18 - 0x1B */
//     '\r', 0, 'a', 's',          /* 0x1C - 0x1F */
//     'd', 'f', 'g', 'h',                 /* 0x20 - 0x23 */
//     'j', 'k', 'l', ';',                 /* 0x24 - 0x27 */
//     '\'', '`', 0, '\\',        /* 0x28 - 0x2B */
//     'z', 'x', 'c', 'v',                 /* 0x2C - 0x2F */
//     'b', 'n', 'm', ',',                 /* 0x30 - 0x33 */
//     '.', '/', 0, 0, /* 0x34 - 0x37 */
//     0, ' ', 0, 0, /* 0x38 - 0x3B */
//     0, 0, 0, 0,     /* 0x3C - 0x3F */
//     0, 0, 0, 0,     /* 0x40 - 0x43 */
//     0, 0, 0, 0,       /* 0x44 - 0x47 */
//     0, 0, 0, 0,       /* 0x48 - 0x4B */
//     0, 0, 0, 0,       /* 0x4C - 0x4F */
//     0, 0, 0, 0,       /* 0x50 - 0x53 */
//     0, 0, 0, 0,       /* 0x54 - 0x57 */
// };

typedef short unsigned int nk_keycode_t;
#define KB_KEY_RELEASE 0x80
static nk_keycode_t flags = 0;

// Special tags to indicate unavailabilty
#define NO_KEY ((nk_keycode_t)(0xffff))
#define NO_SCANCODE ((nk_scancode_t)(0xffff))

// Special flags for a keycode that reflect status of
// modifiers
#define KEY_SPECIAL_FLAG 0x0100
#define KEY_KEYPAD_FLAG 0x0200
#define KEY_SHIFT_FLAG 0x1000
#define KEY_ALT_FLAG 0x2000
#define KEY_CTRL_FLAG 0x4000
#define KEY_CAPS_FLAG 0x8000

// Special ascii characters
#define ASCII_ESC 0x1B
#define ASCII_BS 0x08

// PC-specific keys
#define _SPECIAL(num) (KEY_SPECIAL_FLAG | (num))
#define KEY_UNKNOWN _SPECIAL(0)
#define KEY_F1 _SPECIAL(1)
#define KEY_F2 _SPECIAL(2)
#define KEY_F3 _SPECIAL(3)
#define KEY_F4 _SPECIAL(4)
#define KEY_F5 _SPECIAL(5)
#define KEY_F6 _SPECIAL(6)
#define KEY_F7 _SPECIAL(7)
#define KEY_F8 _SPECIAL(8)
#define KEY_F9 _SPECIAL(9)
#define KEY_F10 _SPECIAL(10)
#define KEY_F11 _SPECIAL(11)
#define KEY_F12 _SPECIAL(12)
#define KEY_LCTRL _SPECIAL(13)
#define KEY_RCTRL _SPECIAL(14)
#define KEY_LSHIFT _SPECIAL(15)
#define KEY_RSHIFT _SPECIAL(16)
#define KEY_LALT _SPECIAL(17)
#define KEY_RALT _SPECIAL(18)
#define KEY_PRINTSCRN _SPECIAL(19)
#define KEY_CAPSLOCK _SPECIAL(20)
#define KEY_NUMLOCK _SPECIAL(21)
#define KEY_SCRLOCK _SPECIAL(22)
#define KEY_SYSREQ _SPECIAL(23)

// more pc-specific keys
#define KEYPAD_START 128
#define _KEYPAD(num) (KEY_KEYPAD_FLAG | KEY_SPECIAL_FLAG | (num + KEYPAD_START))
#define KEY_KPHOME _KEYPAD(0)
#define KEY_KPUP _KEYPAD(1)
#define KEY_KPPGUP _KEYPAD(2)
#define KEY_KPMINUS _KEYPAD(3)
#define KEY_KPLEFT _KEYPAD(4)
#define KEY_KPCENTER _KEYPAD(5)
#define KEY_KPRIGHT _KEYPAD(6)
#define KEY_KPPLUS _KEYPAD(7)
#define KEY_KPEND _KEYPAD(8)
#define KEY_KPDOWN _KEYPAD(9)
#define KEY_KPPGDN _KEYPAD(10)
#define KEY_KPINSERT _KEYPAD(11)
#define KEY_KPDEL _KEYPAD(12)

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

#define KBD_RELEASE 0x80

#define KBD_DATA_REG 0x60
#define KBD_CMD_REG 0x64
#define KBD_STATUS_REG 0x64
typedef uint16_t nk_scancode_t;

nk_keycode_t kbd_translate_monitor(nk_scancode_t scan)
{
  int release;
  const nk_keycode_t *table = 0;
  nk_keycode_t cur;
  nk_keycode_t flag;

  // update the flags

  release = scan & KB_KEY_RELEASE;
  scan &= ~KB_KEY_RELEASE;

  if (flags & KEY_CAPS_FLAG)
  {
    if (flags & KEY_SHIFT_FLAG)
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
    if (flags & KEY_SHIFT_FLAG)
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
      if ((flags & KEY_CAPS_FLAG))
      {
        // turn off caps lock on second press
        flags &= ~KEY_CAPS_FLAG;
        flag = 0;
      }
      else
      {
        flags |= flag;
      }
    }
    return NO_KEY;
  }

  if (release)
  {
    flags &= ~(flag);
  }
  else
  {
    flags |= flag;
  }

  return NO_KEY;

do_noflags:
  if (release)
  { // Chose to display on press, if that is wrong it's an easy change
    return flags | cur;
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

void monitor_init()
{
  vga_copy_out(screen_saved, VGA_WIDTH*VGA_HEIGHT*2);
  vga_get_cursor(&cursor_saved_x, &cursor_saved_y);

  vga_x = vga_y = 0;
  vga_attr = vga_make_color(COLOR_MAGENTA, COLOR_BLACK);
  vga_clear_screen(vga_make_entry(' ', vga_attr));
  vga_set_cursor(vga_x, vga_y);
}

void monitor_deinit()
{
  vga_copy_in(screen_saved, VGA_WIDTH*VGA_HEIGHT*2);
  vga_set_cursor(cursor_saved_x, cursor_saved_y);

}

// writes a line of text into a buffer
static void wait_for_command(char *buf, int buffer_size)
{

  int key;
  int curr = 0;
  while (1)
  {
    key = ps2_wait_for_key();
    uint16_t key_encoded = kbd_translate_monitor(key);
    //vga_clear_screen(vga_make_entry(key, vga_make_color(COLOR_MAGENTA, COLOR_BLACK)));

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
        //Some visual/audio queue that they can't type more
      }

      //strncat(*buf, &key_char, 1);
      //printk(buf);
    }
  };
}

int is_hex_addr(char *addr_as_str) {
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

uint64_t get_hex_addr(char *addr_as_str) {
  
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
      // add ASSERT() later
    }
    
    power_of_16*=16;
  }
  return addr;

}


int is_dec_addr(char *addr_as_str) {
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

uint64_t get_dec_addr(char *addr_as_str) {
  
  uint64_t addr = 0;
  int power_of_10 = 1;
  // iterate backwards from the end of the address to the beginning, stopping before 0x
  for(int i = (strlen(addr_as_str) - 1); i >= 0; i--) {
    char curr = addr_as_str[i];
    if(curr >= '0' && curr <= '9')  {
      addr += (curr - '0')*power_of_10;
    } else {
      // something broken
      // add ASSERT() later
    }
    
    power_of_10*=10;
  }
  return addr;

}

//For Lua Support
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

int execute_quit(char command[]) {
  vga_puts("quit executed");
  return 1;
}

int execute_watch(char command[]) {
  char* potential_addr = get_next_word(NULL);
  char* next_word = get_next_word(NULL);

  if(!potential_addr) {
    vga_puts("too few arguments for watch, expected address");
    return 0;
  }

  if(next_word) {
    vga_puts("too many arguments for watch, expected address");
    return 0;
  }


  if(is_hex_addr(potential_addr)) {
    vga_puts("watchpoint set at address:");
    vga_puts(potential_addr);
    uint64_t addr = get_hex_addr(potential_addr);
    set_watchpoint(addr);
  } else if(is_dec_addr(potential_addr)) {
    vga_puts("watchpoint set at address:");
    vga_puts(potential_addr);
    uint64_t addr = get_dec_addr(potential_addr);
    printk("%d", addr);
    set_watchpoint(addr);
  } else {
    vga_puts("invalid arguments for watch, expected address in hexidecimal format");
  }

  return 0;
}

static enum{
  AWAIT_BREAKPOINT = 0,
  AWAIT_SINGLESTEP
} breakpoint_state;

static void* breakpoint_addr;


int execute_break(char command[]) {
  char* potential_addr = get_next_word(NULL);
  char* next_word = get_next_word(NULL);

  if(!potential_addr) {
    vga_puts("too few arguments for break, expected address");
    return 0;
  }

  if(next_word) {
    vga_puts("too many arguments for break, expected address");
    return 0;
  }


  if(is_hex_addr(potential_addr)) {
    vga_puts("breakpoint set at address:");
    vga_puts(potential_addr);
    uint64_t addr = get_hex_addr(potential_addr);
    set_breakpoint(addr);
    breakpoint_state = AWAIT_BREAKPOINT;
    breakpoint_addr = addr;
  } else if(is_dec_addr(potential_addr)) {
    vga_puts("breakpoint set at address:");
    vga_puts(potential_addr);
    uint64_t addr = get_dec_addr(potential_addr);
    printk("%d", addr);
    set_breakpoint(addr);
    breakpoint_state = AWAIT_BREAKPOINT;
    breakpoint_addr = addr;
  } else {
    vga_puts("invalid arguments for break, expected address in hexidecimal format");
  }

  return 0;

}

int execute_disable(char command[]) {
  vga_puts("disable breakpoints executed");
  disable_breakpoints();

  return 0;
}

int execute_potential_command(char command[])
{
  vga_attr = vga_make_color(COLOR_CYAN, COLOR_BLACK);

  int quit = 0;
  char* print_string = "";

  char* word = get_next_word(command);

  if (strcmp(word, "quit") == 0)
  {
    quit = execute_quit(command);
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
  else if (strcmp(word, "regprint") == 0)
  {
    // do something else
    vga_puts("regprint executed");
  }
  else if (strcmp(word, "memprint") == 0)
  {
    // do something else
    vga_puts("memprint executed");
  }
  /* more else if clauses */
  else /* default: */
  {
    vga_puts("command not recognized");
  }
  vga_attr = vga_make_color(COLOR_MAGENTA, COLOR_BLACK);
  return quit;

}


int nk_monitor_loop()
{
  int buffer_size = VGA_WIDTH * 2; 
  char buffer[buffer_size];

  // Inner loop is indvidual keystrokes, outer loop handles commands
  int done = 0;
  while (!done)
  {
    wait_for_command(buffer, buffer_size);
    //parse_into_words(buffer, buffer_size);
    //printk(buffer); // for debugging
    // vga_puts(buffer);
    vga_putchar('\n');
    done = execute_potential_command(buffer);
    //vga_puts("loop");
  };
  //vga_puts("done"); 
  return 0;
}

static void __attribute__((noinline))
do_backtrace (void ** fp, unsigned depth)
{
    if (!fp || fp >= 0x100000000UL) {//(void**)nk_get_nautilus_info()->sys.mem.phys_mem_avail) {
        return;
    }
    
    DHQ((uint64_t) *(fp+1));
    DS(" ");
    DHQ((uint64_t) *fp);
    DS("\n");
    //"[%2u] RIP: %p RBP: %p\n", depth, *(fp+1), *fp);

    do_backtrace(*fp, depth+1);
}


int nk_monitor_entry()
{
  uint8_t flags = irq_disable_save(); // disable interrupts
  //cli(); // disable interrupts

  //printk("monitor shell command invoked");
  monitor_init();
  //vga_clear_screen(vga_make_entry(' ', vga_make_color(COLOR_MAGENTA, COLOR_BLACK)));
  nk_monitor_loop();
  monitor_deinit();

  irq_enable_restore(flags); // enable interrupts if they were enabled before the monitor call
  return 0;
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

    #define PRINT(x) DS(#x " = "); DHQ(r->x); DS("\n");
    #define PRINTCR(x) DS(#x " = "); DHQ(x); DS("\n");
    
    DS("[-------------- Register Contents --------------]\n");
    PRINT(rip);
    PRINT(rflags);
    PRINT(rbp);
    PRINT(rsp);
    PRINT(rax);
    PRINT(rbx);
    PRINT(rcx);
    PRINT(rdx);
    PRINT(rdi);
    PRINT(rsi);
    PRINT(r8);
    PRINT(r9);
    PRINT(r10);
    PRINT(r11);
    PRINT(r12);
    PRINT(r13);
    PRINT(r14);
    PRINT(r15);
    
    // printk("RIP: %04lx:%016lx\n", r->cs, r->rip);
    // printk("RSP: %04lx:%016lx RFLAGS: %08lx Vector: %08lx Error: %08lx\n", 
    //         r->ss, r->rsp, r->rflags, r->vector, r->err_code);

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

    PRINTCR(cr0);
    PRINTCR(cr2);
    PRINTCR(cr3);
    PRINTCR(cr4);
    PRINTCR(cr8);
    // printk("FS: %016lx(%04x) GS: %016lx(%04x) knlGS: %016lx\n", 
    //         fs, fsi, gs, gsi, sgs);
    // printk("CS: %04x DS: %04x ES: %04x CR0: %016lx\n", 
    //         cs, ds, es, cr0);
    // printk("CR2: %016lx CR3: %016lx CR4: %016lx\n", 
    //         cr2, cr3, cr4);
    // printk("CR8: %016lx EFER: %016lx\n", cr8, efer);

   // printk("[-----------------------------------------------]\n");
}


static void dump_entry(excp_entry_t * excp) {
  //nk_print_regs(r);
  struct nk_regs * r = (struct nk_regs*)((char*)excp - 128);
  monitor_print_regs(r);
  do_backtrace(r->rbp, 0); 
}

int nk_monitor_excp_entry(excp_entry_t * excp,
                    excp_vec_t vector,
		            void *state)
{

  uint8_t flags = irq_disable_save(); // disable interrupts

  monitor_init();
  vga_puts("+++ UNHANDLED EXCEPTION +++");
  dump_entry(excp);
  nk_monitor_loop();
  monitor_deinit();

  irq_enable_restore(flags); // enable interrupts if they were enabled before the monitor call
  return 0;
}
int nk_monitor_irq_entry()
{

  uint8_t flags = irq_disable_save(); // disable interrupts
  monitor_init();
  vga_puts("+++ Unhandled IRQ +++");
  nk_monitor_loop();
  monitor_deinit();

  irq_enable_restore(flags); // enable interrupts if they were enabled before the monitor call
  return 0;
}


int nk_monitor_debug_entry(excp_entry_t * excp,
                    excp_vec_t vector,
		            void *state)
{

  uint8_t flags = irq_disable_save(); // disable interrupts
  // ignoring watchpoint for now
  if(breakpoint_state == AWAIT_SINGLESTEP) {
    // set breakpoint back
    set_breakpoint(breakpoint_addr);
    breakpoint_state = AWAIT_BREAKPOINT;
    excp->rflags &= ~0b100000000ul;

  } else {

    monitor_init();
    vga_puts("+++ Debug Exception Thrown +++");
    dump_entry(excp);
    nk_monitor_loop();
    monitor_deinit();
    disable_breakpoints();
    breakpoint_state = AWAIT_SINGLESTEP;
    excp->rflags |= 0b100000000ul;
  }

  irq_enable_restore(flags); // enable interrupts if they were enabled before the monitor call
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
