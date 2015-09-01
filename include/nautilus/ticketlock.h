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
#ifndef __TICKET_LOCK_H__
#define __TICKET_LOCK_H__


#ifdef __cplusplus
extern "C" {
#endif


union nk_ticket_lock {
    uint32_t val;

    struct {
        uint16_t ticket;
        uint16_t users;
    } lock;
};

typedef union nk_ticket_lock nk_ticket_lock_t;

void nk_ticket_lock_init(nk_ticket_lock_t * l);
void nk_ticket_lock_deinit(nk_ticket_lock_t * l);
void nk_ticket_lock(nk_ticket_lock_t * l);
void nk_ticket_unlock(nk_ticket_lock_t * l);
int nk_ticket_trylock(nk_ticket_lock_t * l);


#ifdef __cplusplus
}
#endif


#endif
