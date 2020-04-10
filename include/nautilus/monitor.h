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
#ifndef _MONITOR
#define _MONITOR

// entry points to monitor - return 0 on success

// intentional entry into monitor anywhere in code
int nk_monitor_entry(void);

// entry due to unhandled exception
int nk_monitor_excp_entry(excp_entry_t * excp,
			  excp_vec_t vector,
			  void *state);

// entry due to unhandled interrupt
int nk_monitor_irq_entry(excp_entry_t * excp,
			 excp_vec_t vector,
			 void *state);

// entry due to explicit panic call
int nk_monitor_panic_entry(char *str);

// entry due to hanging on some cpu (NMI from watchdog)
int nk_monitor_hang_entry(excp_entry_t * excp,
			  excp_vec_t vector,
			  void *state);

// entry for debug exceptions (debug register use within monitor)
int nk_monitor_debug_entry(excp_entry_t * excp,
			   excp_vec_t vector,
			   void *state);

// entry in response to another CPU's entry
// if we enter the monitor on any CPU for any reason,
// other CPUs must also enter it, using this function
// In order to get them to do so, we NMI them.
int nk_monitor_sync_entry (void);

// returns 1 if we are already in the monitor on any cpu
//  and cpu is written with the cpu that caused the entry
int nk_monitor_check(int *cpu);

// invoke on BSP at boot as early as possible and definitely
// before bringing up APs
void nk_monitor_init(void);

#endif

