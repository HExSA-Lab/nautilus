#include <paging.h>
#include <printk.h>
#include <string.h>
#include <mb_utils.h>
#include <idt.h>

extern addr_t _loadStart;
extern addr_t _bssEnd;

addr_t page_map_start, page_map_end;
ullong_t phys_mem_avail, npages;
uint8_t * page_map;


void
free_page (addr_t addr) 
{
    ulong_t * bm       = (ulong_t *)page_map;
    uint_t pgnum       = PADDR_TO_PAGE(addr);
    uint_t word_offset = pgnum % (sizeof(ulong_t)*8);
    uint_t word_idx    = (pgnum/8)/sizeof(ulong_t);

    unset_bit(word_offset, &bm[word_idx]);
}


addr_t 
alloc_page (void) 
{
    int i;
    uint8_t idx  = 0;
    ulong_t * bm = (ulong_t*)page_map;

    for (i = 0; i < (npages/8)/sizeof(ulong_t); i++) {
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
}


int
pf_handler (excp_entry_t * excp,
                 excp_vec_t     vector,
                 addr_t         fault_addr,
                 addr_t         jump_addr)
{
    printk("PAGE FAULT!\n");
    printk("vector: %d\n", (int)vector);
    printk("faulting addr: 0x%x\n", fault_addr);
    printk("jump addr: 0x%x\n", jump_addr);

    printk("\terror code: %x, RIP: 0x%x\n, cs: 0x%x, rflags: 0x%x, rsp: 0x%x, ss: 0x%x\n",
            excp->error_code, excp->rip, excp->cs, excp->rflags, excp->rsp, excp->ss);

    addr_t new = alloc_page();
    printk("allocated a new page at 0x%x\n", new);
    panic("done\n");


    return 0;
}


void
init_page_frame_alloc (ulong_t mbd)
{
    addr_t page_map_start = 0;
    addr_t page_map_end   = 0;
    addr_t kernel_start   = (addr_t)&_loadStart;
    addr_t kernel_end     = (addr_t)&_bssEnd;

    phys_mem_avail = multiboot_get_phys_mem(mbd);

    printk("Total Memory Available: %u KB\n", phys_mem_avail>>10);

    /* make sure not to overwrite multiboot header */
    if (mbd > kernel_end) {
        page_map_start = mbd + multiboot_get_size(mbd);
    } else {
        page_map_start = kernel_end;
    }

    page_map       = (uint8_t*)page_map_start;
    npages         = phys_mem_avail >> PAGE_SHIFT;

    // layout the bitmap 
    page_map_end = page_map_start + (npages >> 3);
    memset((void*)page_map_start, 0, (npages >> 3));

    // set kernel memory + page frame bitmap as reserved
    printk("Reserving kernel memory (page num %d to %d)\n", PADDR_TO_PAGE(kernel_start), PADDR_TO_PAGE(page_map_end-1));
    mark_range_reserved(page_map, kernel_start, page_map_end-1);
    
    printk("Setting aside system reserved memory\n");
    multiboot_rsv_mem_regions(mbd);

    printk("Reserving BDA and Real Mode IVT\n");
    mark_range_reserved(page_map, 0x0, 0x4ff);

    printk("Reserving Video Memory\n");
    mark_range_reserved(page_map, 0xa0000, 0xfffff);
}



