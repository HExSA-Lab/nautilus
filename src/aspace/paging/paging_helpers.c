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
#include "paging_helpers.h"

#ifndef NAUT_CONFIG_DEBUG_ASPACE_PAGING
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("aspace-paging: helpers: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("aspace-paging: helpers: " fmt, ##args)
#define INFO(fmt, args...)   INFO_PRINT("aspace-paging: helpers: " fmt, ##args)


#define ALLOC_PHYSICAL_PAGE() malloc(PAGE_SIZE_4KB)
#define FREE_PHYSICAL_PAGE(p) free((void*)p)

int paging_helper_create(ph_cr3e_t *cr3)
{
    ph_pml4e_t *pml4 = ALLOC_PHYSICAL_PAGE();  // must be aligned to 4 KB boundary

    if (!pml4) {
	ERROR("Failed to allocate PML4\n");
	return -1;
    }

    memset(pml4,0,PAGE_SIZE_4KB);

    cr3->val = 0;

    cr3->pml4_base = ADDR_TO_PAGE_NUM_4KB(pml4);

    DEBUG("Allocated PML4 at %p\n", pml4);

    return 0;
    
}

int paging_helper_free(ph_cr3e_t cr3, int free_data)
{
    int i,j,k,l;

    ph_pml4e_t *pml4 = (ph_pml4e_t *)PAGE_NUM_TO_ADDR_4KB(cr3.pml4_base);
    
    for (i=0;i<NUM_PML4E_ENTRIES;i++) {
	if (pml4[i].present) {
	    ph_pdpe_t *pdpe = (ph_pdpe_t *)PAGE_NUM_TO_ADDR_4KB(pml4[i].pdp_base);
	    for (j=0;j<NUM_PDPE_ENTRIES;j++) {
		if (pdpe[j].present) {
		    ph_pde_t *pde = (ph_pde_t *)PAGE_NUM_TO_ADDR_4KB(pdpe[j].pd_base);
		    for (k=0;k<NUM_PDE_ENTRIES;k++) {
			if (pde[k].present) {
			    ph_pte_t *pte = (ph_pte_t *)PAGE_NUM_TO_ADDR_4KB(pde[k].pt_base);
			    if (free_data) { 
				for (l=0;l<NUM_PTE_ENTRIES;l++) {
				    if (pte[l].present) {
					// data page free
					FREE_PHYSICAL_PAGE(PAGE_NUM_TO_ADDR_4KB(pte[l].page_base));
				    }
				}
			    }
			    // page table free
			    FREE_PHYSICAL_PAGE(pte); 
			}
		    }
		    // page directory table free
		    FREE_PHYSICAL_PAGE(pde);
		}
	    }
	    // page directory pointer table free
	    FREE_PHYSICAL_PAGE(pdpe);
	}
    }

    // pml4 table free
    FREE_PHYSICAL_PAGE(pml4);
    return 0;
}


// generic permission check on any page table entry
// returns 1 if permissions ok
int paging_helper_permissions_ok(uint64_t *entry, ph_pf_access_t a)
{
    // all levels treat permissions the same, so we will use the base pte
    ph_pte_t *p = (ph_pte_t *)entry;

    return (p->writable>=a.write) && (p->user>=a.user) && (p->no_exec<a.ifetch);
}

int paging_helper_set_permissions(uint64_t *entry, ph_pf_access_t a)
{
    // all levels treat permissions the same, so we will use the base pte
    ph_pte_t *p = (ph_pte_t *)entry;

    p->writable = a.write;
    p->user = a.user;
    p->no_exec = !a.ifetch;
    return 0;
}

#define perm_ok(p,a) paging_helper_permissions_ok((uint64_t*)p,a)
#define perm_set(p,a) paging_helper_set_permissions((uint64_t*)p,a)

int paging_helper_walk(ph_cr3e_t cr3, addr_t vaddr, ph_pf_access_t access_type, uint64_t **entry)
{
    ph_pml4e_t *pml4 = (ph_pml4e_t *)PAGE_NUM_TO_ADDR_4KB(cr3.pml4_base);
    ph_pml4e_t *pml4e = &pml4[ADDR_TO_PML4_INDEX(vaddr)];
    
    if (pml4e->present && perm_ok(pml4e,access_type)) {
	ph_pdpe_t *pdp = (ph_pdpe_t *)PAGE_NUM_TO_ADDR_4KB(pml4e->pdp_base);
	ph_pdpe_t *pdpe = &pdp[ADDR_TO_PDP_INDEX(vaddr)];
	if (pdpe->present && perm_ok(pdpe,access_type)) {
	    ph_pde_t *pd = (ph_pde_t *)PAGE_NUM_TO_ADDR_4KB(pdpe->pd_base);
	    ph_pde_t *pde = &pd[ADDR_TO_PD_INDEX(vaddr)];
	    if (pde->present && perm_ok(pde,access_type)) {
		ph_pte_t *pt = (ph_pte_t *)PAGE_NUM_TO_ADDR_4KB(pde->pt_base);
		ph_pte_t *pte = &pt[ADDR_TO_PT_INDEX(vaddr)];
		if (pte->present && perm_ok(pte,access_type)) {
		    // success
		    *entry = &pte->val;
		    return 0;
		} else {
		    // failed at 4th level
		    *entry = &pte->val;
		    return 4;
		}
	    } else {
		// failed at 3rd level
		*entry = &pde->val;
		return 3;
	    }
	} else {
	    // failed at 2nd level
	    *entry = &pdpe->val;
	    return 2;
	}			      
    } else {
	// failed at first level
	*entry = &pml4e->val;
	return 1;
    }
}

// bug:  permset at high level needs to be a union the permissions at the lower level...
// also need to revoke permissions if we remove mappings.

int paging_helper_drill(ph_cr3e_t cr3, addr_t vaddr, addr_t paddr, ph_pf_access_t access_type)
{
    //DEBUG("drilling %016lx -> %016lx access=%08x\n", vaddr, paddr, *(uint32_t*)(&access_type));
    ph_pml4e_t *pml4 = (ph_pml4e_t *)PAGE_NUM_TO_ADDR_4KB(cr3.pml4_base);
    ph_pml4e_t *pml4e = &pml4[ADDR_TO_PML4_INDEX(vaddr)];

   
    
    if (pml4e->present) {
	perm_set(pml4e,access_type);
	ph_pdpe_t *pdp = (ph_pdpe_t *)PAGE_NUM_TO_ADDR_4KB(pml4e->pdp_base);
	ph_pdpe_t *pdpe = &pdp[ADDR_TO_PDP_INDEX(vaddr)];
	if (pdpe->present) {
	    perm_set(pdpe,access_type);
	    ph_pde_t *pd = (ph_pde_t *)PAGE_NUM_TO_ADDR_4KB(pdpe->pd_base);
	    ph_pde_t *pde = &pd[ADDR_TO_PD_INDEX(vaddr)];
	    if (pde->present) {
		perm_set(pde,access_type);
		ph_pte_t *pt = (ph_pte_t *)PAGE_NUM_TO_ADDR_4KB(pde->pt_base);
		ph_pte_t *pte = &pt[ADDR_TO_PT_INDEX(vaddr)];
		// does not matter if the PTE is present or not
		// since we are going to update it anyway
		pte->val = 0;
		pte->present = 1;
		perm_set(pte,access_type);
		pte->page_base = ADDR_TO_PAGE_NUM_4KB(paddr);
		return 0;
	    } else {
		// allocate a PT
		ph_pte_t *pt = (ph_pte_t *)ALLOC_PHYSICAL_PAGE();
		if (!pt) {
		    ERROR("Cannot allocate PT\n");
		    return -1;
		}
 		memset(pt,0,PAGE_SIZE_4KB);
		pde->present = 1;
		pde->pt_base = ADDR_TO_PAGE_NUM_4KB(pt);
		// try again
		return paging_helper_drill(cr3,vaddr,paddr,access_type);
	    }
	} else {
	    // allocate a PDT
	    ph_pde_t *pd = (ph_pde_t *)ALLOC_PHYSICAL_PAGE();
	    if (!pd) {
		ERROR("Cannot allocate PDT\n");
		return -1;
	    }
	    memset(pd,0,PAGE_SIZE_4KB);
	    pdpe->present = 1;
	    pdpe->pd_base = ADDR_TO_PAGE_NUM_4KB(pd);
	    // try again
	    return paging_helper_drill(cr3,vaddr,paddr,access_type);
	}
    } else {
	// allocate a PDPT
	ph_pdpe_t *pdp = (ph_pdpe_t *)ALLOC_PHYSICAL_PAGE();
	if (!pdp) {
	    ERROR("Cannot allocate PDPT\n");
	    return -1;
	}
	memset(pdp,0,PAGE_SIZE_4KB);
	pml4e->present = 1;
	pml4e->pdp_base = ADDR_TO_PAGE_NUM_4KB(pdp);
	// try again
	return paging_helper_drill(cr3,vaddr,paddr,access_type);
    }
}
