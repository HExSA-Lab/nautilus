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
        list_del(&(elm->node));
        if (free_entries) {
            free(elm);
        }
    }

    free(q);
}




nk_queue_entry_t*
nk_dequeue_entry (nk_queue_entry_t * entry)
{
    list_del(&(entry->node));
    return entry;
}


nk_queue_entry_t* 
nk_dequeue_entry_atomic (nk_queue_t * q, nk_queue_entry_t * entry)
{
    nk_queue_entry_t * ret = NULL;
    uint8_t flags = spin_lock_irq_save(&(q->lock));
    if (!nk_queue_empty(q)) {
        ret = entry;
        list_del(&(entry->node));
    } 
    spin_unlock_irq_restore(&(q->lock), flags);
    return ret;
}


nk_queue_entry_t* 
nk_dequeue_first (nk_queue_t * q)
{
    nk_queue_entry_t * ret = NULL;
    if (!nk_queue_empty(q)) {
        struct list_head * first = q->queue.next;
        ret = list_entry(first, nk_queue_entry_t, node);
        list_del(first);
    }
    return ret;
}


nk_queue_entry_t* 
nk_dequeue_first_atomic (nk_queue_t * q)
{
    uint8_t flags = spin_lock_irq_save(&(q->lock));
    nk_queue_entry_t * ret = NULL;
    if (!nk_queue_empty(q)) {
        struct list_head * first = q->queue.next;
        ret = list_entry(first, nk_queue_entry_t, node);
        list_del(&(ret->node));
    }
    spin_unlock_irq_restore(&(q->lock), flags);
    return ret;
}




uint8_t 
nk_queue_empty_atomic (nk_queue_t * q)
{
    uint8_t ret = 1;
    uint8_t flags = spin_lock_irq_save(&(q->lock));
    ret = list_empty(&(q->queue));
    spin_unlock_irq_restore(&(q->lock), flags);
    return ret;
}
