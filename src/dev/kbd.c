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
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Yang Wu, Fei Luo and Yuanhui Yang
 *          {YangWu2015, FeiLuo2015, YuanhuiYang2015}@u.northwestern.edu
 *          Peter Dinda <pdinda@northwestern.edu>
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/irq.h>
#include <nautilus/thread.h>
#include <dev/kbd.h>
#include <nautilus/vc.h>

#ifndef NAUT_CONFIG_DEBUG_KBD
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("kbd: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("kbd: " fmt, ##args)
#define INFO(fmt, args...)  INFO_PRINT("kbd: " fmt, ##args)


#define SCAN_MAX_QUEUE 16

static enum { VC_START, VC_CONTEXT, VC_PREV, VC_NEXT, VC_MENU, VC_PREV_ALT, VC_NEXT_ALT, VC_MENU_ALT}  switch_state = VC_START;
static uint8_t switcher_num_queued=0;
static nk_scancode_t switcher_scancode_queue[SCAN_MAX_QUEUE];

#define KB_KEY_RELEASE 0x80

static const nk_keycode_t NoShiftNoCaps[] = {
    KEY_UNKNOWN, ASCII_ESC, '1', '2',   /* 0x00 - 0x03 */
    '3', '4', '5', '6',                 /* 0x04 - 0x07 */
    '7', '8', '9', '0',                 /* 0x08 - 0x0B */
    '-', '=', ASCII_BS, '\t',           /* 0x0C - 0x0F */
    'q', 'w', 'e', 'r',                 /* 0x10 - 0x13 */
    't', 'y', 'u', 'i',                 /* 0x14 - 0x17 */
    'o', 'p', '[', ']',                 /* 0x18 - 0x1B */
    '\r', KEY_LCTRL, 'a', 's',          /* 0x1C - 0x1F */
    'd', 'f', 'g', 'h',                 /* 0x20 - 0x23 */
    'j', 'k', 'l', ';',                 /* 0x24 - 0x27 */
    '\'', '`', KEY_LSHIFT, '\\',        /* 0x28 - 0x2B */
    'z', 'x', 'c', 'v',                 /* 0x2C - 0x2F */
    'b', 'n', 'm', ',',                 /* 0x30 - 0x33 */
    '.', '/', KEY_RSHIFT, KEY_PRINTSCRN, /* 0x34 - 0x37 */
    KEY_LALT, ' ', KEY_CAPSLOCK, KEY_F1, /* 0x38 - 0x3B */
    KEY_F2, KEY_F3, KEY_F4, KEY_F5,     /* 0x3C - 0x3F */
    KEY_F6, KEY_F7, KEY_F8, KEY_F9,     /* 0x40 - 0x43 */
    KEY_F10, KEY_NUMLOCK, KEY_SCRLOCK, KEY_KPHOME,  /* 0x44 - 0x47 */
    KEY_KPUP, KEY_KPPGUP, KEY_KPMINUS, KEY_KPLEFT,  /* 0x48 - 0x4B */
    KEY_KPCENTER, KEY_KPRIGHT, KEY_KPPLUS, KEY_KPEND,  /* 0x4C - 0x4F */
    KEY_KPDOWN, KEY_KPPGDN, KEY_KPINSERT, KEY_KPDEL,  /* 0x50 - 0x53 */
    KEY_SYSREQ, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,  /* 0x54 - 0x57 */
};

static const nk_keycode_t ShiftNoCaps[] = {
    KEY_UNKNOWN, ASCII_ESC, '!', '@',   /* 0x00 - 0x03 */
    '#', '$', '%', '^',                 /* 0x04 - 0x07 */
    '&', '*', '(', ')',                 /* 0x08 - 0x0B */
    '_', '+', ASCII_BS, '\t',           /* 0x0C - 0x0F */
    'Q', 'W', 'E', 'R',                 /* 0x10 - 0x13 */
    'T', 'Y', 'U', 'I',                 /* 0x14 - 0x17 */
    'O', 'P', '{', '}',                 /* 0x18 - 0x1B */
    '\r', KEY_LCTRL, 'A', 'S',          /* 0x1C - 0x1F */
    'D', 'F', 'G', 'H',                 /* 0x20 - 0x23 */
    'J', 'K', 'L', ':',                 /* 0x24 - 0x27 */
    '"', '~', KEY_LSHIFT, '|',          /* 0x28 - 0x2B */
    'Z', 'X', 'C', 'V',                 /* 0x2C - 0x2F */
    'B', 'N', 'M', '<',                 /* 0x30 - 0x33 */
    '>', '?', KEY_RSHIFT, KEY_PRINTSCRN, /* 0x34 - 0x37 */
    KEY_LALT, ' ', KEY_CAPSLOCK, KEY_F1, /* 0x38 - 0x3B */
    KEY_F2, KEY_F3, KEY_F4, KEY_F5,     /* 0x3C - 0x3F */
    KEY_F6, KEY_F7, KEY_F8, KEY_F9,     /* 0x40 - 0x43 */
    KEY_F10, KEY_NUMLOCK, KEY_SCRLOCK, KEY_KPHOME,  /* 0x44 - 0x47 */
    KEY_KPUP, KEY_KPPGUP, KEY_KPMINUS, KEY_KPLEFT,  /* 0x48 - 0x4B */
    KEY_KPCENTER, KEY_KPRIGHT, KEY_KPPLUS, KEY_KPEND,  /* 0x4C - 0x4F */
    KEY_KPDOWN, KEY_KPPGDN, KEY_KPINSERT, KEY_KPDEL,  /* 0x50 - 0x53 */
    KEY_SYSREQ, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,  /* 0x54 - 0x57 */
};

static const nk_keycode_t NoShiftCaps[] = {
    KEY_UNKNOWN, ASCII_ESC, '1', '2',   /* 0x00 - 0x03 */
    '3', '4', '5', '6',                 /* 0x04 - 0x07 */
    '7', '8', '9', '0',                 /* 0x08 - 0x0B */
    '-', '=', ASCII_BS, '\t',           /* 0x0C - 0x0F */
    'Q', 'W', 'E', 'R',                 /* 0x10 - 0x13 */
    'T', 'Y', 'U', 'I',                 /* 0x14 - 0x17 */
    'O', 'P', '[', ']',                 /* 0x18 - 0x1B */
    '\r', KEY_LCTRL, 'A', 'S',          /* 0x1C - 0x1F */
    'D', 'F', 'G', 'H',                 /* 0x20 - 0x23 */
    'J', 'K', 'L', ';',                 /* 0x24 - 0x27 */
    '\'', '`', KEY_LSHIFT, '\\',        /* 0x28 - 0x2B */
    'Z', 'X', 'C', 'V',                 /* 0x2C - 0x2F */
    'B', 'N', 'M', ',',                 /* 0x30 - 0x33 */
    '.', '/', KEY_RSHIFT, KEY_PRINTSCRN, /* 0x34 - 0x37 */
    KEY_LALT, ' ', KEY_CAPSLOCK, KEY_F1, /* 0x38 - 0x3B */
    KEY_F2, KEY_F3, KEY_F4, KEY_F5,     /* 0x3C - 0x3F */
    KEY_F6, KEY_F7, KEY_F8, KEY_F9,     /* 0x40 - 0x43 */
    KEY_F10, KEY_NUMLOCK, KEY_SCRLOCK, KEY_KPHOME,  /* 0x44 - 0x47 */
    KEY_KPUP, KEY_KPPGUP, KEY_KPMINUS, KEY_KPLEFT,  /* 0x48 - 0x4B */
    KEY_KPCENTER, KEY_KPRIGHT, KEY_KPPLUS, KEY_KPEND,  /* 0x4C - 0x4F */
    KEY_KPDOWN, KEY_KPPGDN, KEY_KPINSERT, KEY_KPDEL,  /* 0x50 - 0x53 */
    KEY_SYSREQ, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,  /* 0x54 - 0x57 */
};

static const nk_keycode_t ShiftCaps[] = {
    KEY_UNKNOWN, ASCII_ESC, '!', '@',   /* 0x00 - 0x03 */
    '#', '$', '%', '^',                 /* 0x04 - 0x07 */
    '&', '*', '(', ')',                 /* 0x08 - 0x0B */
    '_', '+', ASCII_BS, '\t',           /* 0x0C - 0x0F */
    'q', 'w', 'e', 'r',                 /* 0x10 - 0x13 */
    't', 'y', 'u', 'i',                 /* 0x14 - 0x17 */
    'o', 'p', '{', '}',                 /* 0x18 - 0x1B */
    '\r', KEY_LCTRL, 'a', 's',          /* 0x1C - 0x1F */
    'd', 'f', 'g', 'h',                 /* 0x20 - 0x23 */
    'j', 'k', 'l', ':',                 /* 0x24 - 0x27 */
    '"', '~', KEY_LSHIFT, '|',          /* 0x28 - 0x2B */
    'z', 'x', 'c', 'v',                 /* 0x2C - 0x2F */
    'b', 'n', 'm', '<',                 /* 0x30 - 0x33 */
    '>', '?', KEY_RSHIFT, KEY_PRINTSCRN, /* 0x34 - 0x37 */
    KEY_LALT, ' ', KEY_CAPSLOCK, KEY_F1, /* 0x38 - 0x3B */
    KEY_F2, KEY_F3, KEY_F4, KEY_F5,     /* 0x3C - 0x3F */
    KEY_F6, KEY_F7, KEY_F8, KEY_F9,     /* 0x40 - 0x43 */
    KEY_F10, KEY_NUMLOCK, KEY_SCRLOCK, KEY_KPHOME,  /* 0x44 - 0x47 */
    KEY_KPUP, KEY_KPPGUP, KEY_KPMINUS, KEY_KPLEFT,  /* 0x48 - 0x4B */
    KEY_KPCENTER, KEY_KPRIGHT, KEY_KPPLUS, KEY_KPEND,  /* 0x4C - 0x4F */
    KEY_KPDOWN, KEY_KPPGDN, KEY_KPINSERT, KEY_KPDEL,  /* 0x50 - 0x53 */
    KEY_SYSREQ, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,  /* 0x54 - 0x57 */
};



#define KBD_DATA_REG 0x60
#define KBD_ACK_REG  0x61
#define KBD_CMD_REG  0x64

#define KBD_RELEASE  0x80

#define STATUS_OUTPUT_FULL 0x1

static nk_keycode_t flags = 0;


static void queue_scancode(nk_scancode_t scan)
{
  if (switcher_num_queued==SCAN_MAX_QUEUE) { 
    ERROR("Out of room in switcher queue\n");
  } else {
    switcher_scancode_queue[switcher_num_queued++] = scan;
  }
}

static void dequeue_scancodes()
{
  int i;

  for (i=0;i<switcher_num_queued;i++) { 
    nk_vc_handle_input(switcher_scancode_queue[i]);
  }

  switcher_num_queued=0;
}

static void flush_scancodes()
{
  switcher_num_queued=0;
}

nk_keycode_t kbd_translate(nk_scancode_t scan)
{
  int release;
  const nk_keycode_t *table=0;
  nk_keycode_t cur;
  nk_keycode_t flag;
  

  // update the flags

  release = scan & KB_KEY_RELEASE;
  scan &= ~KB_KEY_RELEASE;

  if (flags & KEY_CAPS_FLAG) { 
    if (flags & KEY_SHIFT_FLAG) { 
      table = ShiftCaps;
    } else {
      table = NoShiftCaps;
    }
  } else {
    if (flags & KEY_SHIFT_FLAG) { 
      table = ShiftNoCaps;
    } else {
      table = NoShiftNoCaps;
    }
  }    
  
  cur = table[scan];
  
  flag = 0;
  switch (cur) { 
  case KEY_LSHIFT:
  case KEY_RSHIFT:
    flag = KEY_SHIFT_FLAG;
    break;
  case KEY_LCTRL:
  case KEY_RCTRL:
    flag = KEY_CTRL_FLAG;
    break;
  case KEY_LALT:
  case KEY_RALT:
    flag = KEY_ALT_FLAG;
    break;
  case KEY_CAPS_FLAG:
    flag = KEY_CAPS_FLAG;
    break;
  default:
    goto do_noflags;
    break;
  }
  
  // do_flags:
  if (flag==KEY_CAPS_FLAG) { 
    if ((!release) && (flags & KEY_CAPS_FLAG)) { 
      // turn off caps lock on second press
      flags &= ~KEY_CAPS_FLAG;
      flag = 0;
    } 
  }

  if (release) {
    flags &= ~(flag);
  } else {
    flags |= flag;
  }

  DEBUG("Handled flag change (flags now 0x%x)\n", flags);

  return NO_KEY;

 do_noflags:
  if (release) { 
    DEBUG("Handled key release (returning 0x%x)\n", flags|cur);
    return flags | cur;
  } else {
    return NO_KEY;
  }
}


#define ALT 0x38
#define ONE 0x2
#define TWO 0x3
#define TILDE 0x29
 
/*
  Special case handling of scancodes relating to virtual
  console switching.  This happens regardless of whether
  the current console is raw or cooked.   If the console
  switch state machine doesn't recognize the scancode (or
  the ones it has accumulated), it passes them to the 
  virtual console subsystem for further processing.  
*/
static int switcher(nk_scancode_t scan) 
{
  queue_scancode(scan);

  switch (switch_state) { 
  case VC_START:
    if (scan==ALT) { 
      DEBUG("VC CONTEXT\n");
      switch_state = VC_CONTEXT;
    } else {
      DEBUG("VC RESTART\n");
      dequeue_scancodes();
      switch_state = VC_START;
    }
    break;
  case VC_CONTEXT:
    if (scan==ONE) { 
      DEBUG("VC PREV\n");
      switch_state = VC_PREV;
    } else if (scan==TWO) { 
      DEBUG("VC NEXT\n");
      switch_state = VC_NEXT;
    } else if (scan==TILDE) {
      DEBUG("VC MENU\n");
      switch_state = VC_MENU;
    } else {
      DEBUG("VC RESTART\n");
      dequeue_scancodes();
      switch_state = VC_START;
    }
    break;
  case VC_PREV:
    if (scan==(ONE | KB_KEY_RELEASE)) { 
      DEBUG("VC PREV ALT\n");
      switch_state = VC_PREV_ALT;
    } else {
      DEBUG("VC RESTART\n");
      switch_state = VC_START;
      dequeue_scancodes();
    }
    break;
  case VC_NEXT:
    if (scan==(TWO | KB_KEY_RELEASE)) { 
      DEBUG("VC NEXT ALT\n");
      switch_state = VC_NEXT_ALT;
    } else {
      DEBUG("VC RESTART\n");
      switch_state = VC_START;
      dequeue_scancodes();
    }
    break;
  case VC_MENU:
    if (scan==(TILDE | KB_KEY_RELEASE)) { 
      DEBUG("VC MENU ALT\n");
      switch_state = VC_MENU_ALT;
    } else {
      DEBUG("VC RESTART\n");
      switch_state = VC_START;
      dequeue_scancodes();
    }
    break;
  case VC_PREV_ALT:
    if (scan==(ALT | KB_KEY_RELEASE)) { 
      DEBUG("VC SWITCH TO PREV\n");
      nk_switch_to_prev_vc();
      switch_state = VC_START;
      flush_scancodes();
    } else {
      DEBUG("VC RESTART\n");
      dequeue_scancodes();
      switch_state = VC_START;
    }
    break;
  case VC_NEXT_ALT:
    if (scan==(ALT | KB_KEY_RELEASE)) { 
      DEBUG("VC SWITCH TO NEXT\n");
      nk_switch_to_next_vc();
      switch_state = VC_START;
      flush_scancodes();
    } else {
      DEBUG("VC RESTART\n");
      dequeue_scancodes();
      switch_state = VC_START;
    }
    break;  
  case VC_MENU_ALT:
    if (scan==(ALT | KB_KEY_RELEASE)) { 
      DEBUG("VC SWITCH TO MENU\n");
      nk_switch_to_vc_list();
      switch_state = VC_START;
      flush_scancodes();
    } else {
      DEBUG("VC RESTART\n");
      dequeue_scancodes();
      switch_state = VC_START;
    }
    break;
  default:
    DEBUG("VC HUH?\n");
    dequeue_scancodes();
    switch_state = VC_START;
    break;
  }
  return 0;
}


static int 
kbd_handler (excp_entry_t * excp, excp_vec_t vec)
{
  
  uint8_t status;
  nk_scancode_t scan;
  uint8_t key;
  uint8_t flag;
  
  status = inb(KBD_CMD_REG);
  
  io_delay();

  if ((status & STATUS_OUTPUT_FULL) != 0) {
    scan  = inb(KBD_DATA_REG);
    DEBUG("Keyboard: status=0x%x, scancode=0x%x\n", status, scan);
    io_delay();
    
    
#if NAUT_CONFIG_THREAD_EXIT_KEYCODE == 0xc4
    // Vestigal debug handling to force thread exit
    if (scan == 0xc4) {
      void * ret = NULL;
      IRQ_HANDLER_END();
      kbd_reset();
      nk_thread_exit(ret);
    }
#endif
    
    switcher(scan);
    
  }
  
  IRQ_HANDLER_END();
  return 0;
}



int
kbd_init (struct naut_info * naut)
{
  INFO("init\n");
  register_irq_handler(1, kbd_handler, NULL);
  kbd_reset();
  return 0;
}


int kbd_reset()
{
  INFO("reset\n");
  flags=0;
  return 0;
}

int kbd_deinit()
{
  INFO("deinit\n");
  return 0;
}




