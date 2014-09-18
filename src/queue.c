#include <queue.h>
#include <spinlock.h>

#include <lib/liballoc.h>


queue_t*
queue_create (void)
{
    queue_t * q = NULL;
    q = malloc(sizeof(queue_t));
    if (!q) {
        return NULL;
    }
    memset(q, 0, sizeof(queue_t));
     
    INIT_LIST_HEAD(&(q->queue));

    spinlock_init(&(q->lock));

    return q;
}


void
queue_destroy (queue_t * q, uint8_t free_entries)
{
    queue_entry_t * tmp = NULL;
    queue_entry_t * elm   = NULL;
    
    list_for_each_entry_safe(elm, tmp, &(q->queue), node) {
        list_del(&(elm->node));
        if (free_entries) {
            free(elm);
        }
    }

    free(q);
}


void
enqueue_entry (queue_t * q, queue_entry_t * entry)
{
    list_add_tail(&(entry->node), &(q->queue));

}


void 
enqueue_entry_atomic (queue_t * q, queue_entry_t * entry)
{
    spin_lock(&(q->lock));
    list_add_tail(&(entry->node), &(q->queue));
    spin_unlock(&(q->lock));
}


queue_entry_t*
dequeue_entry (queue_entry_t * entry)
{
    list_del(&(entry->node));
    return entry;
}


queue_entry_t* 
dequeue_entry_atomic (queue_t * q, queue_entry_t * entry)
{
    queue_entry_t * ret = NULL;
    spin_lock(&(q->lock));
    if (!list_empty_careful(&(q->queue))) {
        ret = entry;
        list_del(&(entry->node));
    } 
    spin_unlock(&(q->lock));
    return ret;
}


queue_entry_t* 
dequeue_first (queue_t * q)
{
    queue_entry_t * ret = NULL;
    if (!list_empty_careful(&(q->queue))) {
        struct list_head * first = q->queue.next;
        ret = list_entry(first, queue_entry_t, node);
        list_del(&(ret->node));
    }
    return ret;
}


queue_entry_t* 
dequeue_first_atomic (queue_t * q)
{
    spin_lock(&(q->lock));
    queue_entry_t * ret = NULL;
    if (!list_empty_careful(&(q->queue))) {
        struct list_head * first = q->queue.next;
        ret = list_entry(first, queue_entry_t, node);
        list_del(&(ret->node));
    }
    spin_unlock(&(q->lock));
    return ret;
}


uint8_t 
queue_empty (queue_t * q) 
{
    return list_empty(&(q->queue));
}


uint8_t 
queue_empty_atomic (queue_t * q)
{
    uint8_t ret = 1;
    spin_lock(&(q->lock));
    ret = list_empty_careful(&(q->queue));
    spin_unlock(&(q->lock));
    return ret;
}



