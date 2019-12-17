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
#ifndef __PAGING_H__
#define __PAGING_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/naut_types.h>
#include <nautilus/idt.h>
#include <nautilus/printk.h>
#include <nautilus/smp.h>
    
typedef ulong_t pml4e_t;
typedef ulong_t pdpte_t;
typedef ulong_t pde_t;
typedef ulong_t pte_t;


struct nk_pf_error {
    union {
        uint32_t val;
        struct {
            uint8_t p       : 1;
            uint8_t wr      : 1;
            uint8_t usr     : 1;
            uint8_t rsvd_bit: 1;
            uint8_t ifetch  : 1;
            uint32_t rsvd   : 27;
        };
    };
} __packed;


typedef struct nk_pf_error nk_pf_err_t;

#define MEM_ZONE_UC      0x0000000000000001 // no cache
#define MEM_ZONE_WC      0x0000000000000002 // write combine
#define MEM_ZONE_WT      0x0000000000000004 // write through
#define MEM_ZONE_WB      0x0000000000000008 // write back
#define MEM_ZONE_UCE     0x0000000000000010 // no cache + exported + 'fetch & add'
#define MEM_ZONE_WP      0x0000000000001000 // write protected
#define MEM_ZONE_RP      0x0000000000002000 // read protected
#define MEM_ZONE_XP      0x0000000000004000 // NX


typedef enum {
    MEM_USABLE,
    MEM_UNUSABLE,
} mem_zone_type_t;

struct nk_mem_zone {
    mem_zone_type_t type;
    uint64_t start;
    uint64_t length;
    uint64_t attrs;

    struct list_head node;
};


struct nk_mem_info {
    ulong_t * page_map;
    addr_t    pm_start;
    addr_t    pm_end;
    ulong_t   phys_mem_avail;
    ulong_t   npages;

    struct list_head mem_zone_list;
};

typedef enum {
    PS_4K,
    PS_2M,
    PS_1G,
} page_size_t;


int nk_map_page (addr_t vaddr, addr_t paddr, uint64_t flags, page_size_t ps);
int nk_map_page_nocache (addr_t paddr, uint64_t flags, page_size_t ps);
void nk_paging_init(struct nk_mem_info * mem, ulong_t mbd);

int nk_pf_handler(excp_entry_t * excp, excp_vec_t vector, void *state);
int nk_gpf_handler(excp_entry_t * excp, excp_vec_t vector, void *state);

uint64_t nk_paging_default_page_size();
uint64_t nk_paging_default_cr3();
uint64_t nk_paging_default_cr4();
    
#define PAGE_SHIFT_4KB 12UL
#define PAGE_SHIFT_2MB 21UL
#define PAGE_SHIFT_1GB 30UL
#define PAGE_SHIFT     PAGE_SHIFT_2MB
#define PAGE_SIZE_4KB  4096UL
#define PAGE_SIZE_2MB  2097152UL
#define PAGE_SIZE_1GB  1073741824UL
#define PAGE_SIZE      PAGE_SIZE_2MB

#define PAGE_MASK     (~(PAGE_SIZE-1))

#define ROUND_DOWN_TO_PAGE(x) ((x) & PAGE_MASK)

#define PFN_ROUND_UP(x)   (((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_ROUND_DOWN(x) ((x) >> PAGE_SHIFT)

#define MEM_1GB        0x40000000ULL
#define MEM_2MB        0x200000ULL

#define NUM_PT_ENTRIES    512
#define NUM_PD_ENTRIES    512
#define NUM_PDPT_ENTRIES  512
#define NUM_PML4_ENTRIES  512

#define GEN_PT_MASK(x) ((((1ULL<<((x)+9)))-1) & ((~0ULL) << (x)))

#define PML4_SHIFT 39
#define PML4_MASK  GEN_PT_MASK(PML4_SHIFT)

#define PDPT_SHIFT  30
#define PDPT_MASK   GEN_PT_MASK(PDPT_SHIFT)

#define PD_SHIFT   21
#define PD_MASK    GEN_PT_MASK(PD_SHIFT)

#define PT_SHIFT   12
#define PT_MASK    GEN_PT_MASK(PT_SHIFT)

#define PADDR_TO_PML4_IDX(x)   ((PML4_MASK & (x)) >> PML4_SHIFT)
#define PADDR_TO_PDPT_IDX(x) ((PDPT_MASK & (x)) >> PDPT_SHIFT)
#define PADDR_TO_PD_IDX(x)   ((PD_MASK & (x))   >> PD_SHIFT)
#define PADDR_TO_PT_IDX(x)   ((PT_MASK & (x))   >> PT_SHIFT)

#define PAGE_TO_PADDR(n) ((unsigned long long)(n) << PAGE_SHIFT)
#define PADDR_TO_PAGE(n) ((n) >> PAGE_SHIFT)

#define PTE_ADDR_MASK (~((1<<12)-1))

#define PTE_ADDR(x) ((x) & PTE_ADDR_MASK)

#define PTE_PRESENT_BIT       1ULL
#define PTE_WRITABLE_BIT      2ULL
#define PTE_KERNEL_ONLY_BIT   (1ULL<<2)
#define PTE_WRITE_THROUGH_BIT (1ULL<<3)
#define PTE_CACHE_DISABLE_BIT (1ULL<<4)
#define PTE_ACCESSED_BIT      (1ULL<<5)
#define PTE_DIRTY_BIT         (1ULL<<6)
#define PTE_PAGE_SIZE_BIT     (1ULL<<7)
#define PTE_GLOBAL_BIT        (1ULL<<8)
#define PTE_PAT_BIT           (1ULL<<12)
#define PTE_NX_BIT            (1ULL<<63)

#define PML4E_PRESENT(x) ((x) & 1)
#define PML4E_WRITABLE(x) ((x) & 2)
#define PML4E_KERNEL_ONLY(x) ((x) & (1<<3))
#define PML4E_WRITE_THROUGH(x) ((x) & (1<<4))

#define PDPTE_PRESENT(x) ((x) & 1)
#define PDPTE_WRITABLE(x) ((x) & 2)
#define PDPTE_KERNEL_ONLY(x) ((x) & (1<<3))
#define PDPTE_WRITE_THROUGH(x) ((x) & (1<<4))

#define PDE_PRESENT(x) ((x) & 1)
#define PDE_WRITABLE(x) ((x) & 2)
#define PDE_KERNEL_ONLY(x) ((x) & (1<<3))
#define PDE_WRITE_THROUGH(x) ((x) & (1<<4))

#define PTE_PRESENT(x) ((x) & 1)
#define PTE_WRITABLE(x) ((x) & 2)
#define PTE_KERNEL_ONLY(x) ((x) & (1<<3))
#define PTE_WRITE_THROUGH(x) ((x) & (1<<4))
#define PTE_CACHE_DISABLED(x) ((x) & (1<<5))



static inline ulong_t
ps_type_to_size (page_size_t size) {

    switch (size) {
        case PS_4K:
            return PAGE_SIZE_4KB;
        case PS_2M:
            return PAGE_SIZE_2MB;
        case PS_1G:
            return PAGE_SIZE_1GB;
        default:
            break;
    }
    return PAGE_SIZE_2MB;
}


#define PF_ERR_PROT(x)         ((x)&1)       /* fault caused by page-level prot. violation */
#define PF_ERR_NOT_PRESENT(x)  (~(x)&1)      /* fault caused by not-present page */
#define PF_ERR_READ            (~(x)&2)      /* fault was a read */
#define PF_ERR_WRITE           ((x)&2)       /* fault was a write */
#define PF_ERR_SUPER           (~(x)&(1<<3)) /* fault was in supervisor mode */
#define PF_ERR_USR             ((x)&(1<<3))  /* fault was in user mode */
#define PF_ERR_NO_RSVD         (~(x)&(1<<4)) /* fault not caused by rsvd bit violation */
#define PF_ERR_RSVD_BIT        ((x)&(1<<4)   /* fault caused by reserved bit set in page directory */
#define PF_ERR_NO_IFETCH       (~(x)&(1<<5)) /* fault not caused by ifetch */
#define PF_ERR_IFETCH          ((x)&(1<<5))  /* fault caused by ifetch */


#define __page_align __attribute__ ((aligned(PAGE_SIZE_4KB)))

// given a page num, what's it byte offset in the page map
#define PAGE_MAP_OFFSET(n)   (n >> 3)

// given a page num, what's the bit number within the byte
#define PAGE_MAP_BIT_IDX(n)  (n % 8)


/* TODO: we should get this VA offset from the VMM HRT structures */
#ifdef NAUT_CONFIG_HVM_HRT
#define HRT_HIHALF_OFFSET NAUT_CONFIG_HRT_HIHALF_OFFSET
#else
#define HRT_HIHALF_OFFSET 0x0
#endif

/* NOTE: these are identity when HRT not enabled */
/* NOTE: Ran into a bug in GCC 4.8.3 20140911 wherein 
 * it would inline this function, and upon using the value
 * for the APIC base address would truncate it to a 32-bit value!
 */
static addr_t __attribute__((noinline))
va_to_pa (addr_t vaddr) 
{
    return vaddr - HRT_HIHALF_OFFSET;
}

static addr_t __attribute__((noinline))
pa_to_va (addr_t paddr)
{
    return paddr + HRT_HIHALF_OFFSET;
}


#ifdef __cplusplus
}
#endif

#endif
