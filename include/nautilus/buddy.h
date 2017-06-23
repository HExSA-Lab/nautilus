/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
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

void buddy_free(struct buddy_mempool * mp, void * addr, ulong_t order);
void * buddy_alloc(struct buddy_mempool * mp, ulong_t order);

int  buddy_sanity_check(struct buddy_mempool *mp);

struct buddy_pool_stats {
    void   *start_addr;
    void   *end_addr;
    uint64_t total_blocks_free;
    uint64_t total_bytes_free;
    uint64_t min_alloc_size;
    uint64_t max_alloc_size;
};

void buddy_stats(struct buddy_mempool *mp, struct buddy_pool_stats *stats);


#endif
