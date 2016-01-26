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
 * http://xtack.sandia.gov/hobbes
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
 *
 * This file includes code borrowed from the Kitten LWK, with modifications
 */
#include <nautilus/nautilus.h>
#include <nautilus/mm.h>
#include <nautilus/paging.h>
#include <nautilus/buddy.h>
#include <nautilus/naut_types.h>
#include <nautilus/list.h>
#include <nautilus/naut_assert.h>
#include <nautilus/math.h>
#include <nautilus/macros.h>

#include <lib/bitmap.h>

#ifndef NAUT_CONFIG_DEBUG_BUDDY
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define BUDDY_DEBUG(fmt, args...) DEBUG_PRINT("BUDDY: " fmt, ##args)
#define BUDDY_PRINT(fmt, args...) printk("BUDDY: " fmt, ##args)
#define BUDDY_WARN(fmt, args...)  WARN_PRINT("BUDDY: " fmt, ##args)



/**
 * Each free block has one of these structures at its head. The link member
 * provides linkage for the mp->avail[order] free list, where order is the
 * size of the free block.
 */
struct block {
    struct list_head link;
    ulong_t    order;
};



#if 1
/**
 * __set_bit - Set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike set_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static inline void 
__set_bit (int nr, volatile void * addr)
{
    __asm__ __volatile__ (
        "btsl %1,%0"
        :"+m" (*(volatile long*)addr)
        :"r" (nr) : "memory");
}


static inline void 
__clear_bit (int nr, volatile void * addr)
{
    __asm__ __volatile__ (
        "btrl %1,%0"
        :"+m" (*(volatile long*)addr)
        :"r" (nr));
}
#else
static inline void 
__set_bit (ulong_t nr, volatile void * addr)
{
    
    __asm__ __volatile__ (
        "btsq %1,%0"
        :"+m" (*(volatile long*)addr)
        :"r" (nr) : "memory");
}


static inline void 
__clear_bit (ulong_t nr, volatile void * addr)
{
    __asm__ __volatile__ (
        "btrq %1,%0"
        :"+m" (*(volatile long*)addr)
        :"r" (nr));
}
#endif

static inline void setb(ulong_t nr, volatile char *addr)
{
    ulong_t offset, bitoffset;
    offset = nr/8;
    bitoffset = nr % 8;

    addr[offset] |= (0x1UL << offset);

}

static inline void clearb(ulong_t nr, volatile char *addr)
{
    ulong_t offset, bitoffset;
    offset = nr/8;
    bitoffset = nr % 8;

    addr[offset] &= ~(0x1UL << offset);

}
/**
 * Converts a block address to its block index in the specified buddy allocator.
 * A block's index is used to find the block's tag bit, mp->tag_bits[block_id].
 */
static inline ulong_t
block_to_id (struct buddy_mempool *mp, struct block *block)
{
    ulong_t block_id =
        ((ulong_t)block - mp->base_addr) >> mp->min_order;
#if 0
    if (block_id >= mp->num_blocks) {
        printk("The block %p\n", block);
        printk("Block ID is greater than the number of blocks in this pool!\n"
              "    MemPool Base Addr: %p\n"
              "    MemPool Size:    0x%lx\n"
              "    Min Order:       %u\n"
              "    Num Blocks:      0x%lx\n"
              "    Block ID :       0x%lx\n", 
              mp->base_addr,
              1UL<<mp->pool_order,
              mp->min_order,
              mp->num_blocks,
              block_id);

    }
#endif
    ASSERT(block_id < mp->num_blocks);

    return block_id;
}


/**
 * Marks a block as free by setting its tag bit to one.
 */
static inline void
mark_available (struct buddy_mempool *mp, struct block *block)
{
    if (block==(struct block*)0xdfa00000ULL) {
	BUDDY_DEBUG("Magic block %p: block_to_id=%lu\n", block, block_to_id(mp,block));
    }

    __set_bit(block_to_id(mp, block), (volatile char*)mp->tag_bits);
}


/**
 * Marks a block as allocated by setting its tag bit to zero.
 */
static inline void
mark_allocated (struct buddy_mempool *mp, struct block *block)
{
    __clear_bit(block_to_id(mp, block), (volatile char *)mp->tag_bits);
}


/**
 * Returns true if block is free, false if it is allocated.
 */
static inline int
is_available (struct buddy_mempool *mp, struct block *block)
{
    return test_bit(block_to_id(mp, block), mp->tag_bits);
}


/**
 * Returns the address of the block's buddy block.
 */
static void *
find_buddy (struct buddy_mempool *mp, struct block *block, ulong_t order)
{
    ulong_t _block;
    ulong_t _buddy;

    ASSERT((ulong_t)block >= mp->base_addr);

    /* Fixup block address to be zero-relative */
    _block = (ulong_t)block - mp->base_addr;

    /* Calculate buddy in zero-relative space */
    _buddy = _block ^ (1UL << order);

    /* Return the buddy's address */
    return (void *)(_buddy + mp->base_addr);
}


struct buddy_mempool *
buddy_init (ulong_t base_addr,
            ulong_t pool_order,
            ulong_t min_order)
{
    struct buddy_mempool *mp;
    ulong_t i;

    BUDDY_DEBUG("BUDDY INIT: base_addr=%p order=%lu (sul=%d)\n",(void*)base_addr,pool_order,sizeof(ulong_t));

    /* Smallest block size must be big enough to hold a block structure */
    if ((1UL << min_order) < sizeof(struct block)) {
        min_order = ilog2( roundup_pow_of_two(sizeof(struct block)) );
	BUDDY_DEBUG("min order fixed to %lu\n",min_order);
    }

    /* The minimum block order must be smaller than the pool order */
    if (min_order > pool_order) {
	BUDDY_DEBUG("pool order too small\n");
        return NULL;
    }

    mp = mm_boot_alloc(sizeof(struct buddy_mempool));
    if (!mp) {
        ERROR_PRINT("Could not allocate mempool\n");
        return NULL;
    }
    memset(mp, 0, sizeof(struct buddy_mempool));

    mp->base_addr  = base_addr;
    mp->pool_order = pool_order;
    mp->min_order  = min_order;

    /* Allocate a list for every order up to the maximum allowed order */
    mp->avail = mm_boot_alloc((pool_order + 1) * sizeof(struct list_head));

    if (!mp->avail) { 
	ERROR_PRINT("Cannot allocate list heads\n");
	return NULL;
    }

    /* Initially all lists are empty */
    for (i = 0; i <= pool_order; i++) {
        INIT_LIST_HEAD(&mp->avail[i]);
    }

    /* Allocate a bitmap with 1 bit per minimum-sized block */
    mp->num_blocks = (1UL << pool_order) / (1UL << min_order);
    mp->tag_bits   = mm_boot_alloc(BITS_TO_LONGS(mp->num_blocks) * sizeof(long));

    if (!mp->tag_bits) { 
	ERROR_PRINT("Could not allocate bitmap for mempool\n");
	return NULL;
    }

    BUDDY_DEBUG("num_blocks=%lu, tag_bits=%p alloc=%lu\n",mp->num_blocks, mp->tag_bits,
		BITS_TO_LONGS(mp->num_blocks)*sizeof(long));

    /* Initially mark all minimum-sized blocks as allocated */
    bitmap_zero(mp->tag_bits, mp->num_blocks);

    BUDDY_DEBUG("Created memory pool %p\n",mp);

    return mp;
}


/**
 * Allocates a block of memory of the requested size (2^order bytes).
 *
 * Arguments:
 *       [IN] mp:    Buddy system memory allocator object.
 *       [IN] order: Block size to allocate (2^order bytes).
 *
 * Returns:
 *       Success: Pointer to the start of the allocated memory block.
 *       Failure: NULL
 */
void *
buddy_alloc (struct buddy_mempool *mp, ulong_t order)
{
    ulong_t j;
    struct list_head *list;
    struct block *block;
    struct block *buddy_block;

    ASSERT(mp);

    BUDDY_DEBUG("BUDDY ALLOC on mempool : %p order: %lu mempool_order: %lu\n", mp, order, mp->pool_order);
;
    if (order > mp->pool_order) {
	BUDDY_DEBUG("order is too big\n");
        return NULL;
    }

    /* Fixup requested order to be at least the minimum supported */
    if (order < mp->min_order) {
        order = mp->min_order;
	BUDDY_DEBUG("order expanded to %lu\n",order);
    }

    for (j = order; j <= mp->pool_order; j++) {

        /* Try to allocate the first block in the order j list */
        list = &mp->avail[j];

        if (list_empty(list)) {
	    BUDDY_DEBUG("Skipping order %lu as the list is empty\n",j);
            continue;
        }

        block = list_entry(list->next, struct block, link);
        list_del(&block->link);
        mark_allocated(mp, block);

	BUDDY_DEBUG("Found block %p at order %lu\n",block,j);

        /* Trim if a higher order block than necessary was allocated */
        while (j > order) {
            --j;
            buddy_block = (struct block *)((ulong_t)block + (1UL << j));
            buddy_block->order = j;
            mark_available(mp, buddy_block);
	    BUDDY_DEBUG("Inserted buddy block %p into order %lu\n",buddy_block,j);
            list_add(&buddy_block->link, &mp->avail[j]);
        }

	BUDDY_DEBUG("Returning block %p\n",block);

        return block;
    }

    BUDDY_DEBUG("FAILED TO ALLOCATE from %p - RETURNING  NULL\n", mp);

    return NULL;
}


/**
 * Returns a block of memory to the buddy system memory allocator.
 */
void
buddy_free(
    //!    Buddy system memory allocator object.
    struct buddy_mempool *  mp,
    //!  Address of memory block to free.
    void *        addr,
    //! Size of the memory block (2^order bytes).
    ulong_t order
)
{
    ASSERT(mp);
    ASSERT(order <= mp->pool_order);

    

    BUDDY_DEBUG("BUDDY FREE on memory pool: %p addr=%p order=%lu\n",mp,addr,order);

    if (addr>=(void*)0xdf6a0000UL && addr<(void*)0x100000000UL) { 
	BUDDY_DEBUG("SKIPPED\n");
	return;
    }

    /* Fixup requested order to be at least the minimum supported */
    if (order < mp->min_order) {
        order = mp->min_order;
	BUDDY_DEBUG("updated order to %lu\n",order);
    }
    /* Overlay block structure on the memory block being freed */
    struct block * block = (struct block *) addr;

    ASSERT(!is_available(mp, block));

    /* Coalesce as much as possible with adjacent free buddy blocks */
    while (order < mp->pool_order) {

        /* Determine our buddy block's address */
        struct block * buddy = find_buddy(mp, block, order);

	BUDDY_DEBUG("buddy at order %lu is %p\n",order,buddy);

        /* Make sure buddy is available and has the same size as us */
        if (!is_available(mp, buddy)) {
	    BUDDY_DEBUG("buddy not available\n");
	    break;
	}

        if (is_available(mp, buddy) && (buddy->order != order)) {
	    BUDDY_DEBUG("buddy available but has order %lu\n",buddy->order);
            break;
        }

	BUDDY_DEBUG("buddy merge\n");

        /* OK, we're good to go... buddy merge! */
        list_del(&buddy->link);
        if (buddy < block) {
            block = buddy;
	}
        ++order;
        block->order = order;
    }
    
    /* Add the (possibly coalesced) block to the appropriate free list */
    block->order = order;

    BUDDY_DEBUG("End of search: block=%p order=%lu pool_order=%lu block->order=%lu\n",block,order,mp->pool_order,block->order);


    mark_available(mp, block);

    BUDDY_DEBUG("End of mark: block=%p order=%lu pool_order=%lu block->order=%lu\n",block,order,mp->pool_order,block->order);

    list_add(&block->link, &mp->avail[order]);

    BUDDY_DEBUG("block at %p of order %lu being made available\n",block,block->order);
    
    if (block->order==-1) { 
	ERROR_PRINT("FAIL: block order went nuts\n");
	ERROR_PRINT("mp->base_addr=%p mp->num_blocks=%lu  mp->min_order=%lu, block=%p\n",mp->base_addr,mp->num_blocks, mp->min_order,block);
	panic("Block order\n");
    }

}


/**
 * Dumps the state of a buddy system memory allocator object to the console.
 */
void
buddy_dump_mempool(struct buddy_mempool *mp)
{
    ulong_t i;
    ulong_t num_blocks;
    struct list_head *entry;

    BUDDY_DEBUG("DUMP OF BUDDY MEMORY POOL:\n");

    for (i = mp->min_order; i <= mp->pool_order; i++) {

        /* Count the number of memory blocks in the list */
        num_blocks = 0;
        list_for_each(entry, &mp->avail[i])
            ++num_blocks;

        BUDDY_DEBUG("  order %2lu: %lu free blocks\n", i, num_blocks);
    }
}
