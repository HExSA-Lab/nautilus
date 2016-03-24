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
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/irq.h>
#include <nautilus/thread.h>
#include <dev/kbd.h>
#ifdef NAUT_CONFIG_VIRTUAL_CONSOLE
#include <nautilus/vc.h>
#endif

#ifndef NAUT_CONFIG_DEBUG_KBD
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif


int g_switch_state;
uint8_t switcher_scancode[5];

#define LEFT_SHIFT  0x01
#define RIGHT_SHIFT 0x02
#define LEFT_CTRL   0x04
#define RIGHT_CTRL  0x08
#define LEFT_ALT    0x10
#define RIGHT_ALT   0x20
#define SHIFT_MASK  (LEFT_SHIFT | RIGHT_SHIFT)
#define CTRL_MASK   (LEFT_CTRL | RIGHT_CTRL)
#define ALT_MASK    (LEFT_ALT | RIGHT_ALT)

#define KB_KEY_RELEASE 0x80

#define KEY_SPECIAL_FLAG 0x0100
#define KEY_KEYPAD_FLAG  0x0200
#define KEY_SHIFT_FLAG   0x1000
#define KEY_ALT_FLAG     0x2000
#define KEY_CTRL_FLAG    0x4000
#define KEY_RELEASE_FLAG 0x8000

#define ASCII_ESC 0x1B
#define ASCII_BS  0x08

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

static const unsigned short ScancodeNoShiftNoCapsLock[] = {
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

static const unsigned short ScancodeWithShiftNoCapsLock[] = {
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

static const unsigned short ScancodeNoShiftWithCapsLock[] = {
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

static const unsigned short ScancodeWithShiftWithCapsLock[] = {
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

// State
#define DEFAULT 0
#define NoShiftNoCapsLock 1
#define WithShiftNoCapsLock 2
#define NoShiftWithCapsLock 3
#define WithShiftWithCapsLock 4
// State
// Event
/*
Event 0: ScancodeNoShiftNoCapsLock;
Event 1: ScancodeWithShiftNoCapsLock;
Event 2: ScancodeNoShiftWithCapsLock;
Event 3: ScancodeWithShiftWithCapsLock;
*/
// Event

// Action

// Action

// #define KEY_LSHIFT  _SPECIAL(15)
// #define KEY_RSHIFT  _SPECIAL(16)
// #define KEY_CAPSLOCK _SPECIAL(20)

static int translator_prev_state;
static unsigned char translator_prev_input;
static unsigned short cur_output;

// Feed with scancodes, do conversion to cooked output
ushort_t kbd_translator(unsigned char cur_input) {
    switch (translator_prev_state) {
        case DEFAULT: {
            if (cur_input & KB_KEY_RELEASE) {
                cur_output = 0x0000;
                translator_prev_state = DEFAULT;
            }
            else if (cur_input == KEY_CAPSLOCK) {
                cur_output = 0x0000;
                translator_prev_state = NoShiftWithCapsLock;
            }
            else if (cur_input == KEY_LSHIFT 
                || cur_input == KEY_RSHIFT) {
                cur_output = 0x0000;
                translator_prev_state = WithShiftNoCapsLock;
            }
            else {
                cur_output = 0x0000;
                translator_prev_state = NoShiftNoCapsLock;
            }
            translator_prev_input = cur_input;
            break;
        }
        case NoShiftNoCapsLock: {
            if (cur_input & KB_KEY_RELEASE) {
                cur_output = ScancodeNoShiftNoCapsLock[translator_prev_input];
                translator_prev_state = DEFAULT;
            }
            else if (cur_input == KEY_CAPSLOCK) {
                cur_output = ScancodeNoShiftWithCapsLock[translator_prev_input];
                translator_prev_state = NoShiftWithCapsLock;
            }
            else if (cur_input == KEY_LSHIFT 
                || cur_input == KEY_RSHIFT) {
                cur_output = ScancodeNoShiftNoCapsLock[translator_prev_input];
                translator_prev_state = WithShiftNoCapsLock;
            }
            else {
	      if (!(translator_prev_input & KB_KEY_RELEASE)
                    && translator_prev_input != KEY_CAPSLOCK 
                    && translator_prev_input != KEY_LSHIFT 
                    && translator_prev_input != KEY_RSHIFT) {
                    cur_output = ScancodeNoShiftWithCapsLock[translator_prev_input];
                    translator_prev_state = NoShiftNoCapsLock;
                }
                else {
                    cur_output = 0x0000;
                    translator_prev_state = NoShiftNoCapsLock;
                }
            }
            translator_prev_input = cur_input;
            break;
        }
        case WithShiftNoCapsLock: {
            if (cur_input & KB_KEY_RELEASE) {
                if (translator_prev_input == KEY_CAPSLOCK) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftNoCapsLock;
                }
                else if (translator_prev_input == KEY_LSHIFT 
                    || translator_prev_input == KEY_RSHIFT) {
                    cur_output = 0x0000;
                    translator_prev_state = DEFAULT;
                }
                else {
		  if (!(translator_prev_input & KB_KEY_RELEASE )
                        && translator_prev_input != KEY_CAPSLOCK 
                        && translator_prev_input != KEY_LSHIFT 
                        && translator_prev_input != KEY_RSHIFT) {
                        cur_output = ScancodeWithShiftNoCapsLock[translator_prev_input];
                        translator_prev_state = WithShiftNoCapsLock;
                    }
                    else {
                        cur_output = 0x0000;
                        translator_prev_state = WithShiftNoCapsLock;
                    }
                }
            }
            else if (cur_input == KEY_CAPSLOCK) {
                if (translator_prev_input == KEY_CAPSLOCK) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftNoCapsLock;
                }
                else if (translator_prev_input == KEY_LSHIFT 
                    || translator_prev_input == KEY_RSHIFT) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftNoCapsLock;
                }
                else {
                    if (translator_prev_input != KEY_CAPSLOCK 
                        && translator_prev_input != KEY_LSHIFT 
                        && translator_prev_input != KEY_RSHIFT) {
                        cur_output = ScancodeWithShiftNoCapsLock[translator_prev_input];
                        translator_prev_state = WithShiftNoCapsLock;
                    }
                    else {
                        cur_output = 0x0000;
                        translator_prev_state = WithShiftNoCapsLock;
                    }
                }
            }
            else if (cur_input == KEY_LSHIFT || cur_input == KEY_RSHIFT) {
                if (translator_prev_input == KEY_CAPSLOCK) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftNoCapsLock;
                }
                else if (translator_prev_input == KEY_LSHIFT 
                    || translator_prev_input == KEY_RSHIFT) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftNoCapsLock;
                }
                else {
                    if (translator_prev_input != KEY_CAPSLOCK 
                        && translator_prev_input != KEY_LSHIFT 
                        && translator_prev_input != KEY_RSHIFT) {
                        cur_output = ScancodeWithShiftNoCapsLock[translator_prev_input];
                        translator_prev_state = WithShiftNoCapsLock;
                    }
                    else {
                        cur_output = 0x0000;
                        translator_prev_state = WithShiftNoCapsLock;
                    }
                }
            }
            else if (cur_input != KEY_LSHIFT 
                && cur_input != KEY_RSHIFT 
                && cur_input != KEY_CAPSLOCK) {
                if (translator_prev_input == KEY_CAPSLOCK) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftNoCapsLock;
                }
                else if (translator_prev_input == KEY_LSHIFT 
                    || translator_prev_input == KEY_RSHIFT) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftNoCapsLock;
                }
                else {
                    if (translator_prev_input != KEY_CAPSLOCK 
                        && translator_prev_input != KEY_LSHIFT 
                        && translator_prev_input != KEY_RSHIFT) {
                        cur_output = ScancodeWithShiftNoCapsLock[translator_prev_input];
                        translator_prev_state = WithShiftNoCapsLock;
                    }
                    else {
                        cur_output = 0x0000;
                        translator_prev_state = WithShiftNoCapsLock;
                    }
                }
            }
            else {
                if (translator_prev_input != KEY_CAPSLOCK 
                    && translator_prev_input != KEY_LSHIFT 
                    && translator_prev_input != KEY_RSHIFT) {
                    cur_output = ScancodeWithShiftNoCapsLock[translator_prev_input];
                    translator_prev_state = WithShiftNoCapsLock;
                }
                else {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftNoCapsLock;
                }
            }
            translator_prev_input = cur_input;
            break;
        }
        case NoShiftWithCapsLock: {
            if (cur_input & KB_KEY_RELEASE) {
                if (translator_prev_input == KB_KEY_RELEASE) {
                    cur_output = 0x0000;
                    translator_prev_state = NoShiftWithCapsLock;
                }
                else if (translator_prev_input == KEY_CAPSLOCK) {
                    cur_output = 0x0000;
                    translator_prev_state = NoShiftWithCapsLock;
                }
                else if (translator_prev_input == KEY_CAPSLOCK) {
                    cur_output = 0x0000;
                    translator_prev_state = NoShiftWithCapsLock;
                }
                else if (translator_prev_input == KEY_LSHIFT 
                    && translator_prev_input == KEY_RSHIFT) {
                    cur_output = 0x0000;
                    translator_prev_state = NoShiftWithCapsLock;
                }
                else {
		  if (!(translator_prev_input & KB_KEY_RELEASE)
                        && translator_prev_input != KEY_CAPSLOCK 
                        && translator_prev_input != KEY_LSHIFT 
                        && translator_prev_input != KEY_RSHIFT) {
                        cur_output = ScancodeNoShiftWithCapsLock[translator_prev_input];
                        translator_prev_state = NoShiftWithCapsLock;
                    }
                    else {
                        cur_output = 0x0000;
                        translator_prev_state = NoShiftWithCapsLock;
                    }
                }
            }
            else if (cur_input == KEY_CAPSLOCK) {
                if (translator_prev_input & KB_KEY_RELEASE) {
                    cur_output = 0x0000;
                    translator_prev_state = DEFAULT;
                }
                else if (translator_prev_input == KEY_CAPSLOCK) {
                    cur_output = 0x0000;
                    translator_prev_state = DEFAULT;
                }
                else if (translator_prev_input == KEY_LSHIFT 
                    && translator_prev_input == KEY_RSHIFT) {
                    cur_output = 0x0000;
                    translator_prev_state = DEFAULT;
                }
                else {
		  if (!(translator_prev_input & KB_KEY_RELEASE)
                        && translator_prev_input != KEY_CAPSLOCK 
                        && translator_prev_input != KEY_LSHIFT 
                        && translator_prev_input != KEY_RSHIFT) {
                        cur_output = ScancodeNoShiftWithCapsLock[translator_prev_input];
                        translator_prev_state = DEFAULT;
                    }
                    else {
                        cur_output = 0x0000;
                        translator_prev_state = DEFAULT;
                    }
                }
            }
            else if (cur_input == KEY_LSHIFT || cur_input == KEY_RSHIFT) {
                if (translator_prev_input & KB_KEY_RELEASE) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftWithCapsLock;
                }
                else if (translator_prev_input == KEY_CAPSLOCK) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftWithCapsLock;
                }
                else if (translator_prev_input == KEY_LSHIFT 
                    && translator_prev_input == KEY_RSHIFT) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftWithCapsLock;
                }
                else {
		  if (!(translator_prev_input & KB_KEY_RELEASE)
                        && translator_prev_input != KEY_CAPSLOCK 
                        && translator_prev_input != KEY_LSHIFT 
                        && translator_prev_input != KEY_RSHIFT) {
                        cur_output = ScancodeWithShiftWithCapsLock[translator_prev_input];
                        translator_prev_state = WithShiftWithCapsLock;
                    }
                    else {
                        cur_output = 0x0000;
                        translator_prev_state = WithShiftWithCapsLock;
                    }
                }
            }
            else {
                if (translator_prev_input & KB_KEY_RELEASE 
                    && translator_prev_input != KEY_CAPSLOCK 
                    && translator_prev_input != KEY_LSHIFT 
                    && translator_prev_input != KEY_RSHIFT) {
                    cur_output = ScancodeNoShiftWithCapsLock[translator_prev_input];
                    translator_prev_state = NoShiftWithCapsLock;
                }
                else {
                    cur_output = 0x0000;
                    translator_prev_state = NoShiftWithCapsLock;
                }
            }
            translator_prev_input = cur_input;
            break;
        }
        case WithShiftWithCapsLock: {
            if (cur_input & KB_KEY_RELEASE) {
                if (translator_prev_input & KB_KEY_RELEASE) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftWithCapsLock;
                }
                else if (translator_prev_input == KEY_CAPSLOCK) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftWithCapsLock;
                }
                else if (translator_prev_input == KEY_LSHIFT 
                    || translator_prev_input == KEY_RSHIFT) {
                    cur_output = 0x0000;
                    translator_prev_state = NoShiftWithCapsLock;
                }
                else {
                    if (translator_prev_input & KB_KEY_RELEASE 
                        && translator_prev_input != KEY_CAPSLOCK 
                        && translator_prev_input != KEY_LSHIFT 
                        && translator_prev_input != KEY_RSHIFT) {
                        cur_output = ScancodeWithShiftWithCapsLock[translator_prev_input];
                        translator_prev_state = WithShiftWithCapsLock;
                    }
                    else {
                        cur_output = 0x0000;
                        translator_prev_state = WithShiftWithCapsLock;
                    }
                }
            }
            else if (cur_input == KEY_CAPSLOCK) {
                if (translator_prev_input & KB_KEY_RELEASE) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftNoCapsLock;
                }
                else if (translator_prev_input == KEY_CAPSLOCK) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftNoCapsLock;
                }
                else if (translator_prev_input == KEY_LSHIFT 
                    || translator_prev_input == KEY_RSHIFT) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftWithCapsLock;
                }
                else {
		  if (!(translator_prev_input & KB_KEY_RELEASE )
                        && translator_prev_input != KEY_CAPSLOCK 
                        && translator_prev_input != KEY_LSHIFT 
                        && translator_prev_input != KEY_RSHIFT) {
                        cur_output = ScancodeWithShiftWithCapsLock[translator_prev_input];
                        translator_prev_state = WithShiftNoCapsLock;
                    }
                    else {
                        cur_output = 0x0000;
                        translator_prev_state = WithShiftNoCapsLock;
                    }
                }
            }
            else if (cur_input == KEY_LSHIFT || cur_input == KEY_RSHIFT) {
                if (translator_prev_input & KB_KEY_RELEASE) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftWithCapsLock;
                }
                else if (translator_prev_input == KEY_CAPSLOCK) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftWithCapsLock;
                }
                else if (translator_prev_input == KEY_LSHIFT 
                    || translator_prev_input == KEY_RSHIFT) {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftWithCapsLock;
                }
                else {
		  if (!(translator_prev_input & KB_KEY_RELEASE )
                        && translator_prev_input != KEY_CAPSLOCK 
                        && translator_prev_input != KEY_LSHIFT 
                        && translator_prev_input != KEY_RSHIFT) {
                        cur_output = ScancodeWithShiftWithCapsLock[translator_prev_input];
                        translator_prev_state = WithShiftWithCapsLock;
                    }
                    else {
                        cur_output = 0x0000;
                        translator_prev_state = WithShiftWithCapsLock;
                    }
                }
            }
            else {
	      if (!(translator_prev_input & KB_KEY_RELEASE )
                    && translator_prev_input != KEY_CAPSLOCK 
                    && translator_prev_input != KEY_LSHIFT 
                    && translator_prev_input != KEY_RSHIFT) {
                    cur_output = ScancodeWithShiftWithCapsLock[translator_prev_input];
                    translator_prev_state = WithShiftWithCapsLock;
                }
                else {
                    cur_output = 0x0000;
                    translator_prev_state = WithShiftWithCapsLock;
                }
            }
            translator_prev_input = cur_input;
            break;
        }
    }
    return cur_output;
}


int switcher(uint8_t scan) {
    //Switch Console State Machine
    if (g_switch_state == 0) {
        if(scan == 0x38) {
            g_switch_state = 1;
            switcher_scancode[0] = scan;
        } else if(scan == 0xE0) {
            g_switch_state = 5;
            switcher_scancode[0] = scan;
        } else {
            g_switch_state = 0;
            nk_vc_handle_input(scan);
        }
    } else if(g_switch_state == 1) {
        if(scan == 0x3B || scan == 0x3C) {
            g_switch_state = 2;
            switcher_scancode[1] = scan;
        } else {
            g_switch_state = 0;
            nk_vc_handle_input(switcher_scancode[0]);
            nk_vc_handle_input(scan);
        }
    } else if(g_switch_state == 2) {
        if(scan == 0xB8) {
            g_switch_state = 3;
            switcher_scancode[2] = scan;
        } else if((scan == 0xBB && (switcher_scancode[1] == 0x3B)) || (scan == 0xBC && (switcher_scancode[1] == 0x3C))) {
            g_switch_state = 4;
            switcher_scancode[2] = scan;
        } else {
            g_switch_state = 0;
            nk_vc_handle_input(switcher_scancode[0]);
            nk_vc_handle_input(switcher_scancode[1]);
            nk_vc_handle_input(scan);
          }
    } else if(g_switch_state == 3) {
        if(scan == 0xBB && (switcher_scancode[1] == 0x3B)) {
            nk_switch_to_prev_vc();
            g_switch_state = 0;
        } else if (scan == 0xBC && (switcher_scancode[1] == 0x3C)) {
            nk_switch_to_next_vc();
            g_switch_state = 0;
        } else {
            g_switch_state = 0;
            nk_vc_handle_input(switcher_scancode[0]);
            nk_vc_handle_input(switcher_scancode[1]);
            nk_vc_handle_input(switcher_scancode[2]);
            nk_vc_handle_input(scan);
        }
    } else if(g_switch_state == 4) {
        if(scan == 0xB8) {
            if(switcher_scancode[1] == 0x3B) {
                nk_switch_to_prev_vc();
                g_switch_state = 0;
            } else if (switcher_scancode[1] == 0x3C) {
                nk_switch_to_next_vc();
                g_switch_state = 0;
            }
        } else {
            g_switch_state = 0;
            nk_vc_handle_input(switcher_scancode[0]);
            nk_vc_handle_input(switcher_scancode[1]);
            nk_vc_handle_input(switcher_scancode[2]);
            nk_vc_handle_input(scan);
        }
    } else if(g_switch_state == 5) {
        if(scan == 0x38) {
            g_switch_state = 6;
            switcher_scancode[1] = scan;
        } else {
            g_switch_state = 0;
            nk_vc_handle_input(switcher_scancode[0]);
            nk_vc_handle_input(scan);
        }
    } else if(g_switch_state == 6) {
        if(scan == 0x3B || scan == 0x3C) {
            g_switch_state = 7;
            switcher_scancode[2] = scan;
        } else {
            g_switch_state = 0;
            nk_vc_handle_input(switcher_scancode[0]);
            nk_vc_handle_input(switcher_scancode[1]);
            nk_vc_handle_input(scan);
        }
    } else if(g_switch_state == 7) {
        if(scan == 0xE0) {
            g_switch_state = 8;
            switcher_scancode[3] = scan;
        } else if((scan == 0xBB && (switcher_scancode[2] == 0x3B)) || (scan == 0xBC && (switcher_scancode[2] == 0x3C))) {
            g_switch_state = 10;
            switcher_scancode[3] = scan;
        } else {
            g_switch_state = 0;
            nk_vc_handle_input(switcher_scancode[0]);
            nk_vc_handle_input(switcher_scancode[1]);
            nk_vc_handle_input(switcher_scancode[2]);
            nk_vc_handle_input(scan);
        }
    } else if(g_switch_state == 8) {
        if(scan == 0xB8) {
            g_switch_state = 9;
            switcher_scancode[4] = scan;
        } else {
            g_switch_state = 0;
            nk_vc_handle_input(switcher_scancode[0]);
            nk_vc_handle_input(switcher_scancode[1]);
            nk_vc_handle_input(switcher_scancode[2]);
            nk_vc_handle_input(switcher_scancode[3]);
            nk_vc_handle_input(scan);
        }
    } else if(g_switch_state == 9) {
        if(scan == 0xBB && (switcher_scancode[2] == 0x3B)) {
            nk_switch_to_prev_vc();
            g_switch_state = 0;
        } else if (scan == 0xBC && (switcher_scancode[2] == 0x3C)) {
            nk_switch_to_next_vc();
            g_switch_state = 0;
        } else {
            g_switch_state = 0;
            nk_vc_handle_input(switcher_scancode[0]);
            nk_vc_handle_input(switcher_scancode[1]);
            nk_vc_handle_input(switcher_scancode[2]);
            nk_vc_handle_input(switcher_scancode[3]);
            nk_vc_handle_input(switcher_scancode[4]);
            nk_vc_handle_input(scan);
        }
    } else if(g_switch_state == 10) {
        if(scan == 0xE0) {
            g_switch_state = 11;
            switcher_scancode[4] = scan;
        } else {
            g_switch_state = 0;
            nk_vc_handle_input(switcher_scancode[0]);
            nk_vc_handle_input(switcher_scancode[1]);
            nk_vc_handle_input(switcher_scancode[2]);
            nk_vc_handle_input(switcher_scancode[3]);
            nk_vc_handle_input(scan);
        }
    } else if(g_switch_state == 11) {
        if(scan == 0xB8) {
            if(switcher_scancode[2] == 0x3B) {
                nk_switch_to_prev_vc();
                g_switch_state = 0;
            } else if (switcher_scancode[2] == 0x3C) {
                nk_switch_to_next_vc();
                g_switch_state = 0;
            }
        } else {
            g_switch_state = 0;
            nk_vc_handle_input(switcher_scancode[0]);
            nk_vc_handle_input(switcher_scancode[1]);
            nk_vc_handle_input(switcher_scancode[2]);
            nk_vc_handle_input(switcher_scancode[3]);
            nk_vc_handle_input(switcher_scancode[4]);
            nk_vc_handle_input(scan);
        }
    }
    return 0; 
}



static int 
kbd_handler (excp_entry_t * excp, excp_vec_t vec)
{

    uint8_t status;
    uint8_t scan;
    uint8_t key;
    uint8_t flag;

    status = inb(KBD_CMD_REG);

    io_delay();

    DEBUG_PRINT("Keyboard: status=0x%x\n",status);

    if ((status & STATUS_OUTPUT_FULL) != 0) {
      scan  = inb(KBD_DATA_REG);
      DEBUG_PRINT("Keyboard: status=0x%x, scancode=0x%x\n", status, scan);
      io_delay();

#ifndef NAUT_CONFIG_VIRTUAL_CONSOLE
//What's key release here??
#if NAUT_CONFIG_THREAD_EXIT_KEYCODE == 0xc4
      if (!(scan & KBD_RELEASE)) {
          goto out;
      }
      /* this is a key release */
      if (scan == 0xc4) {
          void * ret = NULL;
          IRQ_HANDLER_END();
          nk_thread_exit(ret);
      }
#endif
#else

    switcher(scan);

#endif
      
    }


out:
    IRQ_HANDLER_END();
    return 0;
}


/*
int kbd_switcher_clear() {
    g_switch_state = 0;
    memset(switcher_scancode, 0, sizeof(uint8_t) * 5);
    return 0;
}
*/

int
kbd_init (struct naut_info * naut)
{
    printk("Initialize keyboard\n");
    g_switch_state = 0;
    memset(switcher_scancode, 0, sizeof(uint8_t) * 5);
    translator_prev_state = 0;
    translator_prev_input = 0;
    cur_output = 0x0000;
    register_irq_handler(1, kbd_handler, NULL);
    nk_unmask_irq(1);
    return 0;
}






