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
#include <nautilus/buddy.h>
#include <nautilus/paging.h>
#include <nautilus/numa.h>
#include <nautilus/spinlock.h>
#include <nautilus/macros.h>
#include <nautilus/naut_assert.h>
#include <nautilus/math.h>
#include <nautilus/intrinsics.h>
#include <nautilus/percpu.h>

#ifndef NAUT_CONFIG_DEBUG_KMEM
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define KMEM_DEBUG(fmt, args...) DEBUG_PRINT("KMEM: " fmt, ##args)
#define KMEM_PRINT(fmt, args...) printk("KMEM: " fmt, ##args)
#define KMEM_WARN(fmt, args...)  WARN_PRINT("KMEM: " fmt, ##args)

/**
 * This specifies the minimum sized memory block to request from the underlying
 * buddy system memory allocator, 2^MIN_ORDER bytes. It must be at least big
 * enough to hold a 'struct kmem_block_hdr'.
 */
#define MIN_ORDER   5  /* 32 bytes */


/**
 * Magic value used for sanity checking. Every block of memory allocated via
 * kmem_alloc() has this value in its block header.
 */
#define KMEM_MAGIC  0xF0F0F0F0F0F0F0F0UL


/**
 *  * Total number of bytes in the kernel memory pool.
 *   */
static unsigned long kmem_bytes_managed;


/**
 *  * Total number of bytes allocated from the kernel memory pool.
 *   */
static unsigned long kmem_bytes_allocated = 0;


/* This is the list of all memory zones */
static struct list_head glob_zone_list;


/**
 * Each block of memory allocated from the kernel memory pool has one of these
 * structures at its head. The structure contains information needed to free
 * the block and return it to the underlying memory allocator.
 *
 * When a block is allocated, the address returned to the caller is
 * sizeof(struct kmem_block_hdr) bytes greater than the block allocated from
 * the underlying memory allocator.
 *
 * WARNING: This structure is defined to be exactly 16 bytes in size.
 *          Do not change this unless you really know what you are doing.
 */
struct kmem_block_hdr {
    uint64_t order;  /* order of the block allocated from buddy system */
    uint64_t magic;  /* magic value used as sanity check */
    struct buddy_mempool * zone; /* zone to which this block belongs */
} __packed;



struct mem_region *
kmem_get_base_zone (void)
{
    printk("getting base zone\n");
    return list_first_entry(&glob_zone_list, struct mem_region, glob_link);
}


/* TODO: if we're going to be using this at runtime, really need to 
 * key these regions in a tree
 */
struct mem_region *
kmem_get_region_by_addr (ulong_t addr)
{
    struct mem_region * region = NULL;
    list_for_each_entry(region, &glob_zone_list, glob_link) {
        if (addr >= region->base_addr && 
            addr < (region->base_addr + region->len)) {
            return region;
        }
    }

    return NULL;
}


/**
 * This adds a zone to the kernel memory pool. Zones exist to allow there to be
 * multiple non-adjacent regions of physically contiguous memory, and to represent
 * regions in different NUMA domains. Each NUMA domain can have various regions of
 * physical memory associated with it, each of which will have its own zone in 
 * the kernel memory pool.
 *
 * Note that the zone range will be in the *virtual* address space 
 * in the case we're compiling this as an HRT for operation in an HVM
 *
 * Arguments:
 *       [IN] region: The memory region to create a zone for
 *
 */
static struct buddy_mempool *
create_zone (struct mem_region * region)
{
    ulong_t pool_order = ilog2(roundup_pow_of_two(region->len));
    ulong_t min_order  = MIN_ORDER;
    struct buddy_mempool * pool = NULL;

    ASSERT(region);

    KMEM_DEBUG("Creating buddy zone for region at %p\n", region->base_addr);
    
    if (region->mm_state) {
        panic("Memory zone already exists for memory region ([%p - %p] domain %u)\n",
            (void*)region->base_addr,
            (void*)(region->base_addr + region->len),
            region->domain_id);
    }

    /* add this region to the global region list */
    list_add_tail(&(region->glob_link), &glob_zone_list);

    /* Initialize the underlying buddy allocator */
    return buddy_init(pa_to_va(region->base_addr), pool_order, min_order);
}


/**
 * This adds memory to the kernel memory pool. The memory region being added
 * must fall within a zone previously specified via kmem_create_zone().
 *
 * Arguments:
 *       [IN] mem:       region of memory to add
 *       [IN] base_addr: the base address of the memory to add
 *       [IN] size:      the size of the memory to add
 */
void
kmem_add_memory (struct mem_region * mem, 
                ulong_t base_addr, 
                size_t size)
{
    /*
     * kmem buddy allocator is initially empty.
     * Memory is added to it via buddy_free().
     * buddy_free() will panic if there are any problems with the args.
     */

    buddy_free(mem->mm_state, (void*)pa_to_va(base_addr), ilog2(size));

    /* Update statistics */
    kmem_bytes_managed += size;
}


/* 
 * initializes the kernel memory pools based on previously 
 * collected memory information (including NUMA domains etc.)
 */
int
nk_kmem_init (void)
{
    struct sys_info * sys = &(nk_get_nautilus_info()->sys);
    struct nk_locality_info * numa_info = &(nk_get_nautilus_info()->sys.locality_info);
    struct mem_region * ent = NULL;
    unsigned i = 0, j = 0;
    
    /* initialize the global zone list */
    INIT_LIST_HEAD(&glob_zone_list);

    for (; i < numa_info->num_domains; i++) {
        j = 0;
        list_for_each_entry(ent, &(numa_info->domains[i]->regions), entry) {
            ent->mm_state = create_zone(ent);
            if (!ent->mm_state) {
                panic("Could not create kmem zone for region %u in domain %u\n", j, i);
                return -1;
            }
            ++j;
        }
    }

    /* now, to avoid this logic at allocation time, 
     * we give each core an ordered list of regions 
     * based on distance from its home node. 
     * We'll try to allocate from these in order */
    for (i = 0; i < sys->num_cpus; i++) {
        struct list_head * local_regions = &(sys->cpus[i]->kmem.ordered_regions);
        INIT_LIST_HEAD(local_regions);

        // first add the local domain's regions
        struct numa_domain * loc_dom = sys->cpus[i]->domain;
        struct mem_region * mem = NULL;
        list_for_each_entry(mem, &loc_dom->regions, entry) {
            struct mem_reg_entry * newent = mm_boot_alloc(sizeof(struct mem_reg_entry));
            if (!newent) {
                ERROR_PRINT("Could not allocate mem region entry\n");
                return -1;
            }
            newent->mem = mem;
            KMEM_DEBUG("Adding region [%p] in domain %u to CPU %u's local region list\n",
                    mem->base_addr, 
                    loc_dom->id,
                    i);
            list_add_tail(&newent->mem_ent, local_regions);
        }

        struct domain_adj_entry * rem_dom_ent = NULL;
        list_for_each_entry(rem_dom_ent, &loc_dom->adj_list, list_ent) {
            struct numa_domain * rem_dom = rem_dom_ent->domain;
            struct mem_region *rem_reg = NULL;

            list_for_each_entry(rem_reg, &rem_dom->regions, entry) {
                struct mem_reg_entry * newent = mm_boot_alloc(sizeof(struct mem_reg_entry));
                if (!newent) {
                    ERROR_PRINT("Could not allocate mem region entry\n");
                    return -1;
                }
                newent->mem = rem_reg;
                list_add_tail(&newent->mem_ent, local_regions);
            }

        }

    }

    /* just to make sure */
    for (i = 0; i < sys->num_cpus; i++) {
        struct list_head * local_regions = &(sys->cpus[i]->kmem.ordered_regions);
        struct mem_reg_entry * reg = NULL;

        KMEM_PRINT("CPU %u region affinity list:\n", i);
        list_for_each_entry(reg, local_regions, mem_ent) {
            KMEM_PRINT("    [Domain=%u, %p-%p]\n", reg->mem->domain_id, reg->mem->base_addr, reg->mem->base_addr + reg->mem->len);
        }
    }

    return 0;
}


/**
 * Allocates memory from the kernel memory pool. This will return a memory
 * region that is at least 16-byte aligned. The memory returned is zeroed.
 *
 * Arguments:
 *       [IN] size: Amount of memory to allocate in bytes.
 *
 * Returns:
 *       Success: Pointer to the start of the allocated memory.
 *       Failure: NULL
 */
void *
malloc (size_t size)
{
    struct kmem_block_hdr *hdr = NULL;
    struct mem_reg_entry * reg = NULL;
    ulong_t order;
    cpu_id_t my_id = my_cpu_id();
    struct kmem_data * my_kmem = &(nk_get_nautilus_info()->sys.cpus[my_id]->kmem);

    /* Make room for block header */
    size += sizeof(struct kmem_block_hdr);

    /* Calculate the block order needed */
    order = ilog2(roundup_pow_of_two(size));
    if (order < MIN_ORDER) {
        order = MIN_ORDER;
    }

    /* scan the blocks in order of affinity */
    list_for_each_entry(reg, &(my_kmem->ordered_regions), mem_ent) {
        struct buddy_mempool * zone = reg->mem->mm_state;

        /* Allocate memory from the underlying buddy system */
        uint8_t flags = spin_lock_irq_save(&zone->lock);
        hdr = buddy_alloc(zone, order);
        spin_unlock_irq_restore(&zone->lock, flags);

        if (hdr) {
            hdr->zone = zone;
            break;
        }
        
    }

    if (hdr) {
        kmem_bytes_allocated += (1UL << order);
    } else {
        return NULL;
    }

    /* Initialize the block header */
    hdr->order = order;       /* kmem_free() needs this to free the block */
    hdr->magic = KMEM_MAGIC;  /* used for sanity check */

    /* Return address of first byte after block header to caller */
    return hdr + 1;
}


/**
 * Frees memory previously allocated with kmem_alloc().
 *
 * Arguments:
 *       [IN] addr: Address of the memory region to free.
 *
 * NOTE: The size of the memory region being freed is assumed to be in a
 *       'struct kmem_block_hdr' header located immediately before the address
 *       passed in by the caller. This header is created and initialized by
 *       kmem_alloc().
 */
void
free (const void * addr)
{
    struct kmem_block_hdr *hdr;
    struct buddy_mempool * zone;

    if (!addr) {
        return;
    }

    ASSERT((unsigned long)addr >= sizeof(struct kmem_block_hdr));

    /* Find the block header */
    hdr = (struct kmem_block_hdr *)addr - 1;

    ASSERT(hdr->magic == KMEM_MAGIC);

    zone = hdr->zone;

    /* Return block to the underlying buddy system */
    uint8_t flags = spin_lock_irq_save(&zone->lock);
    kmem_bytes_allocated -= (1UL << hdr->order);
    buddy_free(zone, hdr, hdr->order);
    spin_unlock_irq_restore(&zone->lock, flags);
}

