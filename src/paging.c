#include <nautilus.h>
#include <paging.h>
#include <string.h>
#include <mb_utils.h>
#include <idt.h>
#include <lib/bitmap.h>

/* KCH TODO: clean this up by getting rid of old bitmap stuff */

#ifndef NAUT_CONFIG_DEBUG_PAGING
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

extern addr_t _loadStart;
extern addr_t _bssEnd;
extern ulong_t pml4;

struct mem_info mem;

static int 
drill_pt (pte_t * pt, addr_t addr) 
{
    uint_t pt_idx = PADDR_TO_PT_IDX(addr);
    DEBUG_PRINT("drilling pt, pt idx: 0x%x\n", pt_idx);
    addr_t page = 0;

    if (PTE_PRESENT(pt[pt_idx])) {
        printk("pt entry is present\n");
        page = (addr_t)(pt[pt_idx] & PTE_ADDR_MASK);
    } else {
        printk("pt entry not there, creating a new one\n");
        page = alloc_page();

        if (!page) {
            printk("out of memory in %s\n", __FUNCTION__);
            return -1;
        }
        printk("allocated new page at 0x%x\n", page);

        // for now, we'll zero out the page
        memset((void*)page, 0, PAGE_SIZE);

        pt[pt_idx] = page | PTE_PRESENT_BIT | PTE_WRITABLE_BIT;
    }

    return 0;
}


static int 
drill_pd (pde_t * pd, addr_t addr) 
{
    uint_t pd_idx = PADDR_TO_PD_IDX(addr);
    printk("drilling pd, pd idx: 0x%x\n", pd_idx);
    pte_t * pt = 0;

    if (PDE_PRESENT(pd[pd_idx])) {
        printk("pd entry is present\n");
        pt = (pte_t*)(pd[pd_idx] & PTE_ADDR_MASK);
    } else {
        printk("pd entry not there, creating a new one\n");
        pt = (pte_t*)alloc_page();

        if (!pt) {
            panic("out of memory in %s\n", __FUNCTION__);
        }

        memset((void*)pt, 0, NUM_PT_ENTRIES*sizeof(pte_t));

        pd[pd_idx] = (ulong_t)pt | PTE_PRESENT_BIT | PTE_WRITABLE_BIT;
    }

    return drill_pt(pt, addr);
}


static int 
drill_pdpt (pdpte_t * pdpt, addr_t addr) 
{
    uint_t pdpt_idx = PADDR_TO_PDPT_IDX(addr);
    printk("drilling pdpt, pdpt idx: 0x%x\n", pdpt_idx);
    pde_t * pd = 0;

    if (PDPTE_PRESENT(pdpt[pdpt_idx])) {
        printk("pdpt entry is present\n");
        pd = (pde_t*)(pdpt[pdpt_idx] & PTE_ADDR_MASK);
    } else {
        printk("pdpt entry not there, creating a new one\n");
        pd = (pde_t*)alloc_page();

        if (!pd) {
            printk("out of memory in %s\n", __FUNCTION__);
            return -1;
        }

        memset((void*)pd, 0, NUM_PD_ENTRIES*sizeof(pde_t));

        pdpt[pdpt_idx] = (ulong_t)pd | PTE_PRESENT_BIT | PTE_WRITABLE_BIT;
    }

    return drill_pd(pd, addr);
}


// TODO: use cr3 for this...
static int 
drill_page_tables (addr_t addr)
{
    pml4e_t * _pml4 = (pml4e_t*)&pml4;
    uint_t pml4_idx = PADDR_TO_PML4_IDX(addr);
    pdpte_t * pdpt  = 0;
    
    if (PML4E_PRESENT(_pml4[pml4_idx])) {
        printk("pml4 entry is present\n");
        pdpt = (pdpte_t*)(_pml4[pml4_idx] & PTE_ADDR_MASK);
    } else {
        printk("pml4 entry not there, creating a new one\n");
        pdpt = (pdpte_t*)alloc_page();
        if (!pdpt) {
            printk("out of memory in %s\n", __FUNCTION__);
            return -1;
        }
        memset((void*)pdpt, 0, NUM_PDPT_ENTRIES*sizeof(pdpte_t));
        _pml4[pml4_idx] = (ulong_t)pdpt | PTE_PRESENT_BIT | PTE_WRITABLE_BIT;
    }

    printk("the entry (addr: 0x%x): 0x%x\n", &_pml4[pml4_idx], _pml4[pml4_idx]);
    return drill_pdpt(pdpt, addr);
}


int
free_page (addr_t addr) 
{
    //ulong_t * bm       = (ulong_t *)mem.page_map;
    uint_t pgnum       = PADDR_TO_PAGE(addr);
    //uint_t word_offset = pgnum % (sizeof(ulong_t)*8);
    //uint_t word_idx    = (pgnum/8)/sizeof(ulong_t);

    //unset_bit(word_offset, &bm[word_idx]);
    bitmap_clear(mem.page_map, pgnum, 1);
    return 0;
}


/* num must be power of 2 */
int 
free_pages (void * addr, int num)
{
    uint_t pgnum       = PADDR_TO_PAGE((addr_t)addr);
    /*
    int i;

    for (i = 0; i < num; i++) {
        free_page((addr_t)(addr+(i*PAGE_SIZE)));
    }

    return 0;
    */
    bitmap_release_region(mem.page_map, pgnum, num);
    return 0;
}


/* num must be power of 2 */
addr_t 
alloc_pages (int num)
{
    return PAGE_TO_PADDR(bitmap_find_free_region(mem.page_map, mem.npages, num));
}


addr_t 
alloc_page (void) 
{
    /*
    int i;
    uint8_t idx  = 0;
    ulong_t * bm = (ulong_t*)mem.page_map;

    for (i = 0; i < (mem.npages/8)/sizeof(ulong_t); i++) {
        // all pages in this word are reserved
        if (~bm[i] == 0) {
            continue;
        } else {
            idx = ff_zero(bm[i]);
            uint_t pgnum = ((i*sizeof(ulong_t))*8) + idx;

            // mark it as reserved 
            set_bit(idx, &bm[i]);

            return PAGE_TO_PADDR(pgnum);
        }
    }


    return (addr_t)NULL;
    */

    return PAGE_TO_PADDR(bitmap_find_free_region(mem.page_map, mem.npages, 1));
}


int
pf_handler (excp_entry_t * excp,
            excp_vec_t     vector,
            addr_t         fault_addr)
{
    printk("Page Fault. Fault addr: 0x%x\n", fault_addr);

    if (drill_page_tables(fault_addr) < 0) {
        printk("ERROR handling page fault\n");
        return -1;
    }

    return 0;
}


void
init_page_frame_alloc (ulong_t mbd)
{
    addr_t kernel_start   = (addr_t)&_loadStart;
    addr_t kernel_end     = (addr_t)&_bssEnd;

    mem.phys_mem_avail = multiboot_get_phys_mem(mbd);

    printk("Total Memory Available: %u KB\n", mem.phys_mem_avail>>10);

    /* make sure not to overwrite multiboot header */
    if (mbd > kernel_end) {
        mem.pm_start = mbd + multiboot_get_size(mbd);
    } else {
        mem.pm_start = kernel_end;
    }

    mem.page_map       = (ulong_t*)mem.pm_start;
    mem.npages         = mem.phys_mem_avail >> PAGE_SHIFT;

    // layout the bitmap 
    //mem.pm_end = mem.pm_start + (mem.npages >> 3);
    // we just always include the extra long word
    mem.pm_end = mem.pm_start + (((mem.npages / BITS_PER_LONG))*sizeof(ulong_t));
    memset((void*)mem.pm_start, 0, mem.pm_end - mem.pm_start);

    // set kernel memory + page frame bitmap as reserved
    printk("Reserving kernel memory (page num %d to %d)\n", PADDR_TO_PAGE(kernel_start), PADDR_TO_PAGE(mem.pm_end-1));
    mark_range_reserved((uint8_t*)mem.page_map, kernel_start, mem.pm_end-1);
    
    printk("Setting aside system reserved memory\n");
    multiboot_rsv_mem_regions(mbd);

    printk("Reserving BDA and Real Mode IVT\n");
    mark_range_reserved((uint8_t*)mem.page_map, 0x0, 0x4ff);

    printk("Reserving Video Memory\n");
    mark_range_reserved((uint8_t*)mem.page_map, 0xa0000, 0xfffff);
}



