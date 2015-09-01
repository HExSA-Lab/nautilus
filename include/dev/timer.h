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
#ifndef __TIMER_H__
#define __TIMER_H__

#define NUM_TIMERS 1

#define TEVENT_WAITING 0
#define TEVENT_READY   1

struct naut_info;

struct nk_timer_event {
    uint8_t active;
    uint32_t ticks;
    volatile uint8_t event_flag;
};

#include <nautilus/idt.h>
int nk_timer_handler(excp_entry_t *, excp_vec_t);
int nk_timer_init (struct naut_info * naut);
void nk_sleep (uint_t msec);

#endif
