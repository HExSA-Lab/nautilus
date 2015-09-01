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
#ifndef __KBD_H__
#define __KBD_H__

#define KBD_DATA_REG 0x60
#define KBD_ACK_REG  0x61
#define KBD_CMD_REG  0x64

#define KBD_LED_CODE 0xed
#define KBD_RELEASE  0x80

#define KEY_SPECIAL_FLAG 0x0100

#define SPECIAL_KEY(x) (KEY_SPECIAL_FLAG | (x))
#define KEY_CTRL_C   0x2e03

#define KEY_LCTRL    SPECIAL_KEY(13)
#define KEY_RCTRL    SPECIAL_KEY(14)
#define KEY_LSHIFT   SPECIAL_KEY(15)
#define KEY_RSHIFT   SPECIAL_KEY(16)

#define KBD_BIT_KDATA 0 /* keyboard data in buffer */
#define KBD_BIT_UDATA 1 /* user data in buffer */
#define KBD_RESET     0xfe

#define STATUS_OUTPUT_FULL 0x1

struct naut_info;

int kbd_init(struct naut_info * naut);


#endif
