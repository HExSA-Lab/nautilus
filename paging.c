#include <paging.h>
#include <printk.h>
#include <types.h>
#include <mb_utils.h>

extern addr_t _bssEnd;
addr_t page_map_start, page_map_end;
unsigned long long phys_mem_avail;
unsigned long long npages;


// TODO: we should eventually copy the multiboot info somewhere into our kernel
void
init_page_alloc (unsigned long mbd)
{
    addr_t page_map_start; 
    addr_t page_map_end;

    phys_mem_avail = get_phys_mem(mbd);
    printk("Total Memory Available: %u KB\n", phys_mem_avail>>10);

    page_map_start = _bssEnd;
    npages         = phys_mem_avail>>PAGE_SHIFT;
    page_map_end   = page_map_start + (npages>>3);


}
