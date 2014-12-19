#include <nautilus/nautilus.h>
#include <nautilus/paging.h>
#include <nautilus/naut_string.h>
#include <nautilus/mb_utils.h>
#include <nautilus/idt.h>
#include <nautilus/cpu.h>
#include <nautilus/errno.h>
#include <nautilus/backtrace.h>
#include <nautilus/naut_assert.h>
#include <lib/bitmap.h>
#include <lib/liballoc.h>
#include <nautilus/percpu.h>


#ifndef NAUT_CONFIG_DEBUG_PAGING
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif


extern addr_t _loadStart;
extern addr_t _bssEnd;
extern ulong_t pml4;
extern ulong_t pdpt;

/* 
 * KCH BIG NOTE: Most of the following code assumes that we'll
 * be using 2MB pages. This will eventually change, but for now,
 * BE CAREFUL WHEN EDITING THIS CODE! 
 */


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
static ulong_t
align_addr (ulong_t addr, ulong_t align) 
{
    ASSERT(!(align & (align-1)));
    return (~(align - 1)) & (addr + align);
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
        pd = (pde_t*)nk_alloc_page();

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
    pml4e_t * _pml4 = (pml4e_t*)&pml4;
    uint_t pml4_idx = PADDR_TO_PML4_IDX(addr);
    pdpte_t * pdpt  = 0;
    
    if (PML4E_PRESENT(_pml4[pml4_idx])) {

        DEBUG_PRINT("pml4 entry is present\n");
        pdpt = (pdpte_t*)(_pml4[pml4_idx] & PTE_ADDR_MASK);

    } else {

        panic("no PML4 entry!\n");

        DEBUG_PRINT("pml4 entry not there, creating a new one\n");

        pdpt = (pdpte_t*)nk_alloc_page();

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
 * nk_create_page_mapping
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
nk_create_page_mapping (addr_t vaddr, addr_t paddr, uint64_t flags)
{
    /* TODO: implement */
    return -1;
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
nk_map_page_nocache (addr_t paddr, uint64_t flags)
{
    if (drill_page_tables(PAGE_MASK(paddr), PAGE_MASK(paddr), flags|PTE_CACHE_DISABLE_BIT) != 0) {
        ERROR_PRINT("error marking page range uncacheable\n");
        return -EINVAL;
    }
    return 0;
}



/* 
 * nk_free_page
 *
 * free the page pointed to by addr
 *
 * @addr: the address of the page to free
 *        (must be page aligned)
 *
 * returns -EINVAL on error, 0 on success
 *
 */
int
nk_free_page (addr_t addr) 
{
    ASSERT((unsigned long)addr % PAGE_SIZE == 0);
    return nk_free_pages((void*)addr, 1);
}


/* 
 * nk_free_pages
 *
 * free a number of pages 
 *
 * @addr: the address of the starting page (must be page aligned)
 * @num: the number of pages to free (must be power of 2)
 *
 * returns 0 on success
 *
 */
int 
nk_free_pages (void * addr, unsigned num)
{
    struct nk_mem_info * mem = &(nk_get_nautilus_info()->sys.mem);
    uint_t pgnum             = PADDR_TO_PAGE((addr_t)addr);

    ASSERT((unsigned long)addr % PAGE_SIZE == 0);
    ASSERT(!(num & (num-1)));

    bitmap_release_region(mem->page_map, pgnum, num);
    return 0;
}


/*
 * nk_alloc_pages
 *
 * alloc n pages
 *
 * @n: the number of pages to allocate
 *
 * returns NULL on error, the address of the page 
 * on success
 *
 */
addr_t 
nk_alloc_pages (unsigned n)
{
    struct nk_mem_info * mem = &(nk_get_nautilus_info()->sys.mem);
    int order                = get_count_order(n);
    int p                    = bitmap_find_free_region(mem->page_map, mem->npages, order);

    if (p < 0) {
        panic("Could not find %u free pages\n", n);
        return (addr_t)NULL;
    } 

    return PAGE_TO_PADDR(p);
}


/*
 * nk_alloc_page
 *
 * allocate a single page
 *
 */
addr_t 
nk_alloc_page (void) 
{
    return nk_alloc_pages(1);
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
               addr_t         fault_addr)
{
    printk("\n+++ Page Fault +++\nRIP: %p    Fault Address: %p \nError Code: 0x%x    (core=%u)\n", (void*)excp->rip, (void*)fault_addr, excp->error_code, my_cpu_id());

    struct nk_regs * r = (struct nk_regs*)((char*)excp - 128);
    nk_print_regs(r);
    backtrace(r->rbp);

    panic("+++ HALTING +++\n");
    return 0;
}


/*
 * nk_reserve_pages
 *
 * mark a range of pages as unusable by heap allocator
 *
 * @paddr: the address of the first page to start reserving at
 * @n: the number of pages
 *
 * returns -1 on error, 0 on success
 *
 */
int 
nk_reserve_pages (addr_t paddr, unsigned n)
{
    struct nk_mem_info * mem = &(nk_get_nautilus_info()->sys.mem);
    int order = get_count_order(n);
    return bitmap_allocate_region(mem->page_map, PADDR_TO_PAGE(paddr), order);
}


/*
 * nk_reserve_page
 *
 * reserve a single page (cannot be used by heap allocator)
 *
 * @paddr: the start address of the page to reserve
 *
 * returns -1 on error, 0 on success
 *
 */
int
nk_reserve_page (addr_t paddr)
{
    return nk_reserve_pages(paddr, 1);
}


/*
 * nk_reserve_range
 *
 * reserve a range of pages specified by the given addresses
 *
 * @start: address of page to start with
 * @end: end address of the range (this one is rounded up to the next page
 *       boundary)
 *
 * returns -1 on error, 0 on success
 *
 */
int 
nk_reserve_range (addr_t start, addr_t end)
{
    int npages = (end - start) / PAGE_SIZE;
    npages = (end - start) % PAGE_SIZE ? npages + 1 : npages;
    return nk_reserve_pages(PADDR_TO_PAGE(start), npages);
}


/* TODO: make this account for if we need a new pdpt... */
static ulong_t
finish_ident_map (struct nk_mem_info * mem, ulong_t mbd)
{
    addr_t kernel_end     = (addr_t)&_bssEnd;
    ulong_t * pdpt_start  = (ulong_t*)&pdpt;
    ulong_t * pd_start    = 0;
    ulong_t * pd_ptr      = 0;
    ulong_t paddr_start   = (ulong_t)MEM_1GB;
    uint_t num_pdes       = 0;
    uint_t num_pds        = 0;
    int i, j;

    /* make sure not to overwrite multiboot header */
    /* NOTE: maybe ceil the mbd pointer */
    if (mbd >= kernel_end) {
        pd_start = (ulong_t*)(mbd + multiboot_get_size(mbd));
    } else {
        pd_start = (ulong_t*)kernel_end;
    }

    // align the address where I'll lay them out to a page boundary
    pd_start = (ulong_t*)align_addr((ulong_t)pd_start, PAGE_SIZE_4KB);

    pd_ptr = pd_start;

    /* we've already mapped the first 1GB with 1 PD, 512 PDEs */
    num_pdes = (mem->phys_mem_avail >> PAGE_SHIFT) - NUM_PD_ENTRIES;
    num_pds  = (num_pdes % NUM_PD_ENTRIES) ? 
        (num_pdes / NUM_PD_ENTRIES) + 1 : 
        (num_pdes / NUM_PD_ENTRIES);

    for (i = 0; i < num_pds; i++) {

        ASSERT(!((ulong_t)pd_ptr % (ulong_t)PAGE_SIZE_4KB));

        /* point pdpte to new page dir (start at second pdpte)*/
        pdpt_start[i+1] = (ulong_t)pd_ptr | PTE_PRESENT_BIT | PTE_WRITABLE_BIT;

        /* fill in the new page directory */
        for (j = 0; j < NUM_PD_ENTRIES && (i * NUM_PD_ENTRIES + j) < num_pdes; j++) {

            *pd_ptr = paddr_start      | 
                      PTE_PRESENT_BIT  | 
                      PTE_WRITABLE_BIT | 
                      PTE_PAGE_SIZE_BIT;

            paddr_start += MEM_2MB;
            pd_ptr++;

        }

    }

    return (ulong_t)pd_ptr;
}


void
nk_paging_init (struct nk_mem_info * mem, ulong_t mbd)
{
    addr_t kernel_start   = (addr_t)&_loadStart;
    ulong_t page_dir_end  = 0;

    /* how much memory do we have in the machine? */
    mem->phys_mem_avail = multiboot_get_phys_mem(mbd);

    if (mem->phys_mem_avail < MEM_1GB) {
        panic("Not enough memory to run Nautilus!\n");
    } 

    printk("Total Memory Available: %lu KB\n", 
            mem->phys_mem_avail>>10);


    if (!(page_dir_end = finish_ident_map(mem, mbd))) {
        panic("Unable to finish identity map\n");
    }

    mem->pm_start = (addr_t)page_dir_end;
    mem->page_map = (ulong_t*)mem->pm_start;
    mem->npages   = mem->phys_mem_avail >> PAGE_SHIFT;

    // layout the bitmap 
    mem->pm_end = mem->pm_start + (mem->npages / BITS_PER_LONG)*sizeof(long);
    mem->pm_end = (mem->npages % BITS_PER_LONG) ? mem->pm_end + 1 : mem->pm_end;

    memset((void*)mem->pm_start, 0, mem->pm_end - mem->pm_start);

    // set kernel memory + page directories + page frame bitmap as reserved
    printk("Reserving kernel memory %p - %p (page num %d to %d)\n", kernel_start, mem->pm_end-1,PADDR_TO_PAGE(kernel_start), PADDR_TO_PAGE(mem->pm_end-1));
    nk_reserve_range(kernel_start, mem->pm_end);
    
    DEBUG_PRINT("Setting aside system reserved memory\n");
    multiboot_rsv_mem_regions(mem, mbd);

    DEBUG_PRINT("Reserving BDA and Real Mode IVT\n");
    nk_reserve_range((addr_t)0x0, (addr_t)0x4ff);

    DEBUG_PRINT("Reserving Video Memory\n");
    nk_reserve_range((addr_t)0xa0000, (addr_t)0xfffff);

    DEBUG_PRINT("Reserving APIC/IOAPIC\n");
    
    /* TODO: these shouldn't be hardcoded */
    nk_reserve_range((addr_t)0xfec00000, (addr_t)0xfedfffff);

}
