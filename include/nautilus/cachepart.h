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
 * Copyright (c) 2018, Vyas Alwar
 * Copyright (c) 2018, Qingtong Guo
 * Copyright (c) 2018, Peter Dinda
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Vyas Alwar <valwar@math.northwestern.edu>
 *          Qingtong Guo <QingtongGuo2019@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef _CACHE_PART 
#define _CACHE_PART

// always provided for use in thread.h and thread_lowlevel.S
typedef uint16_t  nk_cache_part_thread_state_t;

#ifdef NAUT_CONFIG_CACHEPART

#include <nautilus/group.h>

// a cache partition as seen externally, so it can be passed from thread
// to thread opaquely
typedef uint16_t nk_cache_part_t;

// default partitions used throughout the system
// thread default MUST be zero so that thread creation chooses this naturally
// if interrupt partitioning is enabled, same for interrupt default - MUST be 1
#define NK_CACHE_PART_THREAD_DEFAULT    0  
#define NK_CACHE_PART_INTERRUPT_DEFAULT 1


// Call early in boot process on BSP and APs (currently after scheduler init)
// Returns -1 if cache partitioning is not supported or cannot be enabled
// Memory allocator must be functional
int nk_cache_part_init(uint32_t percent_thread_default, uint32_t percent_interrupt);
int nk_cache_part_init_ap();
    
// Call late on BSP and APs (currently after scheduler start)
// Returns -1 if not supported or cannot be enabled
// If successful, all threads are now running the default thread partition
// and all interrupts are running in the default interrupt partition
int nk_cache_part_start();
int nk_cache_part_start_ap();

// Get the current thread's cache partition
nk_cache_part_t nk_cache_part_get_current();

// Acquire a percentage of the cache for the calling thread
// zero ==> success
// You can get the cache partition using nk_cache_part_get_current()
int nk_cache_part_acquire(uint32_t percent);

// Share some existing partition
// this increments refcount for the partition
int nk_cache_part_select(nk_cache_part_t part);

// Release the cache partition that was either acquired or selected
// refcounts down the partition, frees at zero
int nk_cache_part_release(nk_cache_part_t part);


// thread group versions of the above
// nonzero return value means someone failed, and everyone
// has been reverted to the thread default
int nk_cache_part_group_acquire(nk_thread_group_t *group, uint32_t percent);
int nk_cache_part_group_select(nk_thread_group_t *group, nk_cache_part_t part);
int nk_cache_part_group_release(nk_thread_group_t *group, nk_cache_part_t part);

//Free the cache and corresponding resources
int nk_cache_part_deinit();

// dump out our state
void nk_cache_part_dump();

#endif

#endif
