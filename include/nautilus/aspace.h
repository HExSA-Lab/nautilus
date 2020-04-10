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
 * Copyright (c) 2019, Peter Dinda
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __NK_ASPACE
#define __NK_ASPACE

#include <nautilus/idt.h>

typedef struct nk_aspace_characteristics {
    uint64_t   granularity;     // smallest unit of control (bytes)
    uint64_t   alignment;       // smallest alignment (bytes)
} nk_aspace_characteristics_t;


// An address space implementation registers it self at compile
// time using the following interface
typedef struct nk_aspace_impl {
    char * impl_name; // name of this type of address apce
    // the characteristics of an implementation is the most
    // constrained (e.g., finest granularity, alignment) aspace it can create
    int   (*get_characteristics)(nk_aspace_characteristics_t *c);
    // create will invoke nk_aspace_register
    struct nk_aspace * (*create)(char *name, nk_aspace_characteristics_t *c);
} nk_aspace_impl_t;

#define nk_aspace_register_impl(impl)			  \
    static struct nk_aspace_impl * _nk_aspace_impl_##impl \
    __attribute__((used))				  \
    __attribute__((unused, __section__(".aspace_impls"),  \
        aligned(sizeof(void*))))                          \
    = &impl;                                              \


typedef struct nk_aspace_protections {
    uint64_t        flags;
#define NK_ASPACE_READ   1
#define NK_ASPACE_WRITE  2
#define NK_ASPACE_EXEC   4
#define NK_ASPACE_PIN    8
#define NK_ASPACE_KERN   16   // meaning "kernel only", which is not yet supported
#define NK_ASPACE_SWAP   32   // meaning "is swaped", which is not yet supported
#define NK_ASPACE_EAGER  64   // meaning the mapping must be immediately constructed
} nk_aspace_protection_t;


// Although a region is defined to map a run of virtual addresses
// to a run of physical addresses, this is an abstraction.
// An aspace implementation uses this as the header for its
// internal region structure
typedef struct nk_aspace_region {
    void       *va_start;
    void       *pa_start;
    uint64_t    len_bytes;
    nk_aspace_protection_t  protect;
} nk_aspace_region_t;



// This is the abstract base class for aspaces
// it should be the first member of any specific aspace interface
typedef struct nk_aspace_interface {
    int    (*destroy)(void *state);
    
    // add and remove thread will be called from the context of the
    // thread that is trying to be added/removed.  Interrupts are off
    int    (*add_thread)(void *state);
    int    (*remove_thread)(void *state);
    
    int    (*add_region)(void *state, nk_aspace_region_t *region);
    int    (*remove_region)(void *state, nk_aspace_region_t *region);
    
    int    (*protect_region)(void *state, nk_aspace_region_t *region, nk_aspace_protection_t *prot);
    int    (*move_region)(void *state, nk_aspace_region_t *cur_region, nk_aspace_region_t *new_region);
    
    // do the work needed to install the address space on the CPU
    // this is invoked on a context switch to a thread that is in a different
    // address space
    int    (*switch_from)(void *state);
    int    (*switch_to)(void *state);
    
    // invoked in exception context if we have registered for pagefaults, gpfs, etc
    int    (*exception)(void *state, excp_entry_t *exp, excp_vec_t vec);
    
    // print out info about the aspace
    int    (*print)(void *state, int detailed);
    
} nk_aspace_interface_t;


#define NK_ASPACE_NAME_LEN 32

//
// This structure forms the header of any type-specific
// structure.   it should be the first member of any aspace state
//
typedef struct nk_aspace {
    uint64_t                  flags;
#define NK_ASPACE_HOOK_PF    1     // invoke me on a page fault
#define NK_ASPACE_HOOK_GPF   2     // invoke me on a general protection fault
    
    char                      name[NK_ASPACE_NAME_LEN];
    
    void                     *state;
    
    nk_aspace_interface_t    *interface;
    
    struct list_head          aspace_list_node;         // for system-wide address space list
    
} nk_aspace_t;


// this is invoked by the create() call in the implementation after it has done its local work
nk_aspace_t *nk_aspace_register(char *name, uint64_t flags, nk_aspace_interface_t *interface, void *state);

// this is invoked by the destory() call in the implementation after it has done its local work
int          nk_aspace_unregister(nk_aspace_t *aspace);

// Given the registered type, returns the characteristics possible
// 0   => success, chars contains characteristics of this kind of type
// -1  => type is not supported
int          nk_aspace_query(char *impl_name, nk_aspace_characteristics_t *chars);

nk_aspace_t *nk_aspace_create(char *impl_name, char *name, nk_aspace_characteristics_t *chars);
int          nk_aspace_destroy(nk_aspace_t *aspace);

nk_aspace_t *nk_aspace_find(char *name);

// move the current thread to a different address space
int          nk_aspace_move_thread(nk_aspace_t *aspace);

int          nk_aspace_add_region(nk_aspace_t *aspace, nk_aspace_region_t *region);
int          nk_aspace_remove_region(nk_aspace_t *aspace, nk_aspace_region_t *region);

// change protections for a region
int          nk_aspace_protect(nk_aspace_t *aspace, nk_aspace_region_t *region, nk_aspace_protection_t *prot);

int          nk_aspace_move_region(nk_aspace_t *aspace, nk_aspace_region_t *cur_region, nk_aspace_region_t *new_region);



// call on BSP after percpu and kmem are available
int          nk_aspace_init();
// call on APs 
int          nk_aspace_init_ap();



// functions that may be used by aspace implementations to avoid shared burdens

// initialize base structure
int          nk_aspace_init_aspace(nk_aspace_t *aspace, char *name);

// called during a context switch then the current aspace will change to the one given
int          nk_aspace_switch(nk_aspace_t *aspace);

// called when a hooked exception occurs
int          nk_aspace_exception(excp_entry_t *entry, excp_vec_t vec, void *priv_data);


int          nk_aspace_dump_aspace_impls();
int          nk_aspace_dump_aspaces(int detail);






#endif
