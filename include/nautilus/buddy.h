#ifndef __BUDDY_H__
#define __BUDDY_H__

#include <nautilus/naut_types.h>
#include <nautilus/spinlock.h>

struct buddy_mempool {
    ulong_t    base_addr;    /** base address of the memory pool */
    ulong_t    pool_order;   /** size of memory pool = 2^pool_order */
    ulong_t    min_order;    /** minimum allocatable block size */

    ulong_t    num_blocks;   /** number of bits in tag_bits */
    ulong_t    *tag_bits;    /** one bit for each 2^min_order block
                                    *   0 = block is allocated
                                    *   1 = block is available
                                    */

    struct list_head *avail;       /** one free list for each block size,
                                    * indexed by block order:
                                    *   avail[i] = free list of 2^i blocks
                                    */

    spinlock_t lock;
};

struct buddy_mempool * buddy_init(ulong_t base_addr, ulong_t pool_order, ulong_t min_order);

void buddy_free(struct buddy_mempool * mp, const void * addr, ulong_t order);
void * buddy_alloc(struct buddy_mempool * mp, ulong_t order);

#endif
