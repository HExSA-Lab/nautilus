#include <nautilus/queue.h>
#include <nautilus/spinlock.h>
#include <nautilus/naut_assert.h>

#include <lib/liballoc.h>


nk_queue_t*
nk_queue_create (void)
{
    nk_queue_t * q = NULL;
    q = malloc(sizeof(nk_queue_t));
    if (!q) {
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


void
nk_enqueue_entry (nk_queue_t * q, nk_queue_entry_t * entry)
{
    ASSERT(q && entry);

    list_add_tail(&(entry->node), &(q->queue));
    q->size++;
}


void 
nk_enqueue_entry_atomic (nk_queue_t * q, nk_queue_entry_t * entry)
{
    uint8_t flags; 

    ASSERT(q && entry);

    flags = spin_lock_irq_save(&(q->lock));
    list_add_tail(&(entry->node), &(q->queue));
    q->size++;
    spin_unlock_irq_restore(&(q->lock), flags);
}


/* DEPRECATED */
nk_queue_entry_t*
nk_dequeue_entry (nk_queue_entry_t * entry)
{
    ASSERT(0);
    //list_del(&(entry->node));
    //q->size--;
    //return entry;
    return NULL;
}


nk_queue_entry_t* 
nk_dequeue_entry_atomic (nk_queue_t * q, nk_queue_entry_t * entry)
{
    nk_queue_entry_t * ret = NULL;

    ASSERT(q && entry);

    uint8_t flags = spin_lock_irq_save(&(q->lock));
    if (!list_empty_careful(&(q->queue))) {
        ret = entry;
        list_del(&(entry->node));
        q->size--;
    } 
    spin_unlock_irq_restore(&(q->lock), flags);
    return ret;
}


nk_queue_entry_t* 
nk_dequeue_first (nk_queue_t * q)
{
    nk_queue_entry_t * ret = NULL;

    ASSERT(q);

    if (!list_empty_careful(&(q->queue))) {
        struct list_head * first = q->queue.next;
        ret = list_entry(first, nk_queue_entry_t, node);
        list_del(&(ret->node));
        q->size--;
    }
    return ret;
}


nk_queue_entry_t* 
nk_dequeue_first_atomic (nk_queue_t * q)
{
    nk_queue_entry_t * ret = NULL;
    uint8_t flags;

    ASSERT(q);

    flags = spin_lock_irq_save(&(q->lock));
    if (!list_empty_careful(&(q->queue))) {
        struct list_head * first = q->queue.next;
        ret = list_entry(first, nk_queue_entry_t, node);
        list_del(&(ret->node));
        q->size--;
    }
    spin_unlock_irq_restore(&(q->lock), flags);
    return ret;
}


uint8_t 
nk_queue_empty (nk_queue_t * q) 
{
    ASSERT(q);
    //return list_empty(&(q->queue));
    return !q->size;
}


uint8_t 
nk_queue_empty_atomic (nk_queue_t * q)
{
    uint8_t ret = 1;
    uint8_t flags; 

    ASSERT(q);

    flags = spin_lock_irq_save(&(q->lock));
    //ret = list_empty_careful(&(q->queue));
    ret = !q->size;
    spin_unlock_irq_restore(&(q->lock), flags);
    return ret;
}

unsigned 
nk_queue_size (nk_queue_t * q) 
{
    ASSERT(q);
    return q->size;
}
