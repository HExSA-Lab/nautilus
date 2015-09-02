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
#include <nautilus/nautilus.h>
#include <nautilus/mm.h>
#include <nautilus/sfi.h>
#include <nautilus/naut_assert.h>
#include <nautilus/naut_types.h>
#include <nautilus/multiboot2.h>
#include <nautilus/macros.h>

extern char * mem_region_types[6];

#ifndef NAUT_CONFIG_DEBUG_BOOTMEM
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define BMM_DEBUG(fmt, args...) DEBUG_PRINT("BOOTMEM: " fmt, ##args)
#define BMM_PRINT(fmt, args...) printk("BOOTMEM: " fmt, ##args)
#define BMM_WARN(fmt, args...)  WARN_PRINT("BOOTMEM: " fmt, ##args)

static uint8_t 
blk_cksum_ok (const uint8_t * cursor, unsigned len)
{
    unsigned sum = 0;
    while (len--) {
        sum += *cursor++;
    }
    return ((sum & 0xff) == 0);
}

static uint32_t
memtype_efi2mb (efi_mem_type_t efi_type)
{
    switch (efi_type) {
        case EfiConventionalMemory:
            return MULTIBOOT_MEMORY_AVAILABLE;
        case EfiACPIMemoryNVS:
            return MULTIBOOT_MEMORY_NVS;
        case EfiReservedMemoryType:
            return MULTIBOOT_MEMORY_RESERVED;
        case EfiACPIReclaimMemory:
            return MULTIBOOT_MEMORY_ACPI_RECLAIMABLE;
        case EfiUnusableMemory:
            return MULTIBOOT_MEMORY_BADRAM;
        default:
            return 0;
    }

    return 0;
}


void
arch_reserve_boot_regions (unsigned long mbd)
{
	/* We reserve the zero page so it doesn't look like 
     * allocations failed when they return zero! */
	BMM_PRINT("Reserving zero page\n");
	mm_boot_reserve_mem(0, PAGE_SIZE);
}


void
arch_detect_mem_map (mmap_info_t * mm_info, 
                     mem_map_entry_t * memory_map,
                     unsigned long mbd)
{
    struct sfi_mmap_tbl * sfimem = sfi_get_mmap();
    uint32_t i, n;

    if (!sfimem) {
        panic("Could not find SFI memory map.\n");
    }

    if (!blk_cksum_ok((uint8_t*)sfimem, sfimem->hdr.len)) {
        panic("SFI MMAP table checksum failed\n");
    }

    n = sfi_get_mmap_nentries(sfimem);

    if (n > MAX_MMAP_ENTRIES) {
        panic("Reached memory region limit\n");
    }

    for (i = 0; i < n; i++) {

        /* We ignore the attributes for now */

        efi_mem_desc_t mem = sfimem->entries[i];

        ulong_t start,end;

        start = round_up(mem.phys_start, PAGE_SIZE_4KB);
        end   = round_down(mem.phys_start + (mem.num_pages*PAGE_SIZE_4KB), PAGE_SIZE_4KB);

        memory_map[i].addr = start;
        memory_map[i].len  = end-start;
        memory_map[i].type = memtype_efi2mb(mem.type);

        if(memory_map[i].type == MULTIBOOT_MEMORY_AVAILABLE) {
            mm_info->usable_ram += memory_map[i].len;
        }

        if (end > (mm_info->last_pfn << PAGE_SHIFT)) {
            mm_info->last_pfn = end >> PAGE_SHIFT;
        }

        mm_info->total_mem += end-start;
        ++mm_info->num_regions;
    }
}
