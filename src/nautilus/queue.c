#include <nautilus/queue.h>
#include <nautilus/spinlock.h>

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
    list_add_tail(&(entry->node), &(q->queue));
}


void 
nk_enqueue_entry_atomic (nk_queue_t * q, nk_queue_entry_t * entry)
{
    uint8_t flags = spin_lock_irq_save(&(q->lock));
    list_add_tail(&(entry->node), &(q->queue));
    spin_unlock_irq_restore(&(q->lock), flags);
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
    if (!list_empty_careful(&(q->queue))) {
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
    if (!list_empty_careful(&(q->queue))) {
        struct list_head * first = q->queue.next;
        ret = list_entry(first, nk_queue_entry_t, node);
        list_del(&(ret->node));
    }
    return ret;
}


nk_queue_entry_t* 
nk_dequeue_first_atomic (nk_queue_t * q)
{
    uint8_t flags = spin_lock_irq_save(&(q->lock));
    nk_queue_entry_t * ret = NULL;
    if (!list_empty_careful(&(q->queue))) {
        struct list_head * first = q->queue.next;
        ret = list_entry(first, nk_queue_entry_t, node);
        list_del(&(ret->node));
    }
    spin_unlock_irq_restore(&(q->lock), flags);
    return ret;
}


uint8_t 
nk_queue_empty (nk_queue_t * q) 
{
    return list_empty(&(q->queue));
}


uint8_t 
nk_queue_empty_atomic (nk_queue_t * q)
{
    uint8_t ret = 1;
    uint8_t flags = spin_lock_irq_save(&(q->lock));
    ret = list_empty_careful(&(q->queue));
    spin_unlock_irq_restore(&(q->lock), flags);
    return ret;
}
