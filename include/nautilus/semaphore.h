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
 * Copyright (c) 2018, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__


struct nk_semaphore *nk_semaphore_create(int init_count);
void nk_semaphore_destroy(struct nk_semaphore *s);

void nk_semaphore_up(struct nk_semaphore *s);
void nk_semaphore_down(struct nk_semaphore *s);
// 0 return indicates success
int  nk_semaphore_try_down(struct nk_semaphore *s);
// 0 return indicates have semaphore, nonzero => timeout
int nk_semaphore_down_timeout(struct nk_semaphore *s, uint64_t timeout_ns);

#endif
