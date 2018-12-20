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
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *         Yang Wu, Fei Luo and Yuanhui Yang
 *         {YangWu2015, FeiLuo2015, YuanhuiYang2015}@u.northwestern.edu
 *         Peter Dinda <pdinda@northwestern.edu>
 *         William Wallace, Scott Renshaw 
 *         {WilliamWallace2018,ScottRenshaw2018}@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __PS2_H__
#define __PS2_H__


struct naut_info;

// Keys and scancodes are represented internally as 16 bits
// so we can indicate nonexistence (via -1)
//
// A "keycode" consists of the translated key (lower 8 bits)
// combined with an upper byte that reflects status of the 
// different modifier keys
typedef uint16_t nk_keycode_t;
typedef uint16_t nk_scancode_t;

#define KBD_DATA_REG 0x60
#define KBD_ACK_REG  0x61
#define KBD_CMD_REG  0x64
#define KBD_STATUS_REG  0x64

// Special tags to indicate unavailabilty
#define NO_KEY      ((nk_keycode_t)(0xffff))
#define NO_SCANCODE ((nk_scancode_t)(0xffff))

// Special flags for a keycode that reflect status of 
// modifiers
#define KEY_SPECIAL_FLAG 0x0100
#define KEY_KEYPAD_FLAG  0x0200
#define KEY_SHIFT_FLAG   0x1000
#define KEY_ALT_FLAG     0x2000
#define KEY_CTRL_FLAG    0x4000
#define KEY_CAPS_FLAG    0x8000

// Special ascii characters
#define ASCII_ESC 0x1B
#define ASCII_BS  0x08

// PC-specific keys
#define _SPECIAL(num) (KEY_SPECIAL_FLAG | (num))
#define KEY_UNKNOWN _SPECIAL(0)
#define KEY_F1      _SPECIAL(1)
#define KEY_F2      _SPECIAL(2)
#define KEY_F3      _SPECIAL(3)
#define KEY_F4      _SPECIAL(4)
#define KEY_F5      _SPECIAL(5)
#define KEY_F6      _SPECIAL(6)
#define KEY_F7      _SPECIAL(7)
#define KEY_F8      _SPECIAL(8)
#define KEY_F9      _SPECIAL(9)
#define KEY_F10     _SPECIAL(10)
#define KEY_F11     _SPECIAL(11)
#define KEY_F12     _SPECIAL(12)
#define KEY_LCTRL   _SPECIAL(13)
#define KEY_RCTRL   _SPECIAL(14)
#define KEY_LSHIFT  _SPECIAL(15)
#define KEY_RSHIFT  _SPECIAL(16)
#define KEY_LALT    _SPECIAL(17)
#define KEY_RALT    _SPECIAL(18)
#define KEY_PRINTSCRN _SPECIAL(19)
#define KEY_CAPSLOCK _SPECIAL(20)
#define KEY_NUMLOCK _SPECIAL(21)
#define KEY_SCRLOCK _SPECIAL(22)
#define KEY_SYSREQ  _SPECIAL(23)

// more pc-specific keys
#define KEYPAD_START 128
#define _KEYPAD(num) (KEY_KEYPAD_FLAG | KEY_SPECIAL_FLAG | (num+KEYPAD_START))
#define KEY_KPHOME  _KEYPAD(0)
#define KEY_KPUP    _KEYPAD(1)
#define KEY_KPPGUP  _KEYPAD(2)
#define KEY_KPMINUS _KEYPAD(3)
#define KEY_KPLEFT  _KEYPAD(4)
#define KEY_KPCENTER _KEYPAD(5)
#define KEY_KPRIGHT _KEYPAD(6)
#define KEY_KPPLUS  _KEYPAD(7)
#define KEY_KPEND   _KEYPAD(8)
#define KEY_KPDOWN  _KEYPAD(9)
#define KEY_KPPGDN  _KEYPAD(10)
#define KEY_KPINSERT _KEYPAD(11)
#define KEY_KPDEL   _KEYPAD(12)

nk_keycode_t kbd_translate(nk_scancode_t);

typedef struct nk_mouse_event {
    uint8_t  left;
    uint8_t  middle;
    uint8_t  right;
    uint8_t  res; // counts/mm
    sint32_t dx;
    sint32_t dy;
} nk_mouse_event_t;
	

int ps2_init(struct naut_info * naut);
int ps2_reset();
int ps2_kbd_reset();
int ps2_mouse_reset();
int ps2_deinit();


#endif
