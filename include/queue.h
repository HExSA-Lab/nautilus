#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <spinlock.h>
#include <list.h>

struct queue {
    struct list_head queue;
    spinlock_t lock;
};

struct queue_entry {
    struct list_head node;
};

typedef struct queue queue_t;
typedef struct queue_entry queue_entry_t;

queue_t* queue_create(void);
void queue_destroy(queue_t * q, uint8_t free_entries);

void enqueue_entry(queue_t * q, queue_entry_t * entry);
void enqueue_entry_atomic(queue_t * q, queue_entry_t * entry);
queue_entry_t* dequeue_entry(queue_entry_t * entry);
queue_entry_t* dequeue_entry_atomic(queue_t * q, queue_entry_t * entry);
queue_entry_t* dequeue_first(queue_t * q);
queue_entry_t* dequeue_first_atomic(queue_t * q);

uint8_t queue_empty(queue_t * q);
uint8_t queue_empty_atomic(queue_t * q);





#endif
