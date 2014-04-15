#include <paging.h>
#include <printk.h>
#include <types.h>
//#include <list.h>

extern unsigned long long phys_mem_avail;
extern addr_t _bssEnd;

addr_t page_map_start, page_map_end;
unsigned long long npages;

void
init_page_alloc (void)
{
    //unsigned i;
    addr_t page_map_start; 
    addr_t page_map_end;

    page_map_start = _bssEnd;
    npages         = phys_mem_avail>>PAGE_SHIFT;
    page_map_end   = page_map_start + (npages>>3);
    printk("number of physical page frames: %u\n", npages);


}
