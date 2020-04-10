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

#ifndef __PAGING_HELPERS_H__
#define __PAGING_HELPERS_H__


/*
  All abstractions provide here are for long mode paging
  with basic 4-level page tables.   Good documentation
  for such page is in AMD volume 2, 5.3 and on, or in the
  equivalent Intel documentation in volume 3.

  The levels are named (from the root):

  PML4T  "page map level-4 table"
  PDPT   "page directory pointer table"
  PDT    "page directory table"
  PT     "page table"

  Their corresponding entries are:

  PML4E
  PDPE
  PDE
  PTE

  Yes, this is a crazy naming convention.   


*/


// Length of each level of page table
#define NUM_PML4E_ENTRIES        512
#define NUM_PDPE_ENTRIES         512
#define NUM_PDE_ENTRIES          512
#define NUM_PTE_ENTRIES          512


// tools to convert from an address to the index at each level of the
// page table hierarchy
#define ADDR_TO_PML4_INDEX(x) ((((addr_t)x) >> 39) & 0x1ff)
#define ADDR_TO_PDP_INDEX(x)  ((((addr_t)x) >> 30) & 0x1ff)
#define ADDR_TO_PD_INDEX(x)   ((((addr_t)x) >> 21) & 0x1ff)
#define ADDR_TO_PT_INDEX(x)   ((((addr_t)x) >> 12) & 0x1ff)


// tool to convert from a virtual or physical address to the page
// number it is on (for default base page size of 4KB)
#define ADDR_TO_PAGE_NUM(x)       ((addr_t)(x) >> 12)
#define ADDR_TO_PAGE_NUM_4KB(x)   ((addr_t)(x) >> 12)
// similar tools for conversion to the page number of larger pages
// (large page, huge page, possible next gen giant page)
#define ADDR_TO_PAGE_NUM_2MB(x)   ((addr_t)(x) >> 21)
#define ADDR_TO_PAGE_NUM_1GB(x)   ((addr_t)(x) >> 30)
#define ADDR_TO_PAGE_NUM_512GB(x) ((addr_t)(x) >> 39)

// tool to convert from a page number to the address
// at which the page begins (default page size of 4KB)
#define PAGE_NUM_TO_ADDR(x)       (((addr_t)x) << 12)
#define PAGE_NUM_TO_ADDR_4KB(x)   (((addr_t)x) << 12)
// similar tools to convert from a page number to the address at which
// the page begins for larger pages (large, huge, giant)
#define PAGE_NUM_TO_ADDR_2MB(x)   (((addr_t)x) << 21)
#define PAGE_NUM_TO_ADDR_1GB(x)   (((addr_t)x) << 30)
#define PAGE_NUM_TO_ADDR_512GB(x) (((addr_t)x) << 39)

// tool to extract the page offset from an address (4 KB)
#define ADDR_TO_OFFSET(x)       ((x) & 0xfff)
#define ADDR_TO_OFFSET_4KB(x)   ((x) & 0xfff)
// and for larger pages (large, huge, giant)
#define ADDR_TO_OFFSET_2MB(x)   ((x) &     0x1fffff)
#define ADDR_TO_OFFSET_1GB(x)   ((x) &   0x3fffffff)
#define ADDR_TO_OFFSET_512GB(x) ((x) & 0x7fffffffff)


// Determine the byte address of first byte of the page
// containing the giben address
#define PAGE_ADDR(x)       (((x) >> PAGE_SHIFT) << PAGE_SHIFT)
#define PAGE_ADDR_4KB(x)   (((x) >> PAGE_SHIFT_4KB) << PAGE_SHIFT_4KB)
#define PAGE_ADDR_2MB(x)   (((x) >> PAGE_SHIFT_2MB) << PAGE_SHIFT_2MB)
#define PAGE_ADDR_1GB(x)   (((x) >> PAGE_SHIFT_1GB) << PAGE_SHIFT_1GB)
#define PAGE_ADDR_512GB(x) (((x) >> PAGE_SHIFT_512GB) << PAGE_SHIFT_512GB)


// In the following "ph_" is used as a type prefix to avoid coliding
// with the regular paging types defined in include/nautilus/paging.h
// We are providing clearer/simpler types here to make the development
// effort easier

// cr3 register
typedef union ph_cr3e {
    uint64_t val;
    struct {
	uint_t res1           : 3;  // reserved must be zero
	uint_t write_through  : 1;  // legacy cache control - set to zero
	uint_t cache_disable  : 1;  // legacy cache control - set to zero
	uint_t res2           : 7;  // reserved must be zero
	uint64_t pml4_base    : 40; // pointer to pml4e (bits 12..51, bits 0..11 are zero)
	uint_t res3           : 12; // reserved must be zero
    } __attribute__((packed));
} __attribute__((packed)) ph_cr3e_t;
typedef ph_cr3e_t ph_ptbre_t;

// page table entries
typedef union ph_pml4e {
    uint64_t  val;
    struct {
	uint_t present        : 1;  
	uint_t writable       : 1;  
	uint_t user           : 1;  // 1 => allow user-mode access
	uint_t write_through  : 1;  // legacy cache control - set to zero
	uint_t cache_disable  : 1;  // legacy cache control - set to zero
	uint_t accessed       : 1;  // HW writes to 1 when page accessed
	uint_t reserved       : 1;  // not used, set to zero
	uint_t zero           : 2;  // must be zero
	uint_t avail1         : 3;  // can be used as desired by kernel
	uint64_t pdp_base     : 40; // pointer to PDPT (bits 12..51, bits 0..11 are zero)
	uint_t avail2         : 11; // can be used as desired by kernel
	uint_t no_exec        : 1;  // 1 => cannot fetch instructions from page
    } __attribute__((packed));
} __attribute__((packed)) ph_pml4e_t;


typedef union ph_pdpe {  // mostly the same as pml4e, comments show diffs
    uint64_t val;
    struct {
	uint_t present        : 1;
	uint_t writable       : 1;
	uint_t user           : 1;
	uint_t write_through  : 1;
	uint_t cache_disable  : 1;
	uint_t accessed       : 1;
	uint_t reserved       : 1;
	uint_t zero           : 2;  // must be zero
	                            // bit 7 set makes this a huge page
	uint_t avail1         : 3;   
	uint64_t pd_base      : 40; // pointer to PDT (bits 12..51, bits 0..11 are zero)
	uint_t avail2         : 11;
	uint_t no_exec        : 1;
    } __attribute__((packed));
} __attribute__((packed)) ph_pdpe_t;


typedef union ph_pde {  // mostly the same as pdpe, comments show diffs
    uint64_t val;
    struct {
	uint_t present         : 1;
	uint_t writable        : 1;
	uint_t user            : 1;
	uint_t write_through   : 1;
	uint_t cache_disable   : 1;
	uint_t accessed        : 1;
	uint_t reserved        : 1;
	uint_t zero            : 2;  // must be zero
	                             // bit 7 makes this a large page
	                             // bit 8 makes this a global page
	uint_t avail1          : 3;
	uint64_t pt_base       : 40; // pointer to PT (bits 12..51, bits 0..11 are zero)
	uint_t avail2          : 11;
	uint_t no_exec         : 1;
    } __attribute__((packed));
} __attribute__((packed)) ph_pde_t;

typedef union ph_pte {  // mostly the same as a pde, comments show diffs
    uint64_t val;
    struct {
	uint_t present         : 1;
	uint_t writable        : 1;
	uint_t user            : 1;
	uint_t write_through   : 1;
	uint_t cache_disable   : 1;
	uint_t accessed        : 1;
	uint_t dirty           : 1;  // set to 1 by hw when this page is writen
	uint_t pat             : 1;  // 1 => one bit of cache attribute info (set to zero!)
	uint_t global          : 1;  // 1 => global page (do not flush when TLB is flushed)
	uint_t avail1          : 3;
	ullong_t page_base     : 40; // physical page number (
	uint_t avail2          : 11;
	uint_t no_exec         : 1;
    } __attribute__((packed));
} __attribute__((packed)) ph_pte_t;


// page fault error code deconstruction
typedef struct ph_pf_error {
    uint_t present           : 1; // if 0, fault due to page not present
    uint_t write             : 1; // if 1, faulting access was a write
    uint_t user              : 1; // if 1, faulting access was in user mode
    uint_t rsvd_access       : 1; // if 1, fault from reading a 1 from a reserved field (?)
    uint_t ifetch            : 1; // if 1, faulting access was an instr fetch (only with NX)
    uint_t rsvd              : 27;
} __attribute__((packed)) ph_pf_error_t;

// for access use, present is ignored, write=>writeable, user=>user allowed, ifetch=>ifetch ok
// ie, it reflects the maximum permissions
typedef ph_pf_error_t ph_pf_access_t;



// create a new page table hierarchy, returning a cr3e
// this simply creates an empty PML4T
int paging_helper_create(ph_cr3e_t *cr3);

// delete a whole page table hierachy from a cr3
// if free_data, then data pages will also be freed (you probably do not want this)
int paging_helper_free(ph_cr3e_t cr3, int free_data);

int paging_helper_permissions_ok(uint64_t *entry, ph_pf_access_t a);
int paging_helper_set_permissions(uint64_t *entry, ph_pf_access_t a);

// walk page table as if we were the hardware doing an access of the given type
// return -1 if walk results in error
// return 0 if walk is successful, *pte points to succeeding last level PTE
// return 1 if walk is unsuccessful, *pte points to failing PML4 entry
// reutrn 2 if walk is unsucesssful, *pte points to failing PDPE entry
// return 3 if walk is unsuccessful, *pte points to failing PDE entry
// return 4 if walk is unsuccessful, *pte points to failing PTE entry
int paging_helper_walk(ph_cr3e_t cr3, addr_t vaddr, ph_pf_access_t access_type, uint64_t **entry);

// build a path through the PT hierarchy to enable an access of the given type
int paging_helper_drill(ph_cr3e_t cr3, addr_t vaddr, addr_t paddr, ph_pf_access_t access_type);



#endif
