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
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Yang Wu, Fei Luo and Yuanhui Yang
 *          {YangWu2015, FeiLuo2015, YuanhuiYang2015}@u.northwestern.edu
 *          Peter Dinda <pdinda@northwestern.edu>
 *          William Wallace, Scott Renshaw 
 *          {WilliamWallace2018,ScottRenshaw2018}@u.northwestern.edu>
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/irq.h>
#include <nautilus/thread.h>
#include <dev/ps2.h>
#include <nautilus/vc.h>
#include <nautilus/dev.h>
#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING
#include <nautilus/gdb-stub.h>
#endif

#ifdef NAUT_CONFIG_ENABLE_MONITOR
#include <nautilus/monitor.h>
#endif

#ifndef NAUT_CONFIG_DEBUG_PS2
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("ps2: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("ps2: " fmt, ##args)
#define INFO(fmt, args...)  INFO_PRINT("ps2: " fmt, ##args)


/***************************************************************
    GENERAL PS2
****************************************************************/


typedef union ps2_status {
    uint8_t  val;
    struct {
	uint8_t obf:1;  // output buffer full (1=> can read from 0x60)
	uint8_t ibf:1;  // input buffer full (0=> can write to either port)
	uint8_t sys:1;  // set on successful reset
	uint8_t a2:1;   // last address (0=>0x60, 1=>0x64)
	uint8_t inh:1;  // keyboard inhibit, active low
	uint8_t mobf:1; // mouse buffer full
	uint8_t to:1;   // timeout
	uint8_t perr:1; // parity error
    } ;
} __packed ps2_status_t;

typedef union ps2_cmd {
    uint8_t  val;
    struct {
	uint8_t kint:1; // enable keyboard interrupts
	uint8_t mint:1; // enable mouse interrupts
	uint8_t sys:1;  // set/clear sysflag / do BAT
	uint8_t rsv1:1; 
	uint8_t ken:1;  // keyboard enable, active low
	uint8_t men:1;  // mouse enable, active low
	uint8_t xlat:1; // do scancode translation
	uint8_t rsv2:1;
    } ;
} __packed ps2_cmd_t;


// INPUT => want to write to 0x60 or 0x64
// OUTPUT => want to read from 0x60
typedef enum {INPUT, OUTPUT} wait_t;


static int ps2_wait(wait_t w)
{
    volatile ps2_status_t status;

    do {
	status.val = inb(KBD_STATUS_REG);
	//      INFO("obf=%d ibf=%d req=%s\n",status.obf, status.ibf, w==INPUT ? "input" : "output");
    } while (((!status.obf) && (w==OUTPUT)) ||
	     ((status.ibf) && (w==INPUT))) ;

    return 0;
} 

/***************************************************************
    KEYBOARD
****************************************************************/


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
    nk_vc_handle_keyboard(switcher_scancode_queue[i]);
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
  case KEY_CAPSLOCK:
    flag = KEY_CAPS_FLAG;
    break;
  default:
    goto do_noflags;
    break;
  }

  // do_flags:
  if (flag==KEY_CAPS_FLAG) { 
    if(!release) {
      if ((flags & KEY_CAPS_FLAG)) {
        // turn off caps lock on second press
        flags &= ~KEY_CAPS_FLAG;
      } else {
        flags |= flag;
      }
    }
    return NO_KEY;
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
kbd_handler (excp_entry_t * excp, excp_vec_t vec, void *state)
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
      ps2_kbd_reset();
      nk_thread_exit(ret);
    }
#endif

#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING
    if (scan == 0x42) {
      // F8 down - stop
        nk_gdb_handle_exception(excp, vec, 0, (void *)0x1ULL);
      // now ignore the key
      goto out;
    }
    if (scan == 0xc2) {
      // F8 up - ignore the key
      goto out;
    }

#endif

#ifdef NAUT_CONFIG_ENABLE_MONITOR
    if (scan == 0xc3) {
      // F9 up - stop
      nk_monitor_entry();

      // now ignore the key
      goto out;
    }
    if (scan == 0x43) {
      // F9 down - ignore the key
      goto out;
    }
#endif
    
    switcher(scan);

    goto out; // to avoid label warning
    
  }

 out:

  IRQ_HANDLER_END();
  return 0;
}

/***************************************************************
    MOUSE
****************************************************************/

union mouse_packet {
    uint8_t data[3];
    struct {
	uint8_t  bl:1;
	uint8_t  br:1;
	uint8_t  bm:1;
	uint8_t  a0:1;
	uint8_t  xs:1;
	uint8_t  ys:1;
	uint8_t  xo:1;
	uint8_t  yo:1;
	uint8_t  xm;
	uint8_t  ym;
    } __packed;
} __packed; 



static union mouse_packet cur_mouse_packet;
static int cur_mouse_packet_byte=0;


static void mouse_write(uint8_t d)
{
    ps2_wait(INPUT);
    outb(0xd4, KBD_CMD_REG);  // address mouse
    ps2_wait(INPUT);
    outb(d, KBD_DATA_REG);
}

static uint8_t mouse_read()
{
    ps2_wait(OUTPUT);
    return inb(KBD_DATA_REG);
}

int ps2_mouse_reset()
{
    ps2_cmd_t cmd;
    uint32_t rc;

#ifdef NAUT_CONFIG_DISABLE_PS2_MOUSE
    DEBUG("treating mouse as nonexistent\n");
    return -1;
#endif

    DEBUG("Reseting mouse\n");

    // does the mouse port exist?
    ps2_wait(INPUT);
    outb(0xa9,KBD_CMD_REG);
    ps2_wait(OUTPUT);
    cmd.val = inb(KBD_DATA_REG);
    
    if (cmd.val!=0) { 
	INFO("mouse does not exist\n");
	return -1;
    }
    
    // enable mouse in PS2 controller
    ps2_wait(INPUT);
    outb(0xa8,KBD_CMD_REG); 

    DEBUG("Mouse enabled on PS2 controller\n");

    // enable mouse interrupts on PS2 controller
    ps2_wait(INPUT);
    outb(0x20,KBD_CMD_REG); // prepare to read command word
    ps2_wait(OUTPUT);
    cmd.val = inb(KBD_DATA_REG);
    DEBUG("Initial command word is 0x%x\n",cmd.val);
    cmd.mint = 1;
    cmd.men = 0;  // enable is active low
    ps2_wait(INPUT);
    outb(0x60, KBD_CMD_REG); // prepare to write command word
    ps2_wait(INPUT);
    outb(cmd.val, KBD_DATA_REG);
    DEBUG("PS2 command word is now 0x%x\n",cmd.val);

    // reset mouse
    DEBUG("Reset\n");
    mouse_write(0xff);  
    rc = mouse_read(); 
    rc = (rc << 8) | mouse_read();
    rc = (rc << 8) | mouse_read();
    if (rc==0xfaaa00) {  // ACK SELF-TEST-PASSED DONE
	DEBUG("Mouse reset good\n");
    } else {
	ERROR("Mouse reset failed (%x)\n",rc);
	return -1;
    }

    // determine mouse type
    mouse_write(0xf2);  
    rc = mouse_read(); 
    if (rc!=0xfa) { 
	ERROR("On reset, mouse did not return ack, but rather 0x%x\n",rc);
	return -1;
    } else {
	rc = mouse_read();
	DEBUG("Mouse is of type 0x%x (%s)\n",rc, rc==0 ? "generic ps/2" : "something weird");
    }

    // configure mouse for defaults
    mouse_write(0xf6);  
    rc = mouse_read(); 
    if (rc!=0xfa) { 
	ERROR("On config, mouse did not return ack, but rather 0x%x\n",rc);
	return -1;
    }

    // set stream mode (shouldn't have to do this)
    mouse_write(0xea);
    rc = mouse_read();
    if (rc!=0xfa) { 
	ERROR("Cannot set stream mode (%x)\n",rc);
	return -1;
    }

    // enable mouse data reporting
    mouse_write(0xf4);
    rc = mouse_read(); 
    if (rc!=0xfa) { 
	ERROR("On enable, mouse did not return ack, but rather 0x%x\n",rc);
	return -1;
    }
    
    DEBUG("Mouse reset done\n");

    return 0;
}



static int mouse_handler(excp_entry_t * excp, excp_vec_t vec, void *state)
{
    ps2_status_t status;
    int count=0;

    // read just one byte and continue
    // do not read status since this probably has side effects

    uint8_t data;
    data = inb(KBD_DATA_REG);
    
    //INFO("Have mouse data (%x)\n",(uint32_t)data);
    cur_mouse_packet.data[cur_mouse_packet_byte] = data;

    if (cur_mouse_packet_byte==0 && !(data&0x8)) { 
	ERROR("Invalid first byte (%02x) - lost sync, retry\n",data);
	goto out;
    }

    cur_mouse_packet_byte++;

    if (cur_mouse_packet_byte==3) {
	// done with packet
	int dx = cur_mouse_packet.xm - ((cur_mouse_packet.data[0] << 4) & 0x100);
	int dy = cur_mouse_packet.ym - ((cur_mouse_packet.data[0] << 3) & 0x100);

	if (cur_mouse_packet.xo || cur_mouse_packet.yo) { 
	    DEBUG("Mouse Packet Ignored: overflows: xo: %d yo: %d\n",
		  cur_mouse_packet.xo, cur_mouse_packet.yo);
	    DEBUG("Mouse: %02x %02x %02x\n", cur_mouse_packet.data[0], cur_mouse_packet.data[1], cur_mouse_packet.data[2]);
	} else {
	    nk_mouse_event_t m;
	    DEBUG("Mouse Packet: buttons: %s %s %s\n",
		  cur_mouse_packet.bl ? "down" : "up",
		  cur_mouse_packet.bm ? "down" : "up",
		  cur_mouse_packet.br ? "down" : "up");
	    DEBUG("Mouse Packet: overflows: xo: %d yo: %d\n",
		  cur_mouse_packet.xo, cur_mouse_packet.yo);
	    DEBUG("Mouse Packet: dx: %d dy: %d\n", dx, dy);
	    m.left = cur_mouse_packet.bl;
	    m.middle = cur_mouse_packet.bm;
	    m.right = cur_mouse_packet.br;
	    m.res = 4; // default mouse res
	    m.dx = dx;
	    m.dy = dy;

	    nk_vc_handle_mouse(&m);

	}
	cur_mouse_packet_byte=0;
    }
    
 out:
    IRQ_HANDLER_END();
    return 0;

}

int ps2_kbd_reset()
{
  DEBUG("kbd reset\n");
  flags=0;
  return 0;
}

#define HAVE_NO_KEYBOARD        -1
#define HAVE_KEYBOARD_AND_MOUSE 0
#define HAVE_KEYBOARD           1

int ps2_reset()
{
  if (ps2_kbd_reset()) { 
    return HAVE_NO_KEYBOARD;
  } else {
    if (ps2_mouse_reset()) { 
      return HAVE_KEYBOARD;
    } else {
      return HAVE_KEYBOARD_AND_MOUSE;
    }
  }
}

static struct nk_dev_int kops = {
    .open=0,
    .close=0,
};

static struct nk_dev_int mops = {
    .open=0,
    .close=0,
};
   
int ps2_init(struct naut_info * naut)
{
  int rc;

  INFO("init\n");

  rc=ps2_reset();
  
  if (rc==HAVE_NO_KEYBOARD) { 
    return -1;
  } else {
    if (rc==HAVE_KEYBOARD || rc == HAVE_KEYBOARD_AND_MOUSE) { 
      nk_dev_register("ps2-keyboard",NK_DEV_GENERIC,0,&kops,0);
      register_irq_handler(1, kbd_handler, NULL);
      nk_unmask_irq(1);
    } 
    if (rc==HAVE_KEYBOARD_AND_MOUSE) { 
      nk_dev_register("ps2-mouse",NK_DEV_GENERIC,0,&mops,0);
      register_irq_handler(12, mouse_handler, NULL);
      nk_unmask_irq(12);
    }
  }

  return 0;
}



int ps2_deinit()
{
  INFO("deinit\n");
  ps2_reset();
  nk_mask_irq(1);
  nk_mask_irq(12);
  return 0;
}




