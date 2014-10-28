#ifndef __RWLOCK_H__
#define __RWLOCK_H__

#include <spinlock.h>

struct rwlock {
    spinlock_t lock;
    unsigned readers;
};

typedef struct rwlock rwlock_t;

int rwlock_init(rwlock_t * l);
int rwlock_rd_lock(rwlock_t * l);
int rwlock_wr_lock(rwlock_t * l);
int rwlock_rd_unlock(rwlock_t * l);
int rwlock_wr_unlock(rwlock_t * l);
uint8_t rwlock_wr_lock_irq_save(rwlock_t * l);
int rwlock_wr_unlock_irq_restore(rwlock_t * l, uint8_t flags);

void rwlock_test(void);


#endif
