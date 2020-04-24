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
#include <nautilus/nautilus.h>
#include <nautilus/paging.h>
#include <nautilus/naut_string.h>
#include <nautilus/mb_utils.h>
#include <nautilus/idt.h>
#include <nautilus/cpu.h>
#include <nautilus/errno.h>
#include <nautilus/cpuid.h>
#include <nautilus/backtrace.h>
#include <nautilus/macros.h>
#include <nautilus/naut_assert.h>
#include <nautilus/numa.h>
#include <nautilus/mm.h>
#include <lib/bitmap.h>
#include <nautilus/percpu.h>

#ifdef NAUT_CONFIG_XEON_PHI
#include <nautilus/sfi.h>
#endif

#ifdef NAUT_CONFIG_HVM_HRT
#include <arch/hrt/hrt.h>
#endif

#ifdef NAUT_CONFIG_ASPACES
#include <nautilus/aspace.h>
#endif

#ifndef NAUT_CONFIG_DEBUG_PAGING
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif


extern uint8_t boot_mm_inactive;

extern ulong_t pml4;


static char * ps2str[3] = {
    [PS_4K] = "4KB",
    [PS_2M] = "2MB",
    [PS_1G] = "1GB",
};


extern uint8_t cpu_info_ready;

/*
 * align_addr
 *
 * align addr *up* to the nearest align boundary
 *
 * @addr: address to align
 * @align: power of 2 to align to
 *
 * returns the aligned address
 * 
 */
static inline ulong_t
align_addr (ulong_t addr, ulong_t align) 
{
    ASSERT(!(align & (align-1)));
    return (~(align - 1)) & (addr + align);
}


static inline int
gig_pages_supported (void)
{
    cpuid_ret_t ret;
    struct cpuid_amd_edx_flags flags;
    cpuid(CPUID_AMD_FEATURE_INFO, &ret);
    flags.val = ret.d;
    return flags.pg1gb;
}


static page_size_t
largest_page_size (void)
{
    if (gig_pages_supported()) {
        return PS_1G;
    }

    return PS_2M;
}

/*
static int 
drill_pt (pte_t * pt, addr_t addr, addr_t map_addr, uint64_t flags)
{
    uint_t pt_idx = PADDR_TO_PT_IDX(addr);
    addr_t page = 0;

    DEBUG_PRINT("drilling pt, pt idx: 0x%x\n", pt_idx);

    if (PTE_PRESENT(pt[pt_idx])) {

        DEBUG_PRINT("pt entry is present\n");
        page = (addr_t)(pt[pt_idx] & PTE_ADDR_MASK);

    } else {

        DEBUG_PRINT("pt entry not there, creating a new one\n");

        if (map_addr) {

            DEBUG_PRINT("creating manual mapping to paddr: %p\n", map_addr);
            page = map_addr;
            pt[pt_idx] = page | flags;

        } else {
            page = alloc_page();

            if (!page) {
                ERROR_PRINT("out of memory in %s\n", __FUNCTION__);
                return -1;
            }

            DEBUG_PRINT("allocated new page at 0x%x\n", page);

            pt[pt_idx] = page | PTE_PRESENT_BIT | PTE_WRITABLE_BIT;
        }

    }

    return 0;
}
*/



static int 
drill_pd (pde_t * pd, addr_t addr, addr_t map_addr, uint64_t flags)
{
    uint_t pd_idx = PADDR_TO_PD_IDX(addr);
    pte_t * pt = 0;
    addr_t page = 0;

    DEBUG_PRINT("drilling pd, pd idx: 0x%x\n", pd_idx);

    if (PDE_PRESENT(pd[pd_idx])) {

        DEBUG_PRINT("pd entry is present, setting (addr=%p,flags=%x)\n", (void*)map_addr,flags);
        pd[pd_idx] = map_addr | flags | PTE_PAGE_SIZE_BIT | PTE_PRESENT_BIT;
        invlpg(map_addr);

    } else {

        if (map_addr) {

            DEBUG_PRINT("creating manual mapping to paddr: %p\n", map_addr);
            page = map_addr;
            // NOTE: 2MB page assumption
            pd[pd_idx] = page | flags | PTE_PAGE_SIZE_BIT;

        } else {

            panic("trying to allocate 2MB page with no address provided!\n");
#if 0
            DEBUG_PRINT("pd entry not there, creating a new one\n");
            pt = (pte_t*)alloc_page();

            if (!pt) {
                ERROR_PRINT("out of memory in %s\n", __FUNCTION__);
                return -1;
            }

            memset((void*)pt, 0, NUM_PT_ENTRIES*sizeof(pte_t));

            pd[pd_idx] = (ulong_t)pt | PTE_PRESENT_BIT | PTE_WRITABLE_BIT;
#endif
        }
    }

    return 0;
}


static int 
drill_pdpt (pdpte_t * pdpt, addr_t addr, addr_t map_addr, uint64_t flags)
{
    uint_t pdpt_idx = PADDR_TO_PDPT_IDX(addr);
    pde_t * pd = 0;

    DEBUG_PRINT("drilling pdpt, pdpt idx: 0x%x\n", pdpt_idx);

    if (PDPTE_PRESENT(pdpt[pdpt_idx])) {

        DEBUG_PRINT("pdpt entry is present\n");
        pd = (pde_t*)(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    } else {

        DEBUG_PRINT("pdpt entry not there, creating a new page directory\n");
        pd = (pde_t*)mm_boot_alloc_aligned(PAGE_SIZE_4KB, PAGE_SIZE_4KB);
        DEBUG_PRINT("page dir allocated at %p\n", pd);

        if (!pd) {
            ERROR_PRINT("out of memory in %s\n", __FUNCTION__);
            return -EINVAL;
        }

        memset((void*)pd, 0, NUM_PD_ENTRIES*sizeof(pde_t));

        pdpt[pdpt_idx] = (ulong_t)pd | PTE_PRESENT_BIT | PTE_WRITABLE_BIT;

    }

    DEBUG_PRINT("the entry (addr: 0x%x): 0x%x\n", &pdpt[pdpt_idx], pdpt[pdpt_idx]);
    return drill_pd(pd, addr, map_addr, flags);
}


static int 
drill_page_tables (addr_t addr, addr_t map_addr, uint64_t flags)
{
    pml4e_t * _pml4 = (pml4e_t*)read_cr3();
    uint_t pml4_idx = PADDR_TO_PML4_IDX(addr);
    pdpte_t * pdpt  = 0;
    
    if (PML4E_PRESENT(_pml4[pml4_idx])) {

        DEBUG_PRINT("pml4 entry is present\n");
        pdpt = (pdpte_t*)(_pml4[pml4_idx] & PTE_ADDR_MASK);

    } else {

        panic("no PML4 entry!\n");

        DEBUG_PRINT("pml4 entry not there, creating a new one\n");

        pdpt = (pdpte_t*)mm_boot_alloc_aligned(PAGE_SIZE_4KB, PAGE_SIZE_4KB);

        if (!pdpt) {
            ERROR_PRINT("out of memory in %s\n", __FUNCTION__);
            return -EINVAL;
        }

        memset((void*)pdpt, 0, NUM_PDPT_ENTRIES*sizeof(pdpte_t));
        _pml4[pml4_idx] = (ulong_t)pdpt | PTE_PRESENT_BIT | PTE_WRITABLE_BIT;
    }

    DEBUG_PRINT("the entry (addr: 0x%x): 0x%x\n", &_pml4[pml4_idx], _pml4[pml4_idx]);
    return drill_pdpt(pdpt, addr, map_addr, flags);
}


/*
 * nk_map_page
 *
 * @vaddr: virtual address to map to
 * @paddr: physical address to create mapping for 
 *         (must be page aligned)
 * @flags: bits to set in the PTE
 *
 * create a manual page mapping
 * (currently unused since we're using an ident map)
 *
 */
int 
nk_map_page (addr_t vaddr, addr_t paddr, uint64_t flags, page_size_t ps)
{
    if (drill_page_tables(ROUND_DOWN_TO_PAGE(paddr), ROUND_DOWN_TO_PAGE(paddr), flags) != 0) {
        ERROR_PRINT("Could not map page at vaddr %p paddr %p\n", (void*)vaddr, (void*)paddr);
        return -EINVAL;
    }

    return 0;
}


/*
 * nk_map_page_nocache
 *
 * map this page as non-cacheable
 * 
 * @paddr: the physical address to create a mapping for
 *         (must be page aligned)
 * @flags: the flags (besides non-cacheable) to use in the PTE
 *
 * returns -EINVAL on error, 0 on success 
 *
 */
int
nk_map_page_nocache (addr_t paddr, uint64_t flags, page_size_t ps)
{
    if (nk_map_page(paddr, paddr, flags|PTE_CACHE_DISABLE_BIT, ps) != 0) {
        ERROR_PRINT("Could not map uncached page\n");
        return -EINVAL;
    }

    return 0;
}


/*
 * nk_pf_handler
 *
 * page fault handler
 *
 */
int
nk_pf_handler (excp_entry_t * excp,
               excp_vec_t     vector,
               void         * state)
{

    cpu_id_t id = cpu_info_ready ? my_cpu_id() : 0xffffffff;
    uint64_t fault_addr = read_cr2();
    
#ifdef NAUT_CONFIG_HVM_HRT
    if (excp->error_code == UPCALL_MAGIC_ERROR) {
        return nautilus_hrt_upcall_handler(NULL, 0);
    }
#endif

#ifdef NAUT_CONFIG_ASPACES
    if (!nk_aspace_exception(excp,vector,state)) {
	return 0;
    }
#endif

#ifdef NAUT_CONFIG_ENABLE_MONITOR
    int nk_monitor_excp_entry(excp_entry_t * excp,
			      excp_vec_t vector,
			      void *state);

    return nk_monitor_excp_entry(excp, vector, state);
#endif

    printk("\n+++ Page Fault +++\n"
            "RIP: %p    Fault Address: 0x%llx \n"
            "Error Code: 0x%x    (core=%u)\n", 
            (void*)excp->rip, 
            fault_addr, 
            excp->error_code, 
            id);

    struct nk_regs * r = (struct nk_regs*)((char*)excp - 128);
    nk_print_regs(r);
    backtrace(r->rbp);

    panic("+++ HALTING +++\n");
    return 0;
}


/*
 * nk_gpf_handler
 *
 * general protection fault handler
 *
 */
int
nk_gpf_handler (excp_entry_t * excp,
		excp_vec_t     vector,
		void         * state)
{

    cpu_id_t id = cpu_info_ready ? my_cpu_id() : 0xffffffff;

#ifdef NAUT_CONFIG_ASPACES
    if (!nk_aspace_exception(excp,vector,state)) {
	return 0;
    }
#endif

    // if monitor is active, we will fall through to it
    // by calling null_excp_handler
    return null_excp_handler(excp,vector,state);
}


/* don't really use the page size here, unless we get bigger pages 
 * someday
 */
static void
__fill_pml (pml4e_t * pml, 
            page_size_t ps, 
            ulong_t base_addr,
            ulong_t nents, 
            ulong_t flags)
{
    ulong_t i;

    ASSERT(nents <= NUM_PML4_ENTRIES);

    for (i = 0; i < nents; i++) {
        pdpte_t * pdpt = NULL;
        pdpt = mm_boot_alloc_aligned(PAGE_SIZE_4KB, PAGE_SIZE_4KB);
        if (!pdpt) {
            ERROR_PRINT("Could not allocate pdpt\n");
            return;
        }
        memset((void*)pdpt, 0, PAGE_SIZE_4KB);
        pml[i] = (ulong_t)pdpt | flags;
    }

}


static void
__fill_pdpt (pdpte_t * pdpt, 
             page_size_t ps, 
             ulong_t base_addr,
             ulong_t nents,
             ulong_t flags)
{
    ulong_t i;

    ASSERT(nents <= NUM_PDPT_ENTRIES);

    for (i = 0; i < nents; i++) {

        if (ps == PS_1G) {
            pdpt[i] = base_addr | flags | PTE_PAGE_SIZE_BIT;
        } else {
            pde_t * pd = NULL;
            pd = mm_boot_alloc_aligned(PAGE_SIZE_4KB, PAGE_SIZE_4KB);
            if (!pd) {
                ERROR_PRINT("Could not allocate pd\n");
                return;
            }
            memset(pd, 0, PAGE_SIZE_4KB);
            pdpt[i] = (ulong_t)pd | flags;
        }

        base_addr += PAGE_SIZE_1GB;
    }
}

static void
__fill_pd (pde_t * pd, 
           page_size_t ps, 
           ulong_t base_addr,
           ulong_t nents,
           ulong_t flags)
{
    ulong_t i;

    ASSERT(nents <= NUM_PD_ENTRIES);
    ASSERT(ps == PS_2M || ps == PS_4K);

    for (i = 0; i < nents; i++) {

        if (ps == PS_2M) {
            pd[i] = base_addr | flags | PTE_PAGE_SIZE_BIT;
        } else {
            pte_t * pt = NULL;
            pt = mm_boot_alloc_aligned(PAGE_SIZE_4KB, PAGE_SIZE_4KB);
            if (!pt) {
                ERROR_PRINT("Could not allocate pt\n");
                return;
            }
            memset(pt, 0, PAGE_SIZE_4KB);
            pd[i] = (ulong_t)pt | flags;
        }

        base_addr += PAGE_SIZE_2MB;

    }
}


static void
__fill_pt (pte_t * pt, 
           page_size_t ps, 
           ulong_t base_addr,
           ulong_t nents,
           ulong_t flags)
{
    ulong_t i;

    ASSERT(ps == PS_4K);
    ASSERT(nents <= NUM_PT_ENTRIES);

    for (i = 0; i < nents; i++) {
        pt[i] = base_addr | flags;
        base_addr += PAGE_SIZE_4KB;
    }
}

static void
__construct_tables_4k (pml4e_t * pml, ulong_t bytes)
{
    ulong_t npages    = (bytes + PAGE_SIZE_4KB - 1)/PAGE_SIZE_4KB;
    ulong_t num_pts   = (npages + NUM_PT_ENTRIES - 1)/ NUM_PT_ENTRIES;
    ulong_t num_pds   = (num_pts + NUM_PD_ENTRIES - 1)/NUM_PD_ENTRIES;
    ulong_t num_pdpts = (num_pds + NUM_PDPT_ENTRIES - 1)/NUM_PDPT_ENTRIES;
    ulong_t filled_pdpts = 0;
    ulong_t filled_pds   = 0;
    ulong_t filled_pts   = 0;
    ulong_t filled_pgs   = 0;
    unsigned i, j, k;
    ulong_t addr = 0;

    __fill_pml(pml, PS_4K, addr, num_pdpts, PTE_PRESENT_BIT | PTE_WRITABLE_BIT);

    for (i = 0; i < NUM_PML4_ENTRIES && filled_pdpts < num_pdpts; i++) {

        pdpte_t * pdpt = (pdpte_t*)PTE_ADDR(pml[i]);
        ulong_t pdpte_to_fill = ((num_pds - filled_pds) > NUM_PDPT_ENTRIES) ? NUM_PDPT_ENTRIES:
            (num_pds - filled_pds);
        __fill_pdpt(pdpt, PS_4K, addr, pdpte_to_fill, PTE_PRESENT_BIT | PTE_WRITABLE_BIT);

        for (j = 0; j < NUM_PDPT_ENTRIES && filled_pds < num_pds; j++) {

            pde_t * pd = (pde_t*)PTE_ADDR(pdpt[j]);
            ulong_t pde_to_fill = ((num_pts - filled_pts) > NUM_PD_ENTRIES) ? NUM_PD_ENTRIES:
                (num_pts - filled_pts);
            __fill_pd(pd, PS_4K, addr, pde_to_fill, PTE_PRESENT_BIT | PTE_WRITABLE_BIT);

            for (k = 0; k < NUM_PD_ENTRIES && filled_pts < num_pts; k++) {

                pte_t * pt = (pte_t*)PTE_ADDR(pd[k]);

                ulong_t to_fill = ((npages - filled_pgs) > NUM_PT_ENTRIES) ? NUM_PT_ENTRIES : 
                    npages - filled_pgs;

                __fill_pt(pt, PS_4K, addr, to_fill, PTE_PRESENT_BIT | PTE_WRITABLE_BIT);

                filled_pgs += to_fill;
                addr += PAGE_SIZE_4KB*to_fill;

                ++filled_pts;
            }

            ++filled_pds;
        }

        ++filled_pdpts;
    }
}


static void
__construct_tables_2m (pml4e_t * pml, ulong_t bytes)
{
    ulong_t npages    = (bytes + PAGE_SIZE_2MB - 1)/PAGE_SIZE_2MB;
    ulong_t num_pds   = (npages + NUM_PD_ENTRIES - 1)/NUM_PD_ENTRIES;
    ulong_t num_pdpts = (num_pds + NUM_PDPT_ENTRIES - 1)/NUM_PDPT_ENTRIES;
    ulong_t filled_pdpts = 0;
    ulong_t filled_pds   = 0;
    ulong_t filled_pgs   = 0;
    unsigned i, j;
    ulong_t addr = 0;

    __fill_pml(pml, PS_2M, addr, num_pdpts, PTE_PRESENT_BIT | PTE_WRITABLE_BIT);

    for (i = 0; i < NUM_PML4_ENTRIES && filled_pdpts < num_pdpts; i++) {

        pdpte_t * pdpt = (pdpte_t*)PTE_ADDR(pml[i]);
        ulong_t pdpte_to_fill = ((num_pds - filled_pds) > NUM_PDPT_ENTRIES) ? NUM_PDPT_ENTRIES:
            (num_pds - filled_pds);
        __fill_pdpt(pdpt, PS_2M, addr, pdpte_to_fill, PTE_PRESENT_BIT | PTE_WRITABLE_BIT);

        for (j = 0; j < NUM_PDPT_ENTRIES && filled_pds < num_pds; j++) {

            pde_t * pd = (pde_t*)PTE_ADDR(pdpt[j]);

            ulong_t to_fill = ((npages - filled_pgs) > NUM_PD_ENTRIES) ? NUM_PD_ENTRIES : 
                npages - filled_pgs;

            __fill_pd(pd, PS_2M, addr, to_fill, PTE_PRESENT_BIT | PTE_WRITABLE_BIT);

            filled_pgs += to_fill;
            addr += PAGE_SIZE_2MB*to_fill;

            ++filled_pds;
        }

        ++filled_pdpts;
    }

    ASSERT(filled_pgs == npages);
}


static void
__construct_tables_1g (pml4e_t * pml, ulong_t bytes)
{
    ulong_t npages = (bytes + PAGE_SIZE_1GB - 1)/PAGE_SIZE_1GB;
    ulong_t num_pdpts = (npages + NUM_PDPT_ENTRIES - 1)/NUM_PDPT_ENTRIES;
    ulong_t filled_pdpts = 0;
    ulong_t filled_pgs   = 0;
    unsigned i;
    ulong_t addr = 0;

    __fill_pml(pml, PS_1G, addr, num_pdpts, PTE_PRESENT_BIT | PTE_WRITABLE_BIT);

    for (i = 0; i < NUM_PML4_ENTRIES && filled_pdpts < num_pdpts; i++) {

        pdpte_t * pdpt = (pdpte_t*)PTE_ADDR(pml[i]);

        ulong_t to_fill = ((npages - filled_pgs) > NUM_PDPT_ENTRIES) ? NUM_PDPT_ENTRIES : 
            npages - filled_pgs;

        __fill_pdpt(pdpt, PS_1G, addr, to_fill, PTE_PRESENT_BIT | PTE_WRITABLE_BIT);

        filled_pgs += to_fill;
        addr += PAGE_SIZE_1GB*to_fill;

        ++filled_pdpts;
    }
}


static void 
construct_ident_map (pml4e_t * pml, page_size_t ptype, ulong_t bytes)
{
    ulong_t ps = ps_type_to_size(ptype);

    switch (ptype) {
        case PS_4K:
            __construct_tables_4k(pml, bytes);
            break;
        case PS_2M:
            __construct_tables_2m(pml, bytes);
            break;
        case PS_1G:
            __construct_tables_1g(pml, bytes);
            break;
        default:
            ERROR_PRINT("Undefined page type (%u)\n", ptype);
            return;
    }
}

static uint64_t default_cr3;
static uint64_t default_cr4;

/* 
 * Identity map all of physical memory using
 * the largest pages possible
 */
static void
kern_ident_map (struct nk_mem_info * mem, ulong_t mbd)
{
    page_size_t lps  = largest_page_size();
    ulong_t last_pfn = mm_boot_last_pfn();
    ulong_t ps       = ps_type_to_size(lps);
    pml4e_t * pml    = NULL;

    /* create a new PML4 */
    pml = mm_boot_alloc_aligned(PAGE_SIZE_4KB, PAGE_SIZE_4KB);
    if (!pml) {
        ERROR_PRINT("Could not allocate new PML4\n");
        return;
    }
    memset(pml, 0, PAGE_SIZE_4KB);

    printk("Remapping phys mem [%p - %p] with %s pages\n", 
            (void*)0, 
            (void*)(last_pfn<<PAGE_SHIFT), 
            ps2str[lps]);

    construct_ident_map(pml, lps, last_pfn<<PAGE_SHIFT);

    default_cr3 = (ulong_t)pml;
    default_cr4 = (ulong_t)read_cr4();

    /* install the new tables, this will also flush the TLB */
    write_cr3((ulong_t)pml);
    
}

uint64_t nk_paging_default_page_size()
{
    return ps_type_to_size(largest_page_size());
}

uint64_t nk_paging_default_cr3()
{
    return default_cr3;
}

uint64_t nk_paging_default_cr4()
{
    return default_cr4;
}


void
nk_paging_init (struct nk_mem_info * mem, ulong_t mbd)
{
    kern_ident_map(mem, mbd);
}
