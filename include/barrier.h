#ifndef __BARRIER_H__
#define __BARRIER_H__

#include <spinlock.h>
#include <event.h>

#define BARRIER_LAST 1

typedef struct barrier barrier_t;

struct barrier {
    spinlock_t lock; /* SLOW */
    
    unsigned remaining;
    unsigned init_count;

    uint8_t pad[52];

    /* this is on another cache line */
    volatile unsigned notify;
} __attribute__ ((packed));

int barrier_init (barrier_t * barrier, uint32_t count);
int barrier_destroy (barrier_t * barrier);
int barrier_wait (barrier_t * barrier);
void barrier_test(void);

#endif /* !__BARRIER_H__ */
