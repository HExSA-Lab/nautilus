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

#ifndef __PDSGC__
#define __PDSGC__

int  nk_gc_pdsgc_init();
void nk_gc_bdsgc_deinit();

// A malloc that requires a GC will stop the world
// for the duration
void *nk_gc_pdsgc_malloc(uint64_t size);
void *nk_gc_pdsgc_malloc_specific(uint64_t size, int cpu);

struct nk_gc_pdsgc_stats
{
    uint64_t num_blocks;
    uint64_t total_bytes;
    uint64_t min_block;
    uint64_t max_block;
};

// Note that all the following functions stop the world
// for the duration.  

// Force a GC -r returns 0 if successful
int  nk_gc_pdsgc_collect(struct nk_gc_pdsgc_stats *stats);

// Do leak detection
int  nk_gc_pdsgc_leak_detect(struct nk_gc_pdsgc_stats *stats);

// Assorted tests
int  nk_gc_pdsgc_test();

#endif
