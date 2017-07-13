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
 * Copyright (c) 2017, Matt George <11georgem@gmail.com>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Matt George <11georgem@gmail.com>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

// This is a port of the Boehm garbage collector to
// the Nautilus kernel

#ifndef __BDWGC__
#define __BDWGC__

int  nk_gc_bdwgc_init();
void nk_gc_bdwgc_deinit();

// force a GC -r returns 0 if successful
int  nk_gc_bdwgc_collect();

void *nk_gc_bdwgc_thread_state_init(struct nk_thread *thread);
void  nk_gc_bdwgc_thread_state_deinit(struct nk_thread *thread);

#ifdef NAUT_CONFIG_TEST_BDWGC
int  nk_gc_bdwgc_test();
#endif

#endif
