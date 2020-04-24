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


#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/paging.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>

#include <nautilus/aspace.h>

#ifndef NAUT_CONFIG_DEBUG_ASPACE_BASE
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("aspace-base: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("aspace-base: " fmt, ##args)
#define INFO(fmt, args...)   INFO_PRINT("aspace-base: " fmt, ##args)

// There is only a single base address space,
// hence a single global to represent it
static struct nk_aspace_base {
    nk_aspace_t *aspace;
    
    nk_aspace_region_t theregion; // there is only one

    nk_aspace_characteristics_t chars;
    
    uint64_t     cr3; 
#define CR4_MASK 0xb0ULL // bits 4,5,7
    uint64_t     cr4;
} base_state;


static  int destroy(void *state)
{
    ERROR("Cannot destroy the singular base address space\n");
    return -1;
}
    
static int add_thread(void *state)
{
    struct nk_aspace_base *as = (struct nk_aspace_base *)state;
    struct nk_thread *thread = get_cur_thread();

    DEBUG("Add thread %p to base address\n");
    
    return 0;
}
    
    
static int remove_thread(void *state)
{
    struct nk_thread *thread = get_cur_thread();

    thread->aspace = 0;

    return 0;
    
}

static int add_region(void *state, nk_aspace_region_t *region)
{
    ERROR("Cannot add regions to the base address space\n");
    return -1;
}

static int remove_region(void *state, nk_aspace_region_t *region)
{
    ERROR("Cannot remove regions from the base address space\n");
    return -1;
}
    
static int protect_region(void *state, nk_aspace_region_t *region, nk_aspace_protection_t *prot)
{
    ERROR("Cannot protect regions in the base address space\n");
    return -1;
}

static int move_region(void *state, nk_aspace_region_t *cur_region, nk_aspace_region_t *new_region)
{
    ERROR("Cannot move regions in the base address space\n");
    return -1;
}
    
static int switch_from(void *state)
{
    struct nk_aspace_base *as = (struct nk_aspace_base *)state;
    struct nk_thread *thread = get_cur_thread();

    DEBUG("Switching out base address space from thread %d\n",thread->tid);
    
    return 0;
}

static int switch_to(void *state)
{
    struct nk_aspace_base *as = (struct nk_aspace_base *)state;
    struct nk_thread *thread = get_cur_thread();
    uint64_t cr4;
    
    DEBUG("Switching in base address space from thread %d\n",thread->tid);

    write_cr3(as->cr3);
    cr4 = read_cr4();
    cr4 &= ~CR4_MASK;
    cr4 |= as->cr4;
    write_cr4(cr4);
    return 0;
}
    
static int exception(void *state, excp_entry_t *exp, excp_vec_t vec)
{
    struct nk_aspace_base *as = (struct nk_aspace_base *)state;
    struct nk_thread *thread = get_cur_thread();

    DEBUG("Exception 0x%x on thread %d\n",vec,thread->tid);

    panic("PAGE FAULTS CANNOT OCCUR FOR BASE ADDRESS SPACE\n");
    
    return -1;
}
    
static int print(void *state, int detailed)
{
    struct nk_aspace_base *as = (struct nk_aspace_base *)state;
    struct nk_thread *thread = get_cur_thread();

    nk_vc_printf("%s Base Address Space [granularity 0x%lx alignment 0x%lx]\n",
		 as->aspace->name, as->chars.granularity, as->chars.alignment);
    nk_vc_printf("   Region: %016lx - %016lx => %016lx\n"
		 "   CR3:    %016lx  CR4m: %016lx\n",
		 (uint64_t) as->theregion.va_start,
		 (uint64_t) as->theregion.va_start + as->theregion.len_bytes,
		 (uint64_t) as->theregion.pa_start, as->cr3,as->cr4);

    return 0;
}    

static nk_aspace_interface_t base_interface = {
    .destroy = destroy,
    .add_thread = add_thread,
    .remove_thread = remove_thread,
    .add_region = add_region,
    .remove_region = remove_region,
    .protect_region = protect_region,
    .move_region = move_region,
    .switch_from = switch_from,
    .switch_to = switch_to,
    .exception = exception,
    .print = print
};


static int   get_characteristics(nk_aspace_characteristics_t *c)
{
    *c = base_state.chars;
    return 0;
}

static struct nk_aspace * create(char *name, nk_aspace_characteristics_t *c)
{
    ERROR("failed to create new aspace - only one base address space can exist\n");
    return 0;
}


static nk_aspace_impl_t base = {
				.impl_name = "base",
				.get_characteristics = get_characteristics,
				.create = create,
};



//
// The base address space is special in that there
// is exactly one, it does almost nothing, and it is
// installed at start.
//
//

int nk_aspace_base_init()
{
    struct naut_info *info = nk_get_nautilus_info();
    
    memset(&base_state,0,sizeof(base_state));

    // configure region
    base_state.theregion.va_start=0;
    base_state.theregion.pa_start=0;
    base_state.theregion.len_bytes = mm_boot_last_pfn()<<PAGE_SHIFT;

    // configure chars
    base_state.chars.granularity = 
	base_state.chars.alignment = nk_paging_default_page_size();
    
    
    // Capture pointer to the base page tables
    // that were installed at boot and are in current use
    base_state.cr3 = nk_paging_default_cr3();
    base_state.cr4 = nk_paging_default_cr4() & CR4_MASK;

    base_state.aspace = nk_aspace_register("base",0,&base_interface, &base_state);

    if (!base_state.aspace) {
	ERROR("Unable to register base address space\n");
	return -1;
    }

    DEBUG("Base address space configured and initialized\n");
    
    return 0;
}

nk_aspace_register_impl(base);





