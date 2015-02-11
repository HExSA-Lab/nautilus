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

nk_queue_t* nk_queue_create(void);
void nk_queue_destroy(nk_queue_t * q, uint8_t free_entries);

nk_queue_entry_t* nk_dequeue_entry(nk_queue_entry_t * entry);
nk_queue_entry_t* nk_dequeue_entry_atomic(nk_queue_t * q, nk_queue_entry_t * entry);
nk_queue_entry_t* nk_dequeue_first(nk_queue_t * q);
nk_queue_entry_t* nk_dequeue_first_atomic(nk_queue_t * q);

uint8_t nk_queue_empty_atomic(nk_queue_t * q);

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


static void 
nk_enqueue_entry_atomic (nk_queue_t * q, nk_queue_entry_t * entry)
{
    uint8_t flags = spin_lock_irq_save(&(q->lock));
    list_add_tail(&(entry->node), &(q->queue));
    spin_unlock_irq_restore(&(q->lock), flags);
}


#ifdef __cplusplus
}
#endif


#endif
