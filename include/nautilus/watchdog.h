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
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, The Interweaving Project <http://interweaving.org>
 *                     The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __WATCHDOG
#define __WATCHDOG

// Pet the watchdog periodically when things are going OK
// returns nonzero if the watchdog is barking on any CPU
int nk_watchdog_pet(void);

// Always invoke on an NMI coming from the watchdog
// on this cpu
void nk_watchdog_nmi(void);

//  <0 => error
// ==0 => OK, watchdog is not barking
//  >0 => OK, watchdog is barking (time's up)
int  nk_watchdog_check_this_cpu(void);
int  nk_watchdog_check_any_cpu(void);


// reset watchdog after it has barked
// this will reset every CPU's view
// should be done just on CPU 0
void nk_watchdog_reset(void);

// timeout is how long between pets will be tolerated
int nk_watchdog_init(uint64_t bark_timeout_ns);
int nk_watchdog_deinit(void);


#endif

