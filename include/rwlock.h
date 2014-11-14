#ifndef __RWLOCK_H__
#define __RWLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <spinlock.h>

struct nk_rwlock {
    spinlock_t lock;
    unsigned readers;
};

typedef struct nk_rwlock nk_rwlock_t;

int nk_rwlock_init(nk_rwlock_t * l);
int nk_rwlock_rd_lock(nk_rwlock_t * l);
int nk_rwlock_wr_lock(nk_rwlock_t * l);
int nk_rwlock_rd_unlock(nk_rwlock_t * l);
int nk_rwlock_wr_unlock(nk_rwlock_t * l);
uint8_t nk_rwlock_wr_lock_irq_save(nk_rwlock_t * l);
int nk_rwlock_wr_unlock_irq_restore(nk_rwlock_t * l, uint8_t flags);

void nk_rwlock_test(void);

#ifdef __cplusplus
}
#endif

#endif
