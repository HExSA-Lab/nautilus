#include <paging.h>
#include <printk.h>
#include <mb_utils.h>

extern addr_t _loadStart;
extern addr_t _bssEnd;

addr_t page_map_start, page_map_end;
ullong_t phys_mem_avail, npages;
uint8_t * page_map;

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

    // set kernel memory + page frame bitmap as reserved
    printk("Reserving kernel memory\n");
    mark_range_reserved(page_map, kernel_start, page_map_end-1);
    
    printk("Setting aside system reserved memory\n");
    multiboot_rsv_mem_regions(mbd);
}


inline void 
rsv_page_frame (addr_t addr) 
{
    uint_t page_num   = ADDR_TO_PAGE_NUM(addr);
    uint_t pm_idx     = PAGE_MAP_OFFSET(page_num);
    page_map[pm_idx] |= (1<<PAGE_MAP_BIT_IDX(page_num));
}


inline void 
free_page_frame (addr_t addr)
{
    uint_t page_num   = ADDR_TO_PAGE_NUM(addr);
    uint_t pm_idx     = PAGE_MAP_OFFSET(page_num);
    page_map[pm_idx] &= ~(1<<PAGE_MAP_BIT_IDX(page_num));
}
