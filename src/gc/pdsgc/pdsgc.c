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
 * Copyright (c) 2017, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

// This is an implementation of conservative garbage collection that
// is specific to the Nautilus kernel internals and is tightly bound
// to how to internal allocator (kmem/buddy) works.

#include <nautilus/nautilus.h>
#include <nautilus/mm.h>
#include <nautilus/thread.h>
#include <nautilus/scheduler.h>
#include <nautilus/backtrace.h>
#include <gc/pdsgc/pdsgc.h>

#define VISITED 0x1

#define GC_STACK_SIZE (4*1024*1024)
#define GC_MAX_THREADS (NAUT_CONFIG_MAX_THREADS*16)

#ifndef NAUT_CONFIG_DEBUG_PDSGC
#define DEBUG(fmt, args...)
#else
#define DEBUG(fmt, args...) DEBUG_PRINT("pdsgc: " fmt,  ##args)
#endif

#define INFO(fmt, args...) INFO_PRINT("pdsgc: " fmt, ##args)
#define WARN(fmt, args...) WARN_PRINT("pdsgc: " fmt,  ##args)
#define ERROR(fmt, args...) ERROR_PRINT("pdsgc: " fmt, ##args)

#define PDSGC_SPECIFIC_STACK_TOPMOST(t) ((t) ? ((void*)((uint64_t)(t)->stack)) : 0 )
#define PDSGC_STACK_TOPMOST() (PDSGC_SPECIFIC_STACK_TOPMOST(get_cur_thread()))

#define PDSGC_SPECIFIC_STACK_TOP(t) ((t) ? ((void*)((uint64_t)(t)->rsp)) : 0 )
#define PDSGC_STACK_TOP() (PDSGC_SPECIFIC_STACK_TOP(get_cur_thread()))

#define PDSGC_SPECIFIC_STACK_BOTTOM(t) ((t) ? ((void*)((uint64_t)(t)->stack + (t)->stack_size - 0)) : 0)
#define PDSGC_STACK_BOTTOM() (PDSGC_SPECIFIC_STACK_BOTTOM(get_cur_thread()))

static void *gc_stack=0;

static uint64_t num_thread_stack_limits;

static struct thread_stack_limits {
    void *start;
    void *end;
    void *top;
} *gc_thread_stack_limits = 0;

static void *kmem_internal_start, *kmem_internal_end;

int  nk_gc_pdsgc_init()
{
    gc_stack = kmem_mallocz(GC_STACK_SIZE);
    if (!gc_stack) {
	ERROR("Failed to allocate GC stack\n");
	return -1;
    } 
    gc_thread_stack_limits = kmem_mallocz(sizeof(struct thread_stack_limits)*GC_MAX_THREADS);
    if (!gc_thread_stack_limits) {
	ERROR("Failed to allocate GC thread stack limits array\n");
	return -1;
    } 
    INFO("init\n");
    return 0;
}

void nk_gc_bdsgc_deinit()
{
    kmem_free(gc_stack);
    kmem_free(gc_thread_stack_limits);
    INFO("deinit\n");
}

void *nk_gc_pdsgc_malloc_specific(uint64_t size, int cpu)
{
    DEBUG("Allocate %lu bytes\n",size);

    // We want a block that has had any old pointers stored
    // in it nuked, hence the zero
    void *block = kmem_malloc_specific(size,cpu,1);

    if (block) { 
	DEBUG("Success - returning %p\n",block);
        //BACKTRACE(DEBUG,4);
	return block;
    } else {
#ifdef NAUT_CONFIG_EXPLICIT_ONLY_PDSGC
	DEBUG("Failed - returning NULL\n");
	return 0;
#else
	DEBUG("Failed - running garbage collection\n");
	if (nk_gc_pdsgc_collect(0)) {
	    ERROR("Garbage collection failed!\n");
	    return 0;
	} else {
	    DEBUG("Garbage collection succeeded, trying again\n");
	    block = kmem_malloc_specific(size,cpu,1);
	    if (block) { 
		DEBUG("Success after garbage  collection - returning %p\n", block);
		//BACKTRACE(DEBUG,4);
		return block;
	    } else {
		DEBUG("Failed even after garbage collection\n");
		return block;
	    }
	}
#endif
    } 
}

void *nk_gc_pdsgc_malloc(uint64_t size)
{
    return nk_gc_pdsgc_malloc_specific(size,-1);
}

// Of course, this linear scan data structure will not scale...

static inline struct thread_stack_limits *is_in_thread_stack(void *addr)
{
    uint64_t i;

    for (i=0;i<num_thread_stack_limits;i++) { 
	if (((addr_t)addr >= (addr_t)gc_thread_stack_limits[i].start) &&
	    ((addr_t)addr < (addr_t)gc_thread_stack_limits[i].end)) {
	    return &(gc_thread_stack_limits[i]);
	}
    }
    return 0;
}

static inline struct thread_stack_limits *is_thread_stack(void *start, void *end)
{
    uint64_t i;

    // too small
    // if we get this wrong, we will simply visit the whole thread stack
    // and hence have overkill in terms of visits
    if ((end-start)<PAGE_SIZE_4KB) { 
	return 0;
    }

    for (i=0;i<num_thread_stack_limits;i++) { 
	if ((start == gc_thread_stack_limits[i].start) &&
	    (end == gc_thread_stack_limits[i].end)) {
	    return &(gc_thread_stack_limits[i]);
	}
    }
    return 0;
}


static void handle_thread_stack_limit(nk_thread_t *t, void *state)
{
    void *start = PDSGC_SPECIFIC_STACK_TOPMOST(t);
    void *top = PDSGC_SPECIFIC_STACK_TOP(t);
    void *end = PDSGC_SPECIFIC_STACK_BOTTOM(t);

    if (num_thread_stack_limits >= GC_MAX_THREADS) { 
	ERROR("Out of room for storing stack limits...\n");
	*(int*)state |= -1;
    } else {
	DEBUG("Thread %lu (%s) has stack limits %p-%p (top %p)\n",
	      t->tid,t->is_idle ? "(*idle*)" : !t->name[0] ? "*unnamed*" : t->name,start,end,top);
	gc_thread_stack_limits[num_thread_stack_limits].start = start;
	gc_thread_stack_limits[num_thread_stack_limits].end = end;
	gc_thread_stack_limits[num_thread_stack_limits].top = top;
	num_thread_stack_limits++;
    }
}

static int capture_thread_stack_limits()
{
    int rc = 0;

    DEBUG("***Handling thread stack limit capture\n");

    num_thread_stack_limits = 0;

    nk_sched_map_threads(-1,handle_thread_stack_limit,&rc);

    if (rc) { 
	ERROR("***Failed to handle some thread stack limit rc=%d!\n",rc);
	return -1;
    } else {
	return 0;
    }
}

static inline int handle_block(void *start, void *end);

static inline int handle_address(void *start)
{
    void *block_addr;
    uint64_t block_size, flags;
    void *approx_rsp = &approx_rsp;

    // short circuit 0 
    if (!start) { 
	return 0;
    }

    if ((addr_t)approx_rsp < (addr_t)(gc_stack+4096)) { 
	ERROR("Ran out of room in the GC stack!\n");
	return -1;
    }

    if (kmem_find_block(start,&block_addr,&block_size,&flags)) { 
	//DEBUG("Skipping address %p as it is non-heap\n",start);
	// not a valid block - skip
	return 0;
    }

    if (flags & VISITED) { 
	DEBUG("Skipping address %p (block %p) as it is already marked\n", start, block_addr);
	return 0;
    }

    if (kmem_set_block_flags(block_addr, flags | VISITED)) {
	ERROR("Failed to set visited on block %p\n", block_addr);
	return -1;
    }

    DEBUG("Visited block %p via address %p - now visiting its children\n", block_addr, start);

    return handle_block(block_addr,block_addr+block_size);
}
    

static inline int handle_block(void *start, void *end)
{
    void *cur;

    DEBUG("Handling block %p-%p\n", start,end);
    
    struct thread_stack_limits *t = is_thread_stack(start,end);

    if (t) { 
	// if it's a thread stack, then consider only the range
	// that is relevant.   Anything past the top of stack is 
	// not to be visited...
	DEBUG("Block %p-%p is thread stack - revising to %p-%p\n", start,end,t->top,end);
	start = t->top;
    }

    if ((addr_t)start%8 || (addr_t)end%8) { 
	ERROR("Block %p-%p is not aligned to a pointer\n");
	return -1;
    }
    
    // If we contain the kmem internal range, we must not scan this
    // since it has pointers to all allocated blocks

    // do we contain the kmem internal range?
    if (((addr_t)kmem_internal_start>=(addr_t)start) &&
	((addr_t)kmem_internal_end<=(addr_t)end)) { 
	
	DEBUG("block %p-%p contains kmem range %p-%p - skipping that range\n",
	      start,end,kmem_internal_start,kmem_internal_end);
	
	for (cur=start;cur<kmem_internal_start;cur+=sizeof(addr_t)) { 
	    if (handle_address(*(void**)cur)) { 
		ERROR("Failed to handle address %p - failing block\n", cur);
		return -1;
	    }
	}
	
	for (cur=kmem_internal_end;cur<end;cur+=sizeof(addr_t)) { 
	    if (handle_address(*(void**)cur)) { 
		ERROR("Failed to handle address %p - failing block\n", cur);
		return -1;
	    }
	}
	
    } else {
	
	for (cur=start;cur<end;cur+=sizeof(addr_t)) { 
	    if (handle_address(*(void**)cur)) { 
		ERROR("Failed to handle address %p - failing block\n", cur);
		return -1;
	    }
	}
    }
	
    return 0;
}

static int mark_gc_state()
{
    void *block_addr;
    uint64_t block_size, flags;

    if (kmem_find_block(gc_stack,&block_addr,&block_size,&flags)) { 
	ERROR("Could not find GC stack?!\n");
	return -1;
    }

    if (kmem_set_block_flags(block_addr, flags | VISITED)) {
	ERROR("Failed to set visited on GC stack\n");
	return -1;
    }

    if (kmem_find_block(gc_thread_stack_limits,&block_addr,&block_size,&flags)) { 
	ERROR("Could not find GC thread stack limits?!\n");
	return -1;
    }

    if (kmem_set_block_flags(block_addr, flags | VISITED)) {
	ERROR("Failed to set visited on GC thread stack limits\n");
	return -1;
    }

    return 0;
}

extern int _data_start, _data_end;

static int handle_data_roots()
{
    DEBUG("***Handling data roots %p-%p\n",&_data_start,&_data_end);
    int rc = handle_block(&_data_start, &_data_end);
    if (rc) {
	ERROR("***Handling data roots failed\n");
    }
    return rc;
}

// we need to handle our thread stack separately
static void handle_thread_stack(nk_thread_t *t, void *state)
{
    void *start = PDSGC_SPECIFIC_STACK_TOP(t);
    void *end = PDSGC_SPECIFIC_STACK_BOTTOM(t);
  
    DEBUG("***Handling thread stack for thread %lu (%s) - %p-%p\n",
	  t->tid,t->is_idle ? "(*idle*)" : !t->name[0] ? "*unnamed*" : t->name,start,end);

    int rc =  handle_block(start,end);

    if (rc) { 
	ERROR("***Handling thread stack failed\n");
    }

    *(int *)state |= rc;
}

static int handle_thread_stack_roots()
{
    int rc = 0;

    DEBUG("***Handling thread stacks\n");

    nk_sched_map_threads(-1,handle_thread_stack,&rc);

    if (rc) { 
	ERROR("***Failed to handle some thread stack rc=%d!\n",rc);
	return -1;
    } else {
	return 0;
    }
}

static uint64_t num_gc=0;
static uint64_t blocks_freed=0;
static struct nk_gc_pdsgc_stats *stats=0;

static int dealloc(void *block, void *state)
{
    void *block_addr;
    uint64_t block_size, flags;

    // sanity check
    if (kmem_find_block(block,&block_addr,&block_size,&flags)) { 
	ERROR("Unable to find block %p on free\n",block);
	return -1;
    }

    DEBUG("Freeing garbage block %p - in block %p (%lu bytes, flags=0x%lx)\n",block,block_addr,block_size,flags);
    
    if (block_addr!=block) {
	ERROR("Deallocation is not using the enclosing block (dealloc %p but enclosing block is %p (%lu bytes)\n", block, block_addr, block_size);
	return -1;
    }

    kmem_free(block);
    blocks_freed++;

    if (stats) { 
	stats->num_blocks++;
	stats->total_bytes += block_size;
	if (block_size < stats->min_block) { stats->min_block=block_size; }
	if (block_size > stats->max_block) { stats->max_block=block_size; }
    }

    return 0;
}


static int leak(void *block, void *state)
{
    void *block_addr;
    uint64_t block_size, flags;

    // sanity check
    if (kmem_find_block(block,&block_addr,&block_size,&flags)) { 
	ERROR("Unable to find block %p on leak detection\n",block);
	return -1;
    }

    if (block_addr!=block) {
	ERROR("Leak detector is not using the enclosing block (leak %p but enclosing block is %p (%lu bytes)\n", block, block_addr, block_size);
	return -1;
    }


    INFO("leaked block %p (%lu bytes, flags=0x%lx)\n",block_addr,block_size,flags);
    blocks_freed++;

    if (stats) { 
	stats->num_blocks++;
	stats->total_bytes += block_size;
	if (block_size < stats->min_block) { stats->min_block=block_size; }
	if (block_size > stats->max_block) { stats->max_block=block_size; }
    }

    return 0;
}

static int  _nk_gc_pdsgc_handle(int (*handle_unvisited)(void *block, void *state), void *state)
{
    nk_sched_stop_world();

    blocks_freed=0;

    kmem_get_internal_pointer_range(&kmem_internal_start,&kmem_internal_end);

    DEBUG("kmem internal range is %p-%p\n",kmem_internal_start, kmem_internal_end);

    if (kmem_mask_all_blocks_flags(~VISITED,0)) { 
	ERROR("Failed to clear visit flags...\n");
	goto out_bad;
    }
    
    // Do not revisit the GC's own state
    if (mark_gc_state()) { 
	ERROR("Failed to mark GC stack....\n");
	return -1;
    }

    if (capture_thread_stack_limits()) { 
	ERROR("Cannot capture thread stack limits\n");
	return -1;
    }
    
    if (handle_data_roots()) {
	ERROR("Failed to handle data segment roots\n");
	goto out_bad;
    }

    if (handle_thread_stack_roots()) { 
	ERROR("Failed to handle thread stack roots\n");
	goto out_bad;
    }

    DEBUG("Now applying dealloc or leak to unvisited blocks\n");
    if (kmem_apply_to_matching_blocks(VISITED,0,handle_unvisited,state)) { 
	ERROR("Failed to complete applying dealloc/leak function\n");
	goto out_bad;
    }

// out_good:
    DEBUG("Pass succeeded - pass %lu freed/detected %lu blocks\n", num_gc, blocks_freed);
    num_gc++;
    nk_sched_start_world();
    return 0;

 out_bad:
    ERROR("Pass failed\n");
    num_gc++;
    nk_sched_start_world();
    return -1;
}


int  _nk_gc_pdsgc_collect()
{
    return _nk_gc_pdsgc_handle(dealloc,0);
}

int  _nk_gc_pdsgc_leak_detect()
{
    return _nk_gc_pdsgc_handle(leak,0);
}


extern int _nk_gc_pdsgc_stack_wrap(void *new_stack_bottom, void *old_stack_bottom_save_loc, int (*func)());

int  nk_gc_pdsgc_collect(struct nk_gc_pdsgc_stats *s)
{
    struct nk_thread *t;
    int rc;

    if (!(t=get_cur_thread())) { 
	ERROR("No current thread\n");
	return -1;
    }

    INFO("Performing garbage collection\n");

    stats = s;
    if (stats) { 
	memset(stats,0,sizeof(*stats));
	stats->min_block = -1;
    }

    rc = _nk_gc_pdsgc_stack_wrap(gc_stack+GC_STACK_SIZE,&t->rsp,_nk_gc_pdsgc_collect);

    if (stats && !stats->num_blocks) { 
	stats->min_block=0;
    }
    
    return rc;
}

int  nk_gc_pdsgc_leak_detect(struct nk_gc_pdsgc_stats *s)
{
    struct nk_thread *t;
    int rc;

    if (!(t=get_cur_thread())) { 
	ERROR("No current thread\n");
	return -1;
    }

    INFO("Performing leak detection\n");

    stats = s;
    if (stats) { 
	memset(stats,0,sizeof(*stats));
	stats->min_block = -1;
    }
	   
    rc = _nk_gc_pdsgc_stack_wrap(gc_stack+GC_STACK_SIZE,&t->rsp,_nk_gc_pdsgc_leak_detect);

    if (stats && !stats->num_blocks) { 
	stats->min_block=0;
    }
    
    return rc;
}

#define NUM_ALLOCS 8

static uint64_t num_bytes_alloced = 0;

int _nk_gc_pdsgc_test_inner()
{
    int i;
    void *p;

    num_bytes_alloced = 0;

    for (i=0;i<NUM_ALLOCS;i++) { 
	// pointer must be in rax
	p = malloc(0x1UL<<(i+8));
	if (p) { 
	    nk_vc_printf("Allocated %lu bytes at %p\n",0x1UL<<(i+8), p);
	    num_bytes_alloced += 0x1UL<<(i+8);
	} else {
	    nk_vc_printf("ALLOCATION FAILURE\n");
	    return -1;
	}
    }

    // clear rax
    return 0; 
    
}


int  nk_gc_pdsgc_test()
{
    int rc;
    struct nk_gc_pdsgc_stats before, leak, collect, after;

    nk_vc_printf("Testing PDSGC - this can take some time!\n");
    
    nk_vc_printf("Doing initial leak analysis\n");
    rc = nk_gc_pdsgc_leak_detect(&before);
    if (rc) { 
	nk_vc_printf("Leak detection failed\n");
	return -1;
    }
    nk_vc_printf("%lu blocks / %lu bytes are already leaked\n",
		 before.num_blocks, before.total_bytes);
    nk_vc_printf("smallest leaked block: %lu bytes, largest leaked block: %lu bytes\n",
		 before.min_block, before.max_block);
    
    nk_vc_printf("Doing leaking allocations\n");


    // Make sure that our call to the allocator does
    // not leave a footprint in registers - calling convention
    // will handle rsp and rbp
    __asm__ __volatile__ ("call _nk_gc_pdsgc_test_inner" : : : 
			  "cc", "memory", "rax", "rbx", "rcx", "rdx",
			  "rsi","rdi","r8","r9","r10","r11", "r12",
			  "r13", "r14", "r15");

    nk_vc_printf("Doing leak analysis after leaking allocations\n");

    rc = nk_gc_pdsgc_leak_detect(&leak);
    if (rc) { 
	nk_vc_printf("Leak detection failed\n");
	return -1;
    }
    nk_vc_printf("%lu blocks / %lu bytes are now leaked\n",
		 leak.num_blocks, leak.total_bytes);
    nk_vc_printf("smallest leaked block: %lu bytes, largest leaked block: %lu bytes\n",
		 leak.min_block, leak.max_block);


    nk_vc_printf("Doing garbage collection\n");

    rc = nk_gc_pdsgc_collect(&collect);
    if (rc) { 
	nk_vc_printf("Colleciion failed\n");
	return -1;
    }
    nk_vc_printf("%lu blocks / %lu bytes were collected\n",
		 collect.num_blocks, collect.total_bytes);
    nk_vc_printf("smallest collected block: %lu bytes, largest collected block: %lu bytes\n",
		 collect.min_block, collect.max_block);
    
    nk_vc_printf("Doing leak analysis after collection\n");

    rc = nk_gc_pdsgc_leak_detect(&after);
    if (rc) { 
	nk_vc_printf("Leak detection failed\n");
	return -1;
    }
    nk_vc_printf("%lu blocks / %lu bytes are now leaked\n",
		 after.num_blocks, after.total_bytes);
    nk_vc_printf("smallest leaked block: %lu bytes, largest leaked block: %lu bytes\n",
		 after.min_block, after.max_block);

    rc = 0;

    if ((leak.num_blocks - before.num_blocks) != NUM_ALLOCS) { 
	nk_vc_printf("Error: intentionally leaked %lu blocks, but %lu blocks were detected\n", NUM_ALLOCS, leak.num_blocks-before.num_blocks);
	rc = -1;
    }

    if ((leak.total_bytes - before.total_bytes) != num_bytes_alloced) {
	nk_vc_printf("Error: intentionally leaked %lu bytes, but %lu bytes were detected\n", num_bytes_alloced, leak.total_bytes-before.total_bytes);
	rc = -1;
    }

    if (before.num_blocks != after.num_blocks) { 
	nk_vc_printf("Before/after numblocks do not match\n");
	rc = -1;
    }

    if (before.total_bytes != after.total_bytes) { 
	nk_vc_printf("Before/after byte totals do not match\n");
	rc = -1;
    }

    return rc;
}
