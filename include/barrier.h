#ifndef __BARRIER_H__
#define __BARRIER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <spinlock.h>

#define BARRIER_LAST 1

typedef struct nk_barrier nk_barrier_t;

struct nk_barrier {
    spinlock_t lock; /* SLOW */
    
    unsigned remaining;
    unsigned init_count;

    uint8_t pad[52];

    /* this is on another cache line */
    volatile unsigned notify;
} __attribute__ ((packed));

int nk_barrier_init (nk_barrier_t * barrier, uint32_t count);
int nk_barrier_destroy (nk_barrier_t * barrier);
int nk_barrier_wait (nk_barrier_t * barrier);
void nk_barrier_test(void);

#ifdef __cplusplus
}
#endif

#endif /* !__BARRIER_H__ */
