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
#include <nautilus/naut_types.h>
#include <nautilus/mm.h>
#include <nautilus/paging.h>
#include <nautilus/mb_utils.h>
#include <nautilus/multiboot2.h>
#include <nautilus/macros.h>
#include <lib/bitmap.h>

#define CACHE_LINE_SIZE_DEFAULT 64

#ifndef NAUT_CONFIG_DEBUG_BOOTMEM
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define BMM_DEBUG(fmt, args...) DEBUG_PRINT("BOOTMEM: " fmt, ##args)
#define BMM_PRINT(fmt, args...) printk("BOOTMEM: " fmt, ##args)
#define BMM_WARN(fmt, args...)  WARN_PRINT("BOOTMEM: " fmt, ##args)

char * mem_region_types[6] = {
    [0]                                 = "unknown",
    [MULTIBOOT_MEMORY_AVAILABLE]        = "usable RAM",
    [MULTIBOOT_MEMORY_RESERVED]         = "reserved",
    [MULTIBOOT_MEMORY_ACPI_RECLAIMABLE] = "ACPI reclaimable",
    [MULTIBOOT_MEMORY_NVS]              = "non-volatile storage",
    [MULTIBOOT_MEMORY_BADRAM]           = "bad RAM",
};


extern addr_t _loadStart;
extern addr_t _bssEnd;
extern ulong_t pml4;
extern ulong_t boot_stack_start;

static mem_map_entry_t memory_map[MAX_MMAP_ENTRIES];
static mmap_info_t mm_info;
static boot_mem_info_t bootmem;

uint8_t boot_mm_inactive = 0;


/* 
 * Sifts through the memory map
 * and records each region in the kernel's memory map
 */
static inline void
detect_mem_map (unsigned long mbd)
{
    arch_detect_mem_map(&mm_info, memory_map, mbd);
}


struct mem_map_entry * 
mm_boot_get_region (unsigned i) 
{
    if (i >= mm_info.num_regions) {
        return NULL;
    }
    return &memory_map[i];
}


unsigned 
mm_boot_num_regions (void) 
{
    return mm_info.num_regions;
}

static int is_usable_ram(uint64_t start, uint64_t len)
{
    uint64_t i;
    
    for (i=0;i<mm_info.num_regions;i++) { 
	if ((start >= memory_map[i].addr) &&
	    (start+len <= (memory_map[i].addr+memory_map[i].len))) {
	    return memory_map[i].type==MULTIBOOT_MEMORY_AVAILABLE;
	}
    }

    return 0;
}


/* returns the very last page frame number 
 * that should be mapped by the paging subsystem
 */
ulong_t 
mm_boot_last_pfn (void)
{
    return mm_info.last_pfn;
}


/*
 * returns amount of *usable* system RAM in bytes
 */
uint64_t 
mm_get_usable_ram (void)
{
    return mm_info.usable_ram;
}


/*
 * returns *all* memory bytes, including I/O holes, 
 * unusable memory, ACPI reclaimable etc.
 */
uint64_t
mm_get_all_mem (void)
{
    return mm_info.total_mem;
}


static void
free_usable_ram (boot_mem_info_t * mem)
{
    unsigned i;

    for (i = 0; i < mm_info.num_regions; i++) {
        
        // if we encounter a region smaller than 2MB, we'll just avoid
        // the trouble of adding it here. Otherwise we'll need to support
        // either a smarter page map or a per-size bitmap. Perhaps in the future.
        if (memory_map[i].type == MULTIBOOT_MEMORY_AVAILABLE && 
            memory_map[i].len >= PAGE_SIZE) {
            BMM_DEBUG("Freeing memory region @[%p - %p]\n", memory_map[i].addr, memory_map[i].addr + memory_map[i].len);
            mm_boot_free_mem(memory_map[i].addr, memory_map[i].len);
        } else {
            BMM_DEBUG("  *skipping* memory region @[%p - %p] (type=%s)\n", memory_map[i].addr, memory_map[i].addr + memory_map[i].len, mem_region_types[memory_map[i].type]);
        }

    }
}

void
mm_dump_page_map (void) 
{
	ulong_t i = 0;

	BMM_PRINT("Dumping page map:\n");
	for (i = 0; i < bootmem.npages/BITS_PER_LONG; i++) {
		printk("  [%016llx]\n", bootmem.page_map[i]);
	}
}

int 
mm_boot_init (ulong_t mbd)
{
    addr_t kern_start     = (addr_t)&_loadStart;
    addr_t kern_end       = multiboot_get_modules_end(mbd);
    addr_t pm_start       = round_up(kern_end, PAGE_SIZE);
    boot_mem_info_t * mem = &bootmem;
    ulong_t npages;
    ulong_t pm_len;

    BMM_PRINT("Setting up boot memory allocator\n");

    /* parse the multiboot2 memory map, filing in 
     * some global data that we will subsequently use here  */
    detect_mem_map(mbd);


    npages = mm_info.last_pfn + 1;
    pm_len = (npages/BITS_PER_LONG + !!(npages%BITS_PER_LONG)) * sizeof(long);

    BMM_PRINT("Detected %llu.%llu MB (%lu pages) of usable System RAM\n", mm_info.usable_ram/(1024*1024), mm_info.usable_ram%(1024*1024), npages);

    mem->page_map = (ulong_t*)pm_start;
    mem->npages   = npages;
    mem->pm_len   = pm_len;

    /* everything is initially marked as reserved */
    memset((void*)mem->page_map, 0xff, mem->pm_len);

    /* free up the system RAM  that we can use */
    free_usable_ram(mem);
    
    /* mark as used the kernel and the pages occupying the bitmap */
    uint64_t kern_size = pm_start + pm_len - kern_start;
    kern_size = (PAGE_SIZE <= kern_size) ?  kern_size : PAGE_SIZE;

#ifdef NAUT_CONFIG_HVM_HRT
    mm_boot_reserve_vmem(kern_start, kern_size);
#else
    mm_boot_reserve_mem(kern_start, kern_size);
#endif

    /* reserve the zero page */
    mm_boot_reserve_mem(0, PAGE_SIZE);

    arch_reserve_boot_regions(mbd);

    return 0;
}



void
mm_boot_reserve_mem (addr_t start, ulong_t size)
{
    uint32_t start_page = PADDR_TO_PAGE(start);
    size += start - PAGE_TO_PADDR(start_page);
    uint32_t npages     = (size+PAGE_SIZE-1) / PAGE_SIZE;
    boot_mem_info_t * bm = &bootmem;

    BMM_DEBUG("Reserving memory %p-%p (start page=0x%x, npages=0x%x)\n",start,start+size,start_page,npages);

    if (unlikely(boot_mm_inactive)) {
        panic("Invalid attempt to use boot memory allocator\n");    
    }

    bitmap_set(bm->page_map, start_page, npages);
}


void
mm_boot_reserve_vmem (addr_t start, ulong_t size)
{
    mm_boot_reserve_mem(va_to_pa(start), size);
}


void
mm_boot_free_mem (addr_t start, ulong_t size)
{
    uint32_t start_page = PADDR_TO_PAGE(start);
    uint32_t npages     = (size+PAGE_SIZE-1) / PAGE_SIZE; // what if this is smaller than a page?
    boot_mem_info_t * bm = &bootmem;

    if (unlikely(boot_mm_inactive)) {
        panic("Invalid attempt to use boot memory allocator\n");
    }

    bitmap_clear(bm->page_map, start_page, npages);
}


void 
mm_boot_free_vmem (addr_t start, ulong_t size)
{
    mm_boot_free_mem(va_to_pa(start), size);
}


#define ALIGN_CEIL(x,a) ( (x)%(a) ? ((x)/(a) + 1) * (a) : (x) )

static addr_t addr_low=-1;
static addr_t addr_high=0;

static void update_boot_range(addr_t start, addr_t end)
{
    if (start<addr_low) { 
	addr_low = start;
    }
    if (end>addr_high) {
	addr_high = end;
    }
    
    BMM_DEBUG("Boot range is now %p-%p\n",addr_low,addr_high);
}


// Get the highest allocated address - for kmem.c
void *boot_mm_get_cur_top()
{
    return (void*) addr_high;
}

/*
 * this is our main boot memory allocator, based on a simple 
 * bitmap scan. 
 *
 * NOTE: this is not thread-safe
 */
void *
__mm_boot_alloc (ulong_t size, ulong_t align, ulong_t goal)
{
    ulong_t offset, remaining_size, areasize, preferred;
    ulong_t i, start = 0, incr, eidx, end_pfn = mm_info.last_pfn;
    boot_mem_info_t * minfo = &bootmem;
    void * ret = NULL;

    if (unlikely(boot_mm_inactive)) {
        panic("Invalid attempt to use boot memory allocator\n");
    }

    /* we don't accept zero-sized allocations */
    ASSERT(size);

    /* alignment better be a power of 2 */
    ASSERT(!(align & (align-1)));

    eidx = end_pfn;

    /* NOTE: we ignore goal for now */

    preferred = 0;

    /* ceil it to page frame count (will usually be one) */
    areasize = (size + PAGE_SIZE-1)/PAGE_SIZE;

    /* will almost always be 1 (note the GNU extension usage here...) */
    incr = (align >> PAGE_SHIFT) ? : 1;

    for (i = preferred; i < eidx; i += incr) {
        ulong_t j;
        i = i + find_next_zero_bit(minfo->page_map, eidx+1, i);
        i = ALIGN(i, incr);
        if (i >= eidx)
            break;
        if (test_bit(i, minfo->page_map))
            continue;
        for (j = i + 1; j < i + areasize; ++j) {
            if (j >= eidx)
                goto fail_block;
            if (test_bit(j, minfo->page_map))
                goto fail_block;
        }

        start = i;
        goto found;

    fail_block:
        i = ALIGN(j, incr);
    }

    return NULL;

found:
    minfo->last_success = start << PAGE_SHIFT;
    ASSERT(start < eidx);

    /*
     * Is the next page of the previous allocation-end the start
     * of this allocation's buffer? If yes then we can 'merge'
     * the previous partial page with this allocation.
     */
    if (align < PAGE_SIZE && 
        minfo->last_offset && 
        (minfo->last_pos + 1) == start) {

        offset = ALIGN(minfo->last_offset, align);
        ASSERT(offset <= PAGE_SIZE);
        remaining_size = PAGE_SIZE-offset;

        /* does it fit? */
        if (size < remaining_size) {

            areasize = 0;
            /* last_pos unchanged */
            minfo->last_offset = offset + size;
            ret = (void*)((minfo->last_pos * PAGE_SIZE) + offset);

        } else {

            remaining_size = size - remaining_size;
            areasize = (remaining_size + PAGE_SIZE - 1)/PAGE_SIZE;
            ret = (void*)((minfo->last_pos * PAGE_SIZE) + offset);
            minfo->last_pos = start + areasize - 1;
            minfo->last_offset = remaining_size;

        }

        minfo->last_offset &= ~PAGE_MASK;

    } else {

        minfo->last_pos = start + areasize - 1;
        minfo->last_offset = size & ~PAGE_MASK;
        ret = (void*)(start * PAGE_SIZE);

    }

    for (i = start; i < start+areasize; i++) {
        if (unlikely(test_and_set_bit(i, minfo->page_map)))
            panic("bit %u not set!\n", i);
    }

    BMM_DEBUG("Allocated %d bytes, alignment %d (%d pages) at %p\n", size, align, areasize, ret);

    /* NOTE: we do NOT zero the memory! */
    addr_t addr = pa_to_va((ulong_t)ret);


    update_boot_range(addr,ALIGN_CEIL(addr+size,align));

    return (void*)addr;
}



void * 
mm_boot_alloc (ulong_t size)
{
    return __mm_boot_alloc(size, CACHE_LINE_SIZE_DEFAULT, 0);
}


void *
mm_boot_alloc_aligned (ulong_t size, ulong_t align)
{
    return __mm_boot_alloc(size, align, 0);
}


void 
mm_boot_free (void *addr, ulong_t size)
{
    ulong_t i, start, sidx, eidx;
    boot_mem_info_t * minfo = &bootmem;

    addr = (void*)(va_to_pa((ulong_t)addr));

    if (unlikely(boot_mm_inactive)) {
        panic("Invalid attempt to use boot memory allocator\n");
    }

    /*
     * round down end of usable mem, partially free pages are
     * considered reserved.
     */
    ulong_t end = ((ulong_t)addr + size)/PAGE_SIZE;

    ASSERT(size);
    ASSERT(end > 0);

    if ((ulong_t)addr < minfo->last_success)
        minfo->last_success = (ulong_t)addr;

    /*
     * Round up the beginning of the address.
     */
    start = ((ulong_t)addr + PAGE_SIZE-1) / PAGE_SIZE;
    sidx = start;
    eidx = end;

    for (i = sidx; i < eidx; i++) {
        if (unlikely(!test_and_clear_bit(i, minfo->page_map))) {
            panic("could not free page\n");
        }
    }
}


/* add the unused pages to this mem region's mem-pool */
static ulong_t
add_free_pages (struct mem_region * region) 
{
    ulong_t count = 0;
    ulong_t * pm  = bootmem.page_map;
    ulong_t addr, i, m;

    ASSERT(region);

    ulong_t start_pfn = region->base_addr >> PAGE_SHIFT;
    ulong_t end_pfn   = (region->base_addr + region->len) >> PAGE_SHIFT;

    ASSERT(end_pfn < bootmem.npages);

    for (i = start_pfn; i < end_pfn; ) {

        ulong_t v = ~pm[i/BITS_PER_LONG];
       

        /* we have free pages in this index */
        if (v) {
            addr = start_pfn << PAGE_SHIFT;
            for (m = 1; m && i < end_pfn; m <<= 1, addr += PAGE_SIZE, i++) {
                if (v & m) {
		    if (is_usable_ram(addr,PAGE_SIZE)) { 
			kmem_add_memory(region, addr, PAGE_SIZE);
			++count;
		    } else {
			ERROR_PRINT("Skipping addition of memory at %p (%p bytes) - Likely memory map / SRAT mismatch\n",addr,PAGE_SIZE);
		    }
                } 
            }
        } else {
            i += BITS_PER_LONG;
        }
        start_pfn += BITS_PER_LONG;
    }

    return count*PAGE_SIZE;
}


/*
 * this makes the transfer to the kmem allocator, 
 * we won't be using the boot bitmap allocator anymore
 * after this point
 */
void
mm_boot_kmem_init (void)
{
    unsigned i;
    ulong_t count = 0;
    struct nk_locality_info * loc = &(nk_get_nautilus_info()->sys.locality_info);

    /* we walk ALL of the registered memory regions
     * and add their associated pages in the existing bitmap to the
     * kernel mem pool */
    BMM_PRINT("Adding boot memory regions to the kernel memory pool:\n");
    for (i = 0; i < loc->num_domains; i++) {
        struct mem_region * region = NULL;
        unsigned j = 0;
        list_for_each_entry(region, &(loc->domains[i]->regions), entry) {
            ulong_t added = add_free_pages(region);
            BMM_PRINT("    [Domain %02u : Region %02u] (%0lu.%02lu MB)\n", 
                    i, j,
                    added / 1000000,
                    added % 1000000);
            count += added;
            j++;
        }
    }

    ASSERT(count != 0);


    /* we no longer need to use the boot allocator */
    boot_mm_inactive = 1;

    BMM_PRINT("    =======\n");
    BMM_PRINT("    [TOTAL] (%lu.%lu MB)\n", count/1000000, count%1000000);

}

void 
mm_boot_kmem_cleanup (void)
{
    ulong_t count = 0;

    BMM_PRINT("Reclaiming boot sections and data:\n");

    BMM_PRINT("    [Boot alloc. page map] (%0lu.%02lu MB)\n", bootmem.pm_len/1000000, bootmem.pm_len%1000000);
    if (is_usable_ram(va_to_pa((ulong_t)bootmem.page_map),bootmem.pm_len)) {
	kmem_add_memory(kmem_get_region_by_addr(va_to_pa((ulong_t)bootmem.page_map)),
			va_to_pa((ulong_t)bootmem.page_map), 
			bootmem.pm_len);
	count += bootmem.pm_len;
    } else {
	ERROR_PRINT("Skipping reclaim of boot page map as memory is not usable: %p (%p bytes) - Likely memory map / SRAT mismatch\n",va_to_pa((ulong_t)bootmem.page_map),bootmem.pm_len);
    }


    BMM_PRINT("    [Boot page tables and stack]     (%0lu.%02u MB)\n", PAGE_SIZE_4KB*3/1000000, PAGE_SIZE_4KB%1000000);
    
    if (is_usable_ram(va_to_pa((ulong_t)(&pml4)), 
		      PAGE_SIZE_4KB*3 + PAGE_SIZE_2MB)) { 
	kmem_add_memory(kmem_get_region_by_addr(va_to_pa((ulong_t)&pml4)), 
			va_to_pa((ulong_t)(&pml4)), 
			PAGE_SIZE_4KB*3 + PAGE_SIZE_2MB);
	
	count += PAGE_SIZE_4KB*3 + PAGE_SIZE_2MB;
    } else {
	ERROR_PRINT("Skipping reclaim of boot page tables and stack as memory is not usable: %p (%p bytes) - Likely memory map / SRAT mismatch\n",va_to_pa((ulong_t)(&pml4)), PAGE_SIZE_4KB*3 + PAGE_SIZE_2MB);
    }

    BMM_PRINT("    =======\n");
    BMM_PRINT("    [TOTAL] (%lu.%lu MB)\n", count/1000000, count%1000000);

    BMM_PRINT("    Boot allocation range: %p-%p\n",(void*)addr_low, (void*)addr_high);

    kmem_inform_boot_allocation((void*)addr_low,(void*)addr_high);
   
}
