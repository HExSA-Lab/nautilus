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
#include <nautilus/spinlock.h>
#include <nautilus/irq.h>

void 
spinlock_init (volatile spinlock_t * lock) 
{
    *lock = 0;
}


void
spinlock_deinit (volatile spinlock_t * lock) 
{
    *lock = 0;
}

void
spin_lock_nopause (volatile spinlock_t * lock)
{
    while (__sync_lock_test_and_set(lock, 1)) {
        /* nothing */
    }
}

uint8_t
spin_lock_irq_save_nopause (volatile spinlock_t * lock)
{
    uint8_t flags = irq_disable_save();
    while (__sync_lock_test_and_set(lock, 1)) {
        /* nothing */
    }
    return flags;
}
