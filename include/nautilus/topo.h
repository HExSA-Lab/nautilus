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
 * Copyright (c) 2020, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __TOPO_H__
#define __TOPO_H__

typedef enum {
	NK_TOPO_ALL_FILT,       // all sibling threads/cpus 
    NK_TOPO_HW_THREAD_FILT, // thread on same hw thread (hyperthread)
    NK_TOPO_PHYS_CORE_FILT, // threads/CPUs on same physical core
    NK_TOPO_SOCKET_FILT,    // threads/CPUs on same socket
} nk_topo_filt_t;

/******* Convenience API for CPU topology ************/
struct cpu;

uint32_t nk_topo_get_smt_id (struct cpu * cpu); // get a core's hyperthread ID (within physical core)
uint32_t nk_topo_get_my_smt_id (void);
uint32_t nk_topo_get_socket_id (struct cpu * cpu); // get this CPU's socket ID
uint32_t nk_topo_get_my_socket_id (void);
uint32_t nk_topo_get_phys_core_id (struct cpu * cpu); // get this CPU's *physical* core ID
uint32_t nk_topo_get_my_phys_core_id (void);
uint8_t  nk_topo_cpus_share_phys_core (struct cpu * a, struct cpu * b); // do these two CPU's share a physical core? (distinct hyperthreads)
uint8_t  nk_topo_same_phys_core_as_me (struct cpu * other);
uint8_t  nk_topo_cpus_share_socket (struct cpu * a, struct cpu * b); // do these two CPU's share a socket?
uint8_t  nk_topo_same_socket_as_me (struct cpu * other);

void nk_topo_map_sibling_cpus (void (func)(struct cpu * cpu, void * state), nk_topo_filt_t filter, void * state);
void nk_topo_map_core_sibling_cpus (void (func)(struct cpu * cpu, void * state), void * state);
void nk_topo_map_socket_sibling_cpus (void (func)(struct cpu * cpu, void * state), void * state);

/************** End CPU topology API *****************/


/******* Convenience API for thread topology ************/
struct nk_thread;

uint8_t nk_topo_threads_share_hwthread (struct nk_thread * a, struct nk_thread * b);
uint8_t nk_topo_thread_shares_hwthread_with_me (struct nk_thread * other);
uint8_t nk_topo_threads_share_core (struct nk_thread * a, struct nk_thread * b);
uint8_t nk_topo_thread_shares_core_with_me (struct nk_thread * other);
uint8_t nk_topo_threads_share_socket (struct nk_thread * a, struct nk_thread * b);
uint8_t nk_topo_thread_shares_socket_with_me (struct nk_thread * other);

void nk_topo_map_sibling_threads(void (func)(struct nk_thread *t, void *state), nk_topo_filt_t filter, void *state);
void nk_topo_map_hwthread_sibling_threads(void (func)(struct nk_thread *t, void *state), void *state);
void nk_topo_map_core_sibling_threads(void (func)(struct nk_thread *t, void *state), void *state);
void nk_topo_map_socket_sibling_threads(void (func)(struct nk_thread *t, void *state), void *state);
/******* End Convenience API for thread topology ************/

#endif 
