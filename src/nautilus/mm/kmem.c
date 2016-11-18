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
#define KMEM_ERROR(fmt, args...) ERROR_PRINT("KMEM: " fmt, ##args)
#define KMEM_PRINT(fmt, args...) INFO_PRINT("KMEM: " fmt, ##args)
#define KMEM_WARN(fmt, args...)  WARN_PRINT("KMEM: " fmt, ##args)

/**
 * This specifies the minimum sized memory block to request from the underlying
 * buddy system memory allocator, 2^MIN_ORDER bytes. It must be at least big
 * enough to hold a 'struct kmem_block_hdr'.
 */
#define MIN_ORDER   5  /* 32 bytes */

// The factor by which to reduce the hash table size
// next_prime(total_mem >> 5 / BLOAT) is the maximum number of
// blocks we can allocate with malloc
#define BLOAT  1024



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
 * Each block of memory allocated from the kernel memory pool has 
 * associated with it one of these structures.   The structure 
 * maps the address handed out by malloc to the order of the block
 * that was allocated and the buddy allocator that provided the
 * block 
 */
struct kmem_block_hdr {
    void *   addr;   /* address of block */
    uint64_t order;  /* order of the block allocated from buddy system */
    struct buddy_mempool * zone; /* zone to which this block belongs */
} __packed;


static struct kmem_block_hdr *block_hash_entries=0;
static uint64_t               block_hash_num_entries=0;

// roughly power-of-two primes
static const uint64_t primes[] = 
{ 
    53, 97, 193, 389,
    769, 1543, 3079, 6151,
    12289, 24593, 49157, 98317,
    196613, 393241, 786433, 1572869,
    3145739, 6291469, 12582917, 25165843,
    50331653, 100663319, 201326611, 402653189,
    805306457, 1610612741 
};

static uint64_t next_prime(uint64_t n)
{
  uint64_t i;
  for (i=0;i<sizeof(primes)/sizeof(primes[0]);i++) {
    if (primes[i]>=n) { 
      return primes[i];
    }
  }
  KMEM_DEBUG("next_prime: %lu is too big, returning maximum prime in table (%lu)\n",primes[i-1]);

  return primes[i-1];
}

static int block_hash_init(uint64_t bytes)
{
  uint64_t num_entries = next_prime((bytes >> MIN_ORDER) / BLOAT);
  uint64_t entry_size = sizeof(*block_hash_entries);
  
  KMEM_DEBUG("block_hash_init with %lu entries each of size %lu bytes (%lu bytes)\n",
	     num_entries, entry_size, num_entries*entry_size);

  block_hash_entries = mm_boot_alloc(num_entries*entry_size);

  if (!block_hash_entries) { 
    KMEM_ERROR("block_hash_init failed\n");
    return -1;
  }

  memset(block_hash_entries,0,num_entries*entry_size);
  
  block_hash_num_entries = num_entries;

  return 0;
}

static inline uint64_t block_hash_hash(const void *ptr)
{
  uint64_t n = ((uint64_t) ptr)>>MIN_ORDER;

  n =  n ^ (n>>1) ^ (n<<2) ^ (n>>3) ^ (n<<5) ^ (n>>7)
    ^ (n<<11) ^ (n>>13) ^ (n<<17) ^ (n>>19) ^ (n<<23) 
    ^ (n>>29) ^ (n<<31) ^ (n>>37) ^ (n<<41) ^ (n>>43)
    ^ (n<<47) ^ (n<<53) ^ (n>>59) ^ (n<<61);

  n = n % block_hash_num_entries;

  KMEM_DEBUG("hash of %p returns 0x%lx\n",ptr, n);
  
  return n;
}
    
static inline struct kmem_block_hdr * block_hash_find_entry(const void *ptr)
{
  uint64_t i;
  uint64_t start = block_hash_hash(ptr);
  
  for (i=start;i<block_hash_num_entries;i++) { 
    if (block_hash_entries[i].addr == ptr) { 
      KMEM_DEBUG("Find entry scanned %lu entries\n", i-start+1);
      return &block_hash_entries[i];
    }
  }
  for (i=0;i<start;i++) { 
    if (block_hash_entries[i].addr == ptr) { 
      KMEM_DEBUG("Find entry scanned %lu entries\n", block_hash_num_entries-start + i + 1);
      return &block_hash_entries[i];
    }
  }
  return 0;
}

static inline struct kmem_block_hdr * block_hash_alloc(void *ptr)
{
  uint64_t i;
  uint64_t start = block_hash_hash(ptr);
  
  for (i=start;i<block_hash_num_entries;i++) { 
    if (__sync_bool_compare_and_swap(&block_hash_entries[i].order,0,1)) {
      KMEM_DEBUG("Allocation scanned %lu entries\n", i-start+1);
      return &block_hash_entries[i];
    }
  }
  for (i=0;i<start;i++) { 
    if (__sync_bool_compare_and_swap(&block_hash_entries[i].order,0,1)) {
      KMEM_DEBUG("Allocation scanned %lu entries\n", block_hash_num_entries-start + i + 1);
      return &block_hash_entries[i];
    }
  }
  return 0;
}

static inline void block_hash_free_entry(struct kmem_block_hdr *b)
{
  b->addr = 0;
  b->zone = 0;
  __sync_fetch_and_and (&b->order,0);
}

static inline int block_hash_free(void *ptr)
{
  struct kmem_block_hdr *b = block_hash_find_entry(ptr);

  if (!b) { 
    return -1;
  } else {
    block_hash_free_entry(b);
    return 0;
  }
}


struct mem_region *
kmem_get_base_zone (void)
{
    KMEM_DEBUG("getting base zone\n");
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
    
    KMEM_DEBUG("computed pool order=%lu, len=%lu, roundup_len=%lu\n",
	       pool_order, region->len, roundup_pow_of_two(region->len));

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
     * However, buddy_free() does expect chunks of memory aligned
     * to their size, which we manufacture out of the memory given.
     * buddy_free() will coalesce these chunks as appropriate
     */

    uint64_t max_chunk_size = base_addr ? 1ULL << __builtin_ctzl(base_addr) : size;
    uint64_t chunk_size = max_chunk_size < size ? max_chunk_size : size;
    uint64_t chunk_order = ilog2(chunk_size); // floor
    uint64_t num_chunks = size/chunk_size; // floor
    void *addr=(void*)pa_to_va(base_addr);
    uint64_t i;

    KMEM_DEBUG("Add Memory to region %p base_addr=0x%llx size=0x%llx chunk_size=0x%llx, chunk_order=0x%llx, num_chunks=0x%llx, addr=%p\n",
	       mem,base_addr,size,chunk_size,chunk_order,num_chunks,addr);
    
    for (i=0;i<num_chunks;i++) { 
	buddy_free(mem->mm_state, addr+i*chunk_size, chunk_order);
    }

    /* Update statistics */
    kmem_bytes_managed += chunk_size*num_chunks;
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
    uint64_t total_mem=0;
    uint64_t total_phys_mem=0;
    
    /* initialize the global zone list */
    INIT_LIST_HEAD(&glob_zone_list);

    for (; i < numa_info->num_domains; i++) {
        j = 0;
        list_for_each_entry(ent, &(numa_info->domains[i]->regions), entry) {
	    if (ent->len < (1UL << MIN_ORDER)) { 
		KMEM_DEBUG("Skipping kmem initialization of oddball region of size %lu\n", ent->len);
		continue;
	    }
            ent->mm_state = create_zone(ent);
            if (!ent->mm_state) {
                panic("Could not create kmem zone for region %u in domain %u\n", j, i);
                return -1;
            }
	    total_phys_mem += ent->len;
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

    total_mem = 0;
    /* just to make sure */
    for (i = 0; i < sys->num_cpus; i++) {
        struct list_head * local_regions = &(sys->cpus[i]->kmem.ordered_regions);
        struct mem_reg_entry * reg = NULL;

        KMEM_DEBUG("CPU %u region affinity list:\n", i);
        list_for_each_entry(reg, local_regions, mem_ent) {
            KMEM_DEBUG("    [Domain=%u, %p-%p]\n", reg->mem->domain_id, reg->mem->base_addr, reg->mem->base_addr + reg->mem->len);
	    total_mem += reg->mem->len;
        }
    }

    KMEM_PRINT("Malloc configured to support a maximum of: 0x%lx bytes of physical memory\n", total_phys_mem);

    if (block_hash_init(total_phys_mem)) { 
      KMEM_ERROR("Failed to initialize block hash\n");
      return -1;
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
    void *block = 0;
    struct kmem_block_hdr *hdr = NULL;
    struct mem_reg_entry * reg = NULL;
    ulong_t order;
    cpu_id_t my_id = my_cpu_id();
    struct kmem_data * my_kmem = &(nk_get_nautilus_info()->sys.cpus[my_id]->kmem);


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
        block = buddy_alloc(zone, order);
        spin_unlock_irq_restore(&zone->lock, flags);

	if (block) {
	  hdr = block_hash_alloc(block);
	  if (!hdr) {
	    buddy_free(zone,block,order);
	  }
	}

        if (hdr) {
	    hdr->addr = block;
	    hdr->order = order;
            hdr->zone = zone;
            break;
        }
        
    }

    if (hdr) {
        kmem_bytes_allocated += (1UL << order);
    } else {
        return NULL;
    }

    /* Return address of the block */
    return block;
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
free (void * addr)
{
    struct kmem_block_hdr *hdr;
    struct buddy_mempool * zone;

    if (!addr) {
        return;
    }

    hdr = block_hash_find_entry(addr);

    if (!hdr) { 
      KMEM_DEBUG("Failed to find entry for block %p\n",addr);
      return;
    }

    zone = hdr->zone;

    /* Return block to the underlying buddy system */
    uint8_t flags = spin_lock_irq_save(&zone->lock);
    kmem_bytes_allocated -= (1UL << hdr->order);
    buddy_free(zone, addr, hdr->order);
    spin_unlock_irq_restore(&zone->lock, flags);
    block_hash_free_entry(hdr);
}

