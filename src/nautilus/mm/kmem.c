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
 * Copyright (c) 2017, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter A. Dinda <pdinda@northwestern.edu>
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
#include <nautilus/shell.h>

#include <dev/gpio.h>

#ifndef NAUT_CONFIG_DEBUG_KMEM
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#include <nautilus/backtrace.h>

// turn this on to have a sanity check run before and after each
// malloc and free
#define SANITY_CHECK_PER_OP 0

#define KMEM_DEBUG(fmt, args...) DEBUG_PRINT("KMEM: " fmt, ##args)
#define KMEM_ERROR(fmt, args...) ERROR_PRINT("KMEM: " fmt, ##args)
#define KMEM_PRINT(fmt, args...) INFO_PRINT("KMEM: " fmt, ##args)
#define KMEM_WARN(fmt, args...)  WARN_PRINT("KMEM: " fmt, ##args)

#ifndef NAUT_CONFIG_DEBUG_KMEM
#define KMEM_DEBUG_BACKTRACE()
#else
#define KMEM_DEBUG_BACKTRACE() BACKTRACE(KMEM_DEBUG,3)
#endif	

#define KMEM_ERROR_BACKTRACE() BACKTRACE(KMEM_ERROR,3)
	    

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
                     /* order>=MIN_ORDER => in use, safe to examine */
                     /* order==0 => unallocated header */
                     /* order==1 => allocation in progress, unsafe */
    struct buddy_mempool * zone; /* zone to which this block belongs */
    uint64_t flags;  /* flags for this allocated block */
} __packed __attribute((aligned(8)));


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
    if (block_hash_entries[i].order>=MIN_ORDER && block_hash_entries[i].addr == ptr) { 
      KMEM_DEBUG("Find entry scanned %lu entries\n", i-start+1);
      return &block_hash_entries[i];
    }
  }
  for (i=0;i<start;i++) { 
    if (block_hash_entries[i].order>=MIN_ORDER && block_hash_entries[i].addr == ptr) { 
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
  b->flags = 0;
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
    list_add(&(region->glob_link), &glob_zone_list);

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

void *boot_mm_get_cur_top();

static void *kmem_private_start;
static void *kmem_private_end;

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
    
    kmem_private_start = boot_mm_get_cur_top();

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
                KMEM_ERROR("Could not allocate mem region entry\n");
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
	    if ((reg->mem->base_addr + reg->mem->len) >= sys->mem.phys_mem_avail) {
		sys->mem.phys_mem_avail = reg->mem->base_addr + reg->mem->len;
	    }
        }
    }

    KMEM_PRINT("Malloc configured to support a maximum of: 0x%lx bytes of physical memory\n", total_phys_mem);

    if (block_hash_init(total_phys_mem)) { 
      KMEM_ERROR("Failed to initialize block hash\n");
      return -1;
    }


    // the assumption here is that no further boot_mm allocations will
    // be made by kmem from this point on
    kmem_private_end = boot_mm_get_cur_top();

    return 0;
}


// A fake header representing the boot allocations
static void     *boot_start;
static void     *boot_end;
static uint64_t  boot_flags;

void kmem_inform_boot_allocation(void *low, void *high)
{
    KMEM_DEBUG("Handling boot range %p-%p\n", low, high);
    boot_start = low;
    boot_end = high;
    boot_flags = 0;
    KMEM_PRINT("   boot range: %p-%p   kmem private: %p-%p\n",
 	       low, high, kmem_private_start, kmem_private_end);
}


/**
 * Allocates memory from the kernel memory pool. This will return a memory
 * region that is at least 16-byte aligned. The memory returned is 
 * optionally zeroed.
 *
 * Arguments:
 *       [IN] size: Amount of memory to allocate in bytes.
 *       [IN] cpu:  affinity cpu (-1 => current cpu)
 *       [IN] zero: Whether to zero the whole allocated block
 *
 * Returns:
 *       Success: Pointer to the start of the allocated memory.
 *       Failure: NULL
 */
static void *
_kmem_malloc (size_t size, int cpu, int zero)
{
    NK_GPIO_OUTPUT_MASK(0x20,GPIO_OR);
    int first = 1;
    void *block = 0;
    struct kmem_block_hdr *hdr = NULL;
    struct mem_reg_entry * reg = NULL;
    ulong_t order;
    cpu_id_t my_id;

    if (cpu<0 || cpu>= nk_get_num_cpus()) {
	my_id = my_cpu_id();
    } else {
	my_id = cpu;
    }

    struct kmem_data * my_kmem = &(nk_get_nautilus_info()->sys.cpus[my_id]->kmem);

    KMEM_DEBUG("malloc of %lu bytes (zero=%d) from:\n",size,zero);
    KMEM_DEBUG_BACKTRACE();

#if SANITY_CHECK_PER_OP
    if (kmem_sanity_check()) { 
	panic("KMEM HAS GONE INSANE PRIOR TO MALLOC\n");
	return 0;
    }
#endif

    /* Calculate the block order needed */
    order = ilog2(roundup_pow_of_two(size));
    if (order < MIN_ORDER) {
        order = MIN_ORDER;
    }

 retry:

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
            KMEM_DEBUG("malloc cannot allocate header, releasing block\n");
	    flags = spin_lock_irq_save(&zone->lock);
	    buddy_free(zone,block,order);
	    spin_unlock_irq_restore(&zone->lock, flags);
	    block=0;
	  }
	}

        if (hdr) {
	    hdr->addr = block;
            hdr->zone = zone;
	    // force a software barrier here, since our next write must come last
	    __asm__ __volatile__ ("" :::"memory");
	    hdr->order = order; // allocation complete
            break;
        }
        
    }

    if (hdr) {
        kmem_bytes_allocated += (1UL << order);
    } else {
	// attempt to get memory back by reaping threads now...
	if (first) {
	    KMEM_DEBUG("malloc initially failed for size %lu order %lu attempting reap\n",size,order);
	    nk_sched_reap(1);
	    first=0;
	    goto retry;
	}
	KMEM_DEBUG("malloc permanently failed for size %lu order %lu\n",size,order);
	NK_GPIO_OUTPUT_MASK(~0x20,GPIO_AND);
        return NULL;
    }

    KMEM_DEBUG("malloc succeeded: size %lu order %lu -> 0x%lx\n",size, order, block);
 
    if (zero) { 
	memset(block,0,1ULL << hdr->order);
    }
     
#if SANITY_CHECK_PER_OP
    if (kmem_sanity_check()) { 
	panic("KMEM HAS GONE INSANE AFTER MALLOC\n");
	return 0;
    }
#endif

    NK_GPIO_OUTPUT_MASK(~0x20,GPIO_AND);

    /* Return address of the block */
    return block;
}


void *kmem_malloc(size_t size)
{
    return _kmem_malloc(size,-1,0);
}

void *kmem_mallocz(size_t size)
{
    return _kmem_malloc(size,-1,1);
}

void *kmem_malloc_specific(size_t size, int cpu, int zero)
{
    return _kmem_malloc(size,cpu,zero);
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
kmem_free (void * addr)
{
    struct kmem_block_hdr *hdr;
    struct buddy_mempool * zone;
    uint64_t order;

    KMEM_DEBUG("free of address %p from:\n", addr);
    KMEM_DEBUG_BACKTRACE();

#if SANITY_CHECK_PER_OP
    if (kmem_sanity_check()) { 
	panic("KMEM HAS GONE INSANE PRIOR TO FREE\n");
	return;
    }
#endif

    if (!addr) {
        return;
    }


    // Note that if the user is doing a double-free, it is possible
    // that we race on the block hash entry and so could end up invoking
    // the buddy free more than once

    hdr = block_hash_find_entry(addr);

    if (!hdr) { 
      KMEM_ERROR("Failed to find entry for block %p in kmem_free()\n",addr);
      KMEM_ERROR_BACKTRACE();
      return;
    }

    zone = hdr->zone;
    order = hdr->order;

    // Sanity check things here
    // this will in some cases catch a double free that is causing a
    // race on the header
    if (!zone || order<MIN_ORDER) {
	KMEM_ERROR("Likely double free ignored- addr=%p, zone=%p order=%lu, hdr=%p, hdr->addr=%p, hdr->order=%lu\n", addr,zone, order, hdr,hdr->addr,hdr->order);
	BACKTRACE(KMEM_ERROR,3);
	// avoid freeing the header a second time
	// block_hash_free_entry(hdr);
	return;
    }

    
    /* Return block to the underlying buddy system */
    uint8_t flags = spin_lock_irq_save(&zone->lock);
    kmem_bytes_allocated -= (1UL << order);
    buddy_free(zone, addr, order);
    spin_unlock_irq_restore(&zone->lock, flags);
    KMEM_DEBUG("free succeeded: addr=0x%lx order=%lu\n",addr,order);
    block_hash_free_entry(hdr);

#if SANITY_CHECK_PER_OP
    if (kmem_sanity_check()) { 
	panic("KMEM HAS GONE INSANE AFTER FREE\n");
	return;
    }
#endif

}

/*
 * This is a *dead simple* implementation of realloc that tries to change the
 * size of the allocation pointed to by ptr to size, and returns ptr.  Realloc will
 * malloc a new block of memory, copy as much of the old data as it can, and free the
 * old block. If ptr is NULL, this is equivalent to a malloc for the specified size.
 *
 */
void * 
kmem_realloc (void * ptr, size_t size)
{
	struct kmem_block_hdr *hdr;
	size_t old_size;
	void * tmp = NULL;

	/* this is just a malloc */
	if (!ptr) {
		return kmem_malloc(size);
	}

	hdr = block_hash_find_entry(ptr);

	if (!hdr) {
		KMEM_DEBUG("Realloc failed to find entry for block %p\n", ptr);
		return NULL;
	}

	old_size = 1 << hdr->order;
	tmp = kmem_malloc(size);
	if (!tmp) {
		panic("Realloc failed\n");
	}

	if (old_size >= size) {
		memcpy(tmp, ptr, size);
	} else {
		memcpy(tmp, ptr, old_size);
	}
	
	kmem_free(ptr);
	return tmp;
}


typedef enum {GET,COUNT} stat_type_t;

static uint64_t _kmem_stats(struct kmem_stats *stats, stat_type_t what)
{
    uint64_t cur;
    struct mem_reg_entry * reg = NULL;
    struct kmem_data * my_kmem = &(nk_get_nautilus_info()->sys.cpus[my_cpu_id()]->kmem);
    if (what==GET) { 
	uint64_t num = stats->max_pools;
	memset(stats,0,sizeof(*stats));
	stats->min_alloc_size=-1;
	stats->max_pools = num;
    }

    // We will scan all memory from the current CPU's perspective
    // Since every CPUs sees all memory pools (albeit in NUMA order)
    // this will cover all memory
    cur = 0;
    list_for_each_entry(reg, &(my_kmem->ordered_regions), mem_ent) {
	if (what==GET) { 
	    struct buddy_mempool * zone = reg->mem->mm_state;
	    struct buddy_pool_stats pool_stats;
	    buddy_stats(zone,&pool_stats);
	    if (cur<stats->max_pools) { 
		stats->pool_stats[cur] = pool_stats;
		stats->num_pools++;
	    }
	    stats->total_blocks_free += pool_stats.total_blocks_free;
	    stats->total_bytes_free += pool_stats.total_bytes_free;
	    if (pool_stats.min_alloc_size < stats->min_alloc_size) { 
		stats->min_alloc_size = pool_stats.min_alloc_size;
	    }
	    if (pool_stats.max_alloc_size > stats->max_alloc_size) { 
		stats->max_alloc_size = pool_stats.max_alloc_size;
	    }
	}
	cur++;
    }
    if (what==GET) {
	stats->total_num_pools=cur;
    }
    return cur;
}


uint64_t kmem_num_pools()
{
    return _kmem_stats(0,COUNT);
}

void kmem_stats(struct kmem_stats *stats)
{
    _kmem_stats(stats,GET);
}

int kmem_sanity_check()
{
    int rc=0;
    uint64_t cur;
    struct mem_reg_entry * reg = NULL;
    struct kmem_data * my_kmem = &(nk_get_nautilus_info()->sys.cpus[my_cpu_id()]->kmem);

    list_for_each_entry(reg, &(my_kmem->ordered_regions), mem_ent) {
	struct buddy_mempool * zone = reg->mem->mm_state;
	if (buddy_sanity_check(zone)) { 
	    ERROR_PRINT("buddy memory pool %p is insane\n", zone);
	    rc|=-1;
	}
    }

    return rc;
}


void  kmem_get_internal_pointer_range(void **start, void **end)
{
    *start = kmem_private_start;
    *end = kmem_private_end;
}

int  kmem_find_block(void *any_addr, void **block_addr, uint64_t *block_size, uint64_t *flags)
{
    uint64_t i;
    uint64_t order;
    addr_t   zone_base;
    uint64_t zone_min_order;
    uint64_t zone_max_order;
    addr_t   any_offset;
    struct mem_region *reg;

    if (!(reg = kmem_get_region_by_addr((addr_t)any_addr))) {
	// not in any region we manage
	return -1;
    }

    if (any_addr>=boot_start && any_addr<boot_end) { 
	// in some boot_mm allocation that we treat as a single block
	*block_addr = boot_start;
	*block_size = boot_end-boot_start;
	*flags = boot_flags;
	KMEM_DEBUG("Search of %p found boot block (%p-%p)\n", any_addr, boot_start, boot_end);
	return 0;
    }

    zone_base = reg->mm_state->base_addr;
    zone_min_order = reg->mm_state->min_order;
    zone_max_order = reg->mm_state->pool_order;

    any_offset = (addr_t)any_addr - (addr_t)zone_base;
    
    for (order=zone_min_order;order<=zone_max_order;order++) {
	addr_t mask = ~((1ULL << order)-1);
	void *search_addr = (void*)(zone_base + (any_offset & mask));
	struct kmem_block_hdr *hdr = block_hash_find_entry(search_addr);
	// must exist and must be allocated
	if (hdr && hdr->order>=MIN_ORDER) { 
	    *block_addr = search_addr;
	    *block_size = 0x1ULL<<hdr->order;
	    *flags = hdr->flags;
	    return 0;
	    
	}
    }
    return -1;
}


// set the flags of an allocated block
int  kmem_set_block_flags(void *block_addr, uint64_t flags)
{
    if (block_addr>=boot_start && block_addr<boot_end) { 
	boot_flags = flags;
	return 0;

    } else {

	struct kmem_block_hdr *h =  block_hash_find_entry(block_addr);
	
	if (!h || h->order<MIN_ORDER) { 
	    return -1;
	} else {
	    h->flags = flags;
	    return 0;
	}
    }
}

// applies only to allocated blocks
int  kmem_mask_all_blocks_flags(uint64_t mask, int or)
{
    uint64_t i;

    if (!or) { 
	boot_flags &= mask;
	for (i=0;i<block_hash_num_entries;i++) { 
	    if (block_hash_entries[i].order>=MIN_ORDER) { 
		block_hash_entries[i].flags &= mask;
	    }
	}
    } else {
	boot_flags |= mask;
	for (i=0;i<block_hash_num_entries;i++) { 
	    if (block_hash_entries[i].order>=MIN_ORDER) { 
		block_hash_entries[i].flags |= mask;
	    }
	}
    }

    return 0;
}
    
int  kmem_apply_to_matching_blocks(uint64_t mask, uint64_t flags, int (*func)(void *block, void *state), void *state)
{
    uint64_t i;
    
    if (((boot_flags & mask) == flags)) {
	if (func(boot_start,state)) { 
	    return -1;
	}
    }

    for (i=0;i<block_hash_num_entries;i++) { 
	if (block_hash_entries[i].order>=MIN_ORDER) { 
	    if ((block_hash_entries[i].flags & mask) == flags) {
		if (func(block_hash_entries[i].addr,state)) { 
		    return -1;
		}
	    }
	}
    } 
    
    return 0;
}
    

// We also create malloc, etc, functions to link to
// This is needed for C++ support or anything else
// that expects these to exist in some object file...

// First we generate functions using the macros, which means
// we get the glue logic from the macros, e.g., to the garbage collectors

static inline void *ext_malloc(size_t size)
{
    // this is expanded using the malloc wrapper in mm.h
    return malloc(size);
}

static inline void ext_free(void *p)
{
    // this is expanded using the free wrapper in mm.h
    free(p);
}

static inline void *ext_realloc(void *p, size_t s)
{
    // this is expanded using the realloc wrapper in mm.h
    return realloc(p,s);
}

// Next we blow away the macros

#undef malloc
#undef free
#undef realloc


// Finally, we generate the linkable functions

void *malloc(size_t size)
{
    return ext_malloc(size);
}

void free(void *p)
{
    return ext_free(p);
}

void *realloc(void *p, size_t n)
{
    return ext_realloc(p,n);
}

static int
handle_meminfo (char * buf, void * priv)
{
    uint64_t num = kmem_num_pools();
    struct kmem_stats *s = malloc(sizeof(struct kmem_stats)+num*sizeof(struct buddy_pool_stats));
    uint64_t i;

    if (!s) { 
        nk_vc_printf("Failed to allocate space for mem info\n");
        return 0;
    }

    s->max_pools = num;

    kmem_stats(s);

    for (i=0;i<s->num_pools;i++) { 
        nk_vc_printf("pool %lu %p-%p %lu blks free %lu bytes free\n  %lu bytes min %lu bytes max\n", 
                i,
                s->pool_stats[i].start_addr,
                s->pool_stats[i].end_addr,
                s->pool_stats[i].total_blocks_free,
                s->pool_stats[i].total_bytes_free,
                s->pool_stats[i].min_alloc_size,
                s->pool_stats[i].max_alloc_size);
    }

    nk_vc_printf("%lu pools %lu blks free %lu bytes free\n", s->total_num_pools, s->total_blocks_free, s->total_bytes_free);
    nk_vc_printf("  %lu bytes min %lu bytes max\n", s->min_alloc_size, s->max_alloc_size);

    free(s);

    return 0;
}


static struct shell_cmd_impl meminfo_impl = {
    .cmd      = "meminfo",
    .help_str = "meminfo [detail]",
    .handler  = handle_meminfo,
};
nk_register_shell_cmd(meminfo_impl);


#define BYTES_PER_LINE 16

static int
handle_mem (char * buf, void * priv)
{
    uint64_t addr, data, len, size;

    if ((sscanf(buf, "mem %lx %lu %lu",&addr,&len,&size)==3) ||
            (size=8, sscanf(buf, "mem %lx %lu", &addr, &len)==2)) { 
        uint64_t i,j,k;
        for (i=0;i<len;i+=BYTES_PER_LINE) {
            nk_vc_printf("%016lx :",addr+i);
            for (j=0;j<BYTES_PER_LINE && (i+j)<len; j+=size) {
                nk_vc_printf(" ");
                for (k=0;k<size;k++) { 
                    nk_vc_printf("%02x", *(uint8_t*)(addr+i+j+k));
                }
            }
            nk_vc_printf(" ");
            for (j=0;j<BYTES_PER_LINE && (i+j)<len; j+=size) {
                for (k=0;k<size;k++) { 
                    nk_vc_printf("%c", isalnum(*(uint8_t*)(addr+i+j+k)) ? 
                            *(uint8_t*)(addr+i+j+k) : '.');
                }
            }
            nk_vc_printf("\n");
        }	      

        return 0;
    }
    return 0;
}

static struct shell_cmd_impl mem_impl = {
    .cmd      = "mem",
    .help_str = "mem x n [s]",
    .handler  = handle_mem,
};
nk_register_shell_cmd(mem_impl);


static int
handle_peek (char * buf, void * priv)
{
    uint64_t addr, data, len, size;
    char bwdq;

    if (((bwdq='b', sscanf(buf,"peek b %lx", &addr))==1) ||
            ((bwdq='w', sscanf(buf,"peek w %lx", &addr))==1) ||
            ((bwdq='d', sscanf(buf,"peek d %lx", &addr))==1) ||
            ((bwdq='q', sscanf(buf,"peek q %lx", &addr))==1) ||
            ((bwdq='q', sscanf(buf,"peek %lx", &addr))==1)) {
        switch (bwdq) { 
            case 'b': 
                data = *(uint8_t*)addr;       
                nk_vc_printf("Mem[0x%016lx] = 0x%02lx\n",addr,data);
                break;
            case 'w': 
                data = *(uint16_t*)addr;       
                nk_vc_printf("Mem[0x%016lx] = 0x%04lx\n",addr,data);
                break;
            case 'd': 
                data = *(uint32_t*)addr;       
                nk_vc_printf("Mem[0x%016lx] = 0x%08lx\n",addr,data);
                break;
            case 'q': 
                data = *(uint64_t*)addr;       
                nk_vc_printf("Mem[0x%016lx] = 0x%016lx\n",addr,data);
                break;
            default:
                nk_vc_printf("Unknown size requested\n",bwdq);
                break;
        }
        return 0;
    }

    nk_vc_printf("invalid poke command\n");

    return 0;
}

static struct shell_cmd_impl peek_impl = {
    .cmd      = "peek",
    .help_str = "peek [bwdq] x",
    .handler  = handle_peek,
};
nk_register_shell_cmd(peek_impl);

static int
handle_poke (char * buf, void * priv)
{
    uint64_t addr, data, len, size;
    char bwdq;

    if (((bwdq='b', sscanf(buf,"poke b %lx %lx", &addr,&data))==2) ||
            ((bwdq='w', sscanf(buf,"poke w %lx %lx", &addr,&data))==2) ||
            ((bwdq='d', sscanf(buf,"poke d %lx %lx", &addr,&data))==2) ||
            ((bwdq='q', sscanf(buf,"poke q %lx %lx", &addr,&data))==2) ||
            ((bwdq='q', sscanf(buf,"poke %lx %lx", &addr, &data))==2)) {
        switch (bwdq) { 
            case 'b': 
                *(uint8_t*)addr = data; clflush_unaligned((void*)addr,1);
                nk_vc_printf("Mem[0x%016lx] = 0x%02lx\n",addr,data);
                break;
            case 'w': 
                *(uint16_t*)addr = data; clflush_unaligned((void*)addr,2);
                nk_vc_printf("Mem[0x%016lx] = 0x%04lx\n",addr,data);
                break;
            case 'd': 
                *(uint32_t*)addr = data; clflush_unaligned((void*)addr,4);
                nk_vc_printf("Mem[0x%016lx] = 0x%08lx\n",addr,data);
                break;
            case 'q': 
                *(uint64_t*)addr = data; clflush_unaligned((void*)addr,8);
                nk_vc_printf("Mem[0x%016lx] = 0x%016lx\n",addr,data);
                break;
            default:
                nk_vc_printf("Unknown size requested\n");
                break;
        }
        return 0;
    }

    nk_vc_printf("invalid poke command\n");

    return 0;
}

static struct shell_cmd_impl poke_impl = {
    .cmd      = "poke",
    .help_str = "poke [bwdq] x y",
    .handler  = handle_poke,
};
nk_register_shell_cmd(poke_impl);
