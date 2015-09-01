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
#ifndef __RANDOM_H__
#define __RANDOM_H__

#ifdef __cplusplus
extern "C" {
#endif


#include <nautilus/spinlock.h>

struct nk_rand_info {
    spinlock_t lock;
    uint64_t seed;
    uint64_t xi; 
    uint64_t n;
};

struct cpu;
int nk_rand_init(struct cpu * cpu);
void nk_rand_set_xi(uint64_t xi);
void nk_rand_seed(uint64_t seed);
void nk_get_rand_bytes(uint8_t *buf, unsigned len);

#ifdef __cplusplus
}
#endif

#endif
