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
 */
#ifndef __NUMA_H__
#define __NUMA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/list.h>

#define MAX_NUMA_DOMAINS 128

struct numa_domain {
    uint32_t id;
    uint64_t addr_space_size;
    uint32_t num_regions;

    struct list_head regions;

    /* list of other domains, ordered by distance */
    struct list_head adj_list;
};

struct buddy_mempool;

struct mem_reg_entry {
    struct mem_region * mem;
    struct list_head mem_ent;
};

/* for adjacency lists */
struct domain_adj_entry {
    struct numa_domain * domain;
    struct list_head list_ent;
};


struct mem_region {
    uint32_t domain_id;
    uint64_t base_addr;
    uint64_t len;
    uint8_t  enabled;
    uint8_t  hot_pluggable;
    uint8_t  nonvolatile;

    /* used by the kernel memory allocator */
    struct buddy_mempool * mm_state;

    struct list_head entry;

    struct list_head glob_link;
};

struct nk_locality_info {
    uint32_t  num_domains;
    uint8_t * numa_matrix;

    struct numa_domain * domains[MAX_NUMA_DOMAINS];
};

struct cpu;
int nk_cpu_topo_discover(struct cpu* me);
int nk_numa_init(void);
void nk_dump_numa_info(void);
unsigned nk_my_numa_node(void);
unsigned nk_get_num_domains(void);
struct numa_domain * nk_numa_domain_create (struct sys_info * sys, unsigned id);

struct mem_region * nk_get_base_region_by_cpu(cpu_id_t cpu);
struct mem_region * nk_get_base_region_by_num (unsigned num);


struct nk_topo_params {

    // amd
    uint32_t max_ncores;
    uint32_t max_nthreads;

    // intel
	uint32_t core_plus_mask_width;
	uint32_t core_only_select_mask;
	uint32_t core_plus_select_mask;
    uint32_t smt_mask_width;
    uint32_t smt_select_mask;
    uint32_t pkg_select_mask_width;
    uint32_t pkg_select_mask;
};

struct nk_cpu_coords {
    uint32_t smt_id;  // logical CPU ID within core
    uint32_t core_id; // physical core within package
    uint32_t pkg_id;  // package id
};



/* ARCH-SPECIFIC */

int arch_numa_init(struct sys_info* sys);



#ifdef __cplusplus
}
#endif

#endif
