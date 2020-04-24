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
 */
#ifndef __MM_H__
#define __MM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/naut_types.h>
#include <nautilus/list.h>
#include <nautilus/buddy.h>

#define MAX_MMAP_ENTRIES 128

typedef struct mem_map_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} mem_map_entry_t;

typedef struct mmap_info {
    uint64_t usable_ram; 
    uint64_t total_mem; /* includes I/O holes etc. */
    uint32_t last_pfn;
    uint32_t num_regions;
} mmap_info_t;

typedef struct boot_mem_info {
    ulong_t * page_map;
    ulong_t   npages;
    ulong_t   pm_len;
    ulong_t   last_offset;
    ulong_t   last_pos;
    ulong_t   last_success;
} boot_mem_info_t;

struct mem_map_entry * mm_boot_get_region(unsigned i);
unsigned mm_boot_num_regions(void);

uint64_t mm_get_usable_ram(void);
uint64_t mm_get_all_mem(void);
ulong_t mm_boot_last_pfn(void);
int mm_boot_init (ulong_t mbd);
void mm_boot_kmem_init(void);
void mm_boot_kmem_cleanup(void);

void mm_dump_page_map(void);

void mm_boot_reserve_mem(addr_t start, ulong_t size);
void mm_boot_reserve_vmem(addr_t start, ulong_t size);
void mm_boot_free_mem(addr_t start, ulong_t size);
void mm_boot_free_vmem(addr_t start, ulong_t size);
void * __mm_boot_alloc(ulong_t size, ulong_t align, ulong_t goal);
void * mm_boot_alloc (ulong_t size);
void * mm_boot_alloc_aligned (ulong_t size, ulong_t align);
void mm_boot_free(void *addr, ulong_t size);


/* KMEM FUNCTIONS */

struct kmem_data {
    struct list_head ordered_regions;
};

int nk_kmem_init(void);

struct mem_region;

struct mem_region * kmem_get_base_zone(void);
struct mem_region * kmem_get_region_by_addr(ulong_t addr);
void kmem_add_memory(struct mem_region * mem, ulong_t base_addr, size_t size);

// this the range of heap addresses used by the boot allocator [low,high)
void kmem_inform_boot_allocation(void *low, void *high);

// These functions operate the core memory allocator directly
// You want to use the malloc()/free() wrappers defined below 
// unless you know  what you are doing
void * kmem_malloc_specific(size_t size, int cpu, int zero);
void * kmem_malloc(size_t size);
void * kmem_mallocz(size_t size);
void * kmem_realloc(void * ptr, size_t size);
void   kmem_free(void * addr);

// Support functions for garbage collection
// We currently assume these are done with the world stopped,
// hence no locking

// find the matching block that contains addr and its flags
// returns nonzero if the addr is invalid or within no allocated block
// user flags are allocate from low bit up, while kmem's flags are allocated
// high bit down
int  kmem_find_block(void *any_addr, void **block_addr, uint64_t *block_size, uint64_t *flags);
// set the flags of an allocated block
int  kmem_set_block_flags(void *block_addr, uint64_t flags);
// apply an mask to all the blocks (and mask unless or=1)
int  kmem_mask_all_blocks_flags(uint64_t mask, int ormask);

// range of addresses used for internal kmem state that should be
// ignored when pointer-chasing the heap, for example in a GC
void kmem_get_internal_pointer_range(void **start, void **end);

// check to see if the masked flags match the given flags
int  kmem_apply_to_matching_blocks(uint64_t mask, uint64_t flags, int (*func)(void *block, void *state), void *state);

int  kmem_sanity_check();

/* KCH: I don't believe the GC implementations support realloc explicitly. 
 * Can ifdef later it if need be
 */
#define realloc(p, s) kmem_realloc(p, s)


#ifdef NAUT_CONFIG_ENABLE_BDWGC
void * GC_memalign(size_t, size_t);
void * GC_malloc(size_t);
#ifdef NAUT_CONFIG_ALIGN_BDWGC
#define malloc(s) ({ NK_MALLOC_PROF_ENTRY(); void *____magic_ptr; size_t __a = 1ULL<<(sizeof(size_t)*8UL - __builtin_clzl(s) - 1); __a <<= !!((s)&(~__a));  ____magic_ptr = GC_memalign(__a,s); NK_MALLOC_PROF_EXIT(); ____magic_ptr; })
#else
#define malloc(s) ({ NK_MALLOC_PROF_ENTRY(); void *____magic_ptr = GC_malloc(s); NK_MALLOC_PROF_EXIT(); ____magic_ptr; })
#endif
#define malloc_specific(s,c) malloc(s)
#define free(a) 
#else
#ifdef NAUT_CONFIG_ENABLE_PDSGC
void *nk_gc_pdsgc_malloc(uint64_t);
void *nk_gc_pdsgc_malloc_specific(uint64_t, int cpu);
#define malloc(s) ({  NK_MALLOC_PROF_ENTRY(); void *____magic_ptr = nk_gc_pdsgc_malloc(s); NK_MALLOC_PROF_EXIT(); ____magic_ptr; })
#ifdef NAUT_CONFIG_EXPLICIT_ONLY_PDSGC
#define free(s) NK_FREE_PROF_ENTRY(); kmem_free(s); NK_FREE_PROF_EXIT();
#else
#define free(s) 
#endif
#define malloc_specific(s,c) nk_gc_pdsgc_malloc_specific(s,c)
#else
#define malloc(s) ({ NK_MALLOC_PROF_ENTRY(); void *____magic_ptr = kmem_malloc(s); NK_MALLOC_PROF_EXIT(); ____magic_ptr; })
#define malloc_specific(s,c) ({ NK_MALLOC_PROF_ENTRY(); void *____magic_ptr = kmem_malloc_specific(s,c,0); NK_MALLOC_PROF_EXIT(); ____magic_ptr; })
#define free(a) NK_FREE_PROF_ENTRY(); kmem_free(a); NK_FREE_PROF_EXIT();
#endif
#endif


/* arch specific */
void arch_detect_mem_map (mmap_info_t * mm_info, mem_map_entry_t * memory_map, unsigned long mbd);
void arch_reserve_boot_regions(unsigned long mbd);


struct kmem_stats {
    uint64_t total_num_pools; // how many memory pools there are
    uint64_t total_blocks_free;
    uint64_t total_bytes_free;
    uint64_t min_alloc_size;
    uint64_t max_alloc_size;
    uint64_t max_pools;  // how many pools can we written in the following
    uint64_t num_pools;   // how many pools were written in the following
    struct buddy_pool_stats pool_stats[0];
};

uint64_t kmem_num_pools();
void     kmem_stats(struct kmem_stats *stats);

#ifdef __cplusplus
}
#endif

#endif /* !__MM_H__! */
