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
#include <nautilus/queue.h>
#include <nautilus/spinlock.h>
#include <nautilus/intrinsics.h>
#include <nautilus/mm.h>



nk_queue_t*
nk_queue_create (void)
{
    nk_queue_t * q = NULL;
    q = malloc(sizeof(nk_queue_t));
    if (unlikely(!q)) {
        return NULL;
    }
    memset(q, 0, sizeof(nk_queue_t));
     
    INIT_LIST_HEAD(&(q->queue));

    spinlock_init(&(q->lock));

    return q;
}


void
nk_queue_destroy (nk_queue_t * q, uint8_t free_entries)
{
    nk_queue_entry_t * tmp = NULL;
    nk_queue_entry_t * elm   = NULL;
    
    list_for_each_entry_safe(elm, tmp, &(q->queue), node) {
        list_del_init(&(elm->node));
        if (free_entries) {
            free(elm);
        }
    }

    free(q);
}




