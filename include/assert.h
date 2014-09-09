#ifndef __ASSERT_H__
#define __ASSERT_H__

#include <nautilus.h>
#include <thread.h>

extern thread_t * cur_thread;

#define ASSERT(cond)                   \
do {                            \
    if (!(cond)) {                  \
    printk("Failed assertion in %s: %s at %s, line %d, RA=%lx, thread=%p\n",\
        __func__, #cond, __FILE__, __LINE__,    \
        (ulong_t) __builtin_return_address(0),  \
        cur_thread);           \
    while (1)                   \
       ;                        \
    }                           \
} while (0)

#endif
