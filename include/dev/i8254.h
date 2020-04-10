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
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __I8254_H__
#define __I8254_H__

#ifdef __cplusplus
extern "C" {
#endif

struct naut_info;

ulong_t i8254_calib_tsc(void);
int i8254_init(struct naut_info * naut);

int i8254_set_oneshot(uint64_t time_ns);
    
#define PIT_TIMER_IRQ 0

/* for configuring the PC speaker ports */
#define KB_CTRL_DATA_OUT 0x60
#define KB_CTRL_PORT_B   0x61

#define PIT_CHAN0_DATA 0x40
#define PIT_CHAN1_DATA 0x41
#define PIT_CHAN2_DATA 0x42
#define PIT_CMD_REG    0x43

/* for cmd reg */

#define PIT_CHAN_SEL_0 0x0
#define PIT_CHAN_SEL_1 0x1
#define PIT_CHAN_SEL_2 0x2

#define PIT_CHAN(x) ((x) << 6)

#define PIT_ACC_MODE_LATCH_CNT 0x0
#define PIT_ACC_MODE_JUST_LO   0x1
#define PIT_ACC_MODE_JUST_HI   0x2
#define PIT_ACC_MODE_BOTH      0x3

#define PIT_ACC_MODE(x) ((x) << 4)

#define PIT_MODE_TERMINAL_COUNT  0x0
#define PIT_MODE_ONESHOT  0x1
#define PIT_MODE_RATEGEN  0x2
#define PIT_MODE_SQUARE   0x3
#define PIT_MODE_SWSTROBE 0x4
#define PIT_MODE_HWSTROBE 0x5
#define PIT_MODE_RATEGEN2 0x6
#define PIT_MODE_SQUARE2  0x7

#define PIT_MODE(x) ((x) << 1)

#define PIT_BIT_MODE_BIN 0
#define PIT_BIT_MODE_BCD 1 /* 4-digit BCD */

#define PIT_RATE 1193182UL

#define MS      10
#define LATCH   (PIT_RATE / (1000 / MS))
#define LOOPS   1000

#ifdef __cplusplus
}
#endif

#endif
