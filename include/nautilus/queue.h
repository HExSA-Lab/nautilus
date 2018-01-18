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
#ifndef __QUEUE_H__
#define __QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/spinlock.h>
#include <nautilus/list.h>

struct nk_queue {
    struct list_head queue;
    spinlock_t lock;
};

struct nk_queue_entry {
    struct list_head node;
};

typedef struct nk_queue nk_queue_t;
typedef struct nk_queue_entry nk_queue_entry_t;

static inline void nk_queue_entry_init(nk_queue_entry_t *e) { INIT_LIST_HEAD(&e->node); }
static inline int nk_queue_entry_is_enqueued(nk_queue_entry_t *e) { return !list_empty(&e->node); }
    
nk_queue_t* nk_queue_create(void);
void nk_queue_destroy(nk_queue_t * q, uint8_t free_entries);

    
static inline uint8_t 
nk_queue_empty(nk_queue_t * q) 
{
    return list_empty(&(q->queue));
}

static inline void
nk_enqueue_entry (nk_queue_t * q, nk_queue_entry_t * entry)
{
    list_add_tail(&(entry->node), &(q->queue));
}


static inline void 
nk_enqueue_entry_atomic (nk_queue_t * q, nk_queue_entry_t * entry)
{
    uint8_t flags = spin_lock_irq_save(&(q->lock));
    list_add_tail(&(entry->node), &(q->queue));
    spin_unlock_irq_restore(&(q->lock), flags);
}


static inline nk_queue_entry_t*
nk_dequeue_entry (nk_queue_entry_t * entry)
{
    list_del_init(&(entry->node));
    return entry;
}


static inline nk_queue_entry_t* 
nk_dequeue_entry_atomic (nk_queue_t * q, nk_queue_entry_t * entry)
{
    nk_queue_entry_t * ret = NULL;
    uint8_t flags = spin_lock_irq_save(&(q->lock));
    if (!nk_queue_empty(q)) {
        ret = entry;
        list_del_init(&(entry->node));
    } 
    spin_unlock_irq_restore(&(q->lock), flags);
    return ret;
}


static inline nk_queue_entry_t* 
nk_dequeue_first (nk_queue_t * q)
{
    nk_queue_entry_t * ret = NULL;
    if (!nk_queue_empty(q)) {
        struct list_head * first = q->queue.next;
        ret = list_entry(first, nk_queue_entry_t, node);
        list_del_init(first);
    }
    return ret;
}


static inline nk_queue_entry_t* 
nk_dequeue_first_atomic (nk_queue_t * q)
{
    uint8_t flags = spin_lock_irq_save(&(q->lock));
    nk_queue_entry_t * ret = NULL;
    if (!nk_queue_empty(q)) {
        struct list_head * first = q->queue.next;
        ret = list_entry(first, nk_queue_entry_t, node);
        list_del_init(&(ret->node));
    }
    spin_unlock_irq_restore(&(q->lock), flags);
    return ret;
}




static inline uint8_t 
nk_queue_empty_atomic (nk_queue_t * q)
{
    uint8_t ret = 1;
    uint8_t flags = spin_lock_irq_save(&(q->lock));
    ret = list_empty(&(q->queue));
    spin_unlock_irq_restore(&(q->lock), flags);
    return ret;
}


#ifdef __cplusplus
}
#endif


#endif
