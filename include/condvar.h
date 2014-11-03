#ifndef __CONDVAR_H__
#define __CONDVAR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <thread.h>

typedef struct condvar {
    spinlock_t lock;
    thread_queue_t * wait_queue;
    unsigned nwaiters;
} condvar_t;

int condvar_init(condvar_t * c);
int condvar_destroy(condvar_t * c);
uint8_t condvar_wait(condvar_t * c, spinlock_t * l, uint8_t flags);
int condvar_signal(condvar_t * c);
int condvar_bcast(condvar_t * c);
void condvar_test(void);

#ifdef __cplusplus
}
#endif

#endif
