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
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu> (Gem5 - e820)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/mm.h>
#include <nautilus/mb_utils.h>
#include <nautilus/macros.h>
#include <nautilus/multiboot2.h>

extern char * mem_region_types[6];

#ifndef NAUT_CONFIG_DEBUG_BOOTMEM
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define BMM_DEBUG(fmt, args...) DEBUG_PRINT("BOOTMEM: " fmt, ##args)
#define BMM_PRINT(fmt, args...) printk("BOOTMEM: " fmt, ##args)
#define BMM_WARN(fmt, args...)  WARN_PRINT("BOOTMEM: " fmt, ##args)


void 
arch_reserve_boot_regions (unsigned long mbd)
{
#ifdef NAUT_CONFIG_REAL_MODE_INTERFACE
    INFO_PRINT("Reserving Long->Real Interface Segment (%p, size %lu)\n",
		NAUT_CONFIG_REAL_MODE_INTERFACE_SEGMENT*16UL, 65536UL);
    mm_boot_reserve_mem((addr_t)(NAUT_CONFIG_REAL_MODE_INTERFACE_SEGMENT*16UL),
			(ulong_t)65536);
#endif
}


extern void *gem5_real_mode_data_ptr;

typedef uint32_t e820_count_t;

#define GEM5_E820_COUNT_OFFSET 0x1e8
#define GEM5_E820_TABLE_OFFSET 0x2d0

// uses 20 byte entries
// would be too convenient if they used the 24 byte entries which
// would just be compatible with multiboot2...
typedef struct {
  uint64_t addr;
  uint64_t len;
  uint32_t type; // same as for multiboot2
} __packed e820_entry_t;

void
arch_detect_mem_map (mmap_info_t * mm_info, 
                     mem_map_entry_t * memory_map,
                     unsigned long mbd)
{

  if (!gem5_real_mode_data_ptr) {
    panic("ERROR: no real mode data pointer - so no e820 table...\n");
    return;
  }

  e820_count_t *e820_count = gem5_real_mode_data_ptr + GEM5_E820_COUNT_OFFSET;
  e820_entry_t *e820_entry = gem5_real_mode_data_ptr + GEM5_E820_TABLE_OFFSET;

  uint32_t i;

  if (*e820_count>MAX_MMAP_ENTRIES) {
    panic("Too many e820 entries\n");
    return;
  }

  BMM_PRINT("Parsing GEM5-provided e820 table\n");
    
  for (i=0;
       i<*e820_count;
       i++, e820_entry++) {
    
    ulong_t start,end;
    
    start = round_up(e820_entry->addr, PAGE_SIZE_4KB);
    end   = round_down(e820_entry->addr + e820_entry->len, PAGE_SIZE_4KB);

    memory_map[i].addr = start;
    memory_map[i].len  = end-start;
    memory_map[i].type = e820_entry->type;

    BMM_PRINT("Memory map[%u] - [%p - %p] <%s>\n", 
	      i, 
	      start,
	      end,
	      mem_region_types[memory_map[i].type]);
    
    if (e820_entry->type == 1) {
      mm_info->usable_ram += e820_entry->len;
    }
    
    if (end > (mm_info->last_pfn << PAGE_SHIFT)) {
      mm_info->last_pfn = end >> PAGE_SHIFT;
    }

    mm_info->total_mem += end-start;

    ++mm_info->num_regions;
  }
}


