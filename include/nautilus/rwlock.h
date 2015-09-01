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
#ifndef __RWLOCK_H__
#define __RWLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/spinlock.h>

struct nk_rwlock {
    spinlock_t lock;
    unsigned readers;
};

typedef struct nk_rwlock nk_rwlock_t;

int nk_rwlock_init(nk_rwlock_t * l);
int nk_rwlock_rd_lock(nk_rwlock_t * l);
int nk_rwlock_wr_lock(nk_rwlock_t * l);
int nk_rwlock_rd_unlock(nk_rwlock_t * l);
int nk_rwlock_wr_unlock(nk_rwlock_t * l);
uint8_t nk_rwlock_wr_lock_irq_save(nk_rwlock_t * l);
int nk_rwlock_wr_unlock_irq_restore(nk_rwlock_t * l, uint8_t flags);

void nk_rwlock_test(void);

#ifdef __cplusplus
}
#endif

#endif
