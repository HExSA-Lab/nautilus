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
#ifndef __CONDVAR_H__
#define __CONDVAR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/thread.h>
#include <nautilus/waitqueue.h>

typedef struct nk_condvar {
    NK_LOCK_T lock;
    nk_wait_queue_t * wait_queue;
    unsigned nwaiters;
    unsigned long long wakeup_seq;
    unsigned long long woken_seq;
    unsigned long long main_seq;
    unsigned bcast_seq;
} nk_condvar_t;

int nk_condvar_init(nk_condvar_t * c);
int nk_condvar_destroy(nk_condvar_t * c);
uint8_t nk_condvar_wait(nk_condvar_t * c, NK_LOCK_T * l);
int nk_condvar_signal(nk_condvar_t * c);
int nk_condvar_bcast(nk_condvar_t * c);
void nk_condvar_test(void);

#ifdef __cplusplus
}
#endif

#endif
