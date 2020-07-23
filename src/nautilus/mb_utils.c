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
 * Copyright (c) 2015, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <khale@cs.iit.edu>
 *          Conghao Liu <cliu115@hawk.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/multiboot2.h>
#include <nautilus/mb_utils.h>
#include <nautilus/nautilus.h>
#include <nautilus/naut_types.h>
#include <nautilus/paging.h>
#include <nautilus/mm.h>
#include <nautilus/elf.h>
#include <nautilus/module.h>

extern addr_t _bssEnd;

uint_t 
multiboot_get_size (ulong_t mbd)
{
    return *(uint_t*)mbd;
}



/* 
 * Returns total physical memory in BYTES
 *
 * This includes holes and reserved memory
 */
addr_t
multiboot_get_phys_mem (ulong_t mbd)
{
    struct multiboot_tag * tag;
    ulong_t sum = 0;

    if (mbd & 7) {
        panic("ERROR: Unaligned multiboot info struct\n");
    }

    tag = (struct multiboot_tag*)(mbd+8);
    while (tag->type != MULTIBOOT_TAG_TYPE_MMAP) {
        tag = (struct multiboot_tag*)((multiboot_uint8_t*)tag + ((tag->size+7)&~7));
    }

    if (tag->type != MULTIBOOT_TAG_TYPE_MMAP) {
        panic("ERROR: no mmap tag found\n");
    }

    multiboot_memory_map_t * mmap;

    for (mmap=((struct multiboot_tag_mmap*)tag)->entries;
            (multiboot_uint8_t*)mmap < (multiboot_uint8_t*)tag + tag->size;
            mmap = (multiboot_memory_map_t*)((ulong_t)mmap + 
                ((struct multiboot_tag_mmap*)tag)->entry_size)) {

        sum += mmap->len;
    }

    return (addr_t)sum;
}


ulong_t
multiboot_get_sys_ram (ulong_t mbd)
{
    struct multiboot_tag * tag;
    ulong_t sum = 0;

    if (mbd & 7) {
        panic("ERROR: Unaligned multiboot info struct\n");
    }

    tag = (struct multiboot_tag*)(mbd+8);
    while (tag->type != MULTIBOOT_TAG_TYPE_MMAP) {
        tag = (struct multiboot_tag*)((multiboot_uint8_t*)tag + ((tag->size+7)&~7));
    }

    if (tag->type != MULTIBOOT_TAG_TYPE_MMAP) {
        panic("ERROR: no mmap tag found\n");
    }

    multiboot_memory_map_t * mmap;

    for (mmap=((struct multiboot_tag_mmap*)tag)->entries;
            (multiboot_uint8_t*)mmap < (multiboot_uint8_t*)tag + tag->size;
            mmap = (multiboot_memory_map_t*)((ulong_t)mmap + 
                ((struct multiboot_tag_mmap*)tag)->entry_size)) {

        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            sum += mmap->len;
        }
    }

    return sum;
}


/* 
 * We cannot place the page map directly after the loaded kernel if the
 * bootloader has placed modules in that memory region. Unfortunately, GRUB
 * doesn't appear to include this when it's marking memory regions as reserved.
 * To figure out where we should layout the page map, we thus need to manually
 * scan the module adderesses and make sure we don't use the memory that they
 * occupy.
 *
 */
addr_t
multiboot_get_modules_end (ulong_t mbd)
{
    struct multiboot_tag * tag;
    addr_t addr = (addr_t)&_bssEnd;

    if (mbd & 7) {
        panic("ERROR: Unaligned multiboot info struct\n");
    }

    tag = (struct multiboot_tag*)(mbd+8);

    while (tag->type != MULTIBOOT_TAG_TYPE_END) {


        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {

            struct multiboot_tag_module * mod = (struct multiboot_tag_module*)tag;
            if (mod->mod_end > addr) {
                addr = mod->mod_end;
                DEBUG_PRINT("Increasing kern end to %p\n", (void*)addr);
            }
        }

        tag = (struct multiboot_tag*)((multiboot_uint8_t*)tag + ((tag->size+7)&~7));
	
    }

    return addr;
}


extern void* mm_boot_alloc(size_t);


int 
mb_is_hrt_environ (ulong_t mbd)
{
    struct multiboot_tag * tag;

    if (mbd & 7) {
        panic("ERROR: Unaligned multiboot info struct\n");
    }

    tag = (struct multiboot_tag*)(mbd+8);

    while (tag->type != MULTIBOOT_TAG_TYPE_HRT &&
           tag->type != MULTIBOOT_TAG_TYPE_END) {
        tag = (struct multiboot_tag*)((multiboot_uint8_t*)tag + ((tag->size+7)&~7));
    }

    if (tag->type != MULTIBOOT_TAG_TYPE_HRT) {
        return 0;
    }

    return 1;
}


void * 
mb_get_first_hrt_addr (ulong_t mbd)
{
    struct multiboot_tag * tag;

    if (mbd & 7) {
        panic("ERROR: Unaligned multiboot info struct\n");
    }

    tag = (struct multiboot_tag*)(mbd+8);

    while (tag->type != MULTIBOOT_TAG_TYPE_HRT &&
           tag->type != MULTIBOOT_TAG_TYPE_END) {
        tag = (struct multiboot_tag*)((multiboot_uint8_t*)tag + ((tag->size+7)&~7));
    }

    if (tag->type != MULTIBOOT_TAG_TYPE_HRT) {
        return NULL;
    }

    struct multiboot_tag_hrt * hrt = (struct multiboot_tag_hrt*)tag;
    return (void*)hrt->first_hrt_gpa;
}

struct multiboot_info * 
multiboot_parse (ulong_t mbd, ulong_t magic)
{
    uint_t size;
    struct multiboot_tag * tag;
    struct multiboot_info * mb_info = (struct multiboot_info*)mm_boot_alloc(sizeof(struct multiboot_info));
    if (!mb_info) {
        ERROR_PRINT("Could not allocate multiboot info struct\n");
        return NULL;
    }
    memset(mb_info, 0, sizeof(struct multiboot_info));

    INIT_LIST_HEAD(&mb_info->mod_list);

    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        panic("ERROR: Not loaded by multiboot compliant bootloader\n");
    }

    if (mbd & 7) {
        panic("ERROR: Unaligned multiboot info struct\n");
    }

    size = *(uint_t*)mbd;
    DEBUG_PRINT("Multiboot info size %uB\n", size);

    for (tag = (struct multiboot_tag*)(mbd+8);
            tag->type != MULTIBOOT_TAG_TYPE_END;
            tag = (struct multiboot_tag *) ((multiboot_uint8_t*)tag
                + ((tag->size + 7) & ~7))) {

        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME: {
                struct multiboot_tag_string * str = (struct multiboot_tag_string*)tag;
                mb_info->boot_loader = mm_boot_alloc(str->size);
                strncpy(mb_info->boot_loader, str->string, str->size);
                DEBUG_PRINT("Boot loader: %s\n", ((struct multiboot_tag_string*)tag)->string);
                break;
                                                      }
            case MULTIBOOT_TAG_TYPE_ELF_SECTIONS: {
                struct multiboot_tag_elf_sections * elf = (struct multiboot_tag_elf_sections*)tag;
                DEBUG_PRINT("ELF size=%u, num=%u, entsize=%u, shndx=%u, sechdr=%p\n", 
                        elf->size,
                        elf->num,
                        elf->entsize,
                        elf->shndx,
                        elf->sections);
                break;
                                                  }
            case MULTIBOOT_TAG_TYPE_MMAP: {
                DEBUG_PRINT("Multiboot2 memory map detected\n");
                break;
                                          }

            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO: {
                DEBUG_PRINT("Multiboot2 basic meminfo detected\n");
                break;
                                                   }
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
                struct multiboot_tag_framebuffer_common * fb = (struct multiboot_tag_framebuffer_common*)tag;
                DEBUG_PRINT("fb addr: %p, fb_width: %u, fb_height: %u\n", 
                        (void*)fb->framebuffer_addr,
                        fb->framebuffer_width,
                        fb->framebuffer_height);
                break;
                                                 }
            case MULTIBOOT_TAG_TYPE_CMDLINE: {
                struct multiboot_tag_string* cmd = (struct multiboot_tag_string*)tag;
                mb_info->boot_cmd_line = mm_boot_alloc(cmd->size);
                strncpy(mb_info->boot_cmd_line, cmd->string, cmd->size);
                DEBUG_PRINT("Kernel Cmd line: %s\n", mb_info->boot_cmd_line);
                break;
                                             }

			case MULTIBOOT_TAG_TYPE_BOOTDEV: {
				struct multiboot_tag_bootdev * bd = (struct multiboot_tag_bootdev*)tag;
				DEBUG_PRINT("Boot device: (biosdev=0x%x,slice=%u,part=%u)\n", bd->biosdev, bd->slice, bd->part);
				break;
											 }
			case MULTIBOOT_TAG_TYPE_ACPI_OLD: {
				struct multiboot_tag_old_acpi * oacpi = (struct multiboot_tag_old_acpi*)tag;
				DEBUG_PRINT("Old ACPI: rsdp=%p\n", oacpi->rsdp);
				break;
											  }
			case MULTIBOOT_TAG_TYPE_ACPI_NEW: {
				struct multiboot_tag_new_acpi * nacpi = (struct multiboot_tag_new_acpi*)tag;
				DEBUG_PRINT("New ACPI: rsdp=%p\n", nacpi->rsdp);
				break;
							  }
            case MULTIBOOT_TAG_TYPE_IMAGE_BASE: {
                struct multiboot_tag_image_load_base * imb = (struct multiboot_tag_image_load_base *)tag;
                DEBUG_PRINT("Image load base: 0x%x\n", imb->addr);
                break;

            }
            case MULTIBOOT_TAG_TYPE_MODULE: {
                struct multiboot_tag_module * mod = (struct multiboot_tag_module*)tag;
                DEBUG_PRINT("Found module: \n");
                DEBUG_PRINT("  type:     0x%08x\n", mod->type);
                DEBUG_PRINT("  size:     0x%08x\n", mod->size);
                DEBUG_PRINT("  mod_start 0x%08x\n", mod->mod_start);
                DEBUG_PRINT("  mod_end   0x%08x\n", mod->mod_end);
                DEBUG_PRINT("  args:     %s\n",     mod->cmdline);
                if (nk_register_mod(mb_info, mod) != 0) {
                    ERROR_PRINT("Could not register multiboot module\n");
                    return NULL;
                }
                break;
            }
#ifdef NAUT_CONFIG_HVM_HRT
            case MULTIBOOT_TAG_TYPE_HRT: {
                struct multiboot_tag_hrt * hrt = (struct multiboot_tag_hrt*)tag;
                mb_info->hrt_info = mm_boot_alloc(hrt->size);
                memcpy(mb_info->hrt_info, hrt, hrt->size);
                DEBUG_PRINT("HRT Info struct\n");
                DEBUG_PRINT("  total_num_apics: %u\n", hrt->total_num_apics);
                DEBUG_PRINT("  first_hrt_apic_id: %u\n", hrt->first_hrt_apic_id);
                DEBUG_PRINT("  have_hrt_ioapic: %u\n", hrt->have_hrt_ioapic);
                DEBUG_PRINT("  first_hrt_ioapic_entry: %u\n", hrt->first_hrt_ioapic_entry);
                DEBUG_PRINT("  cpu_freq_khz: %llu\n", (void*)hrt->cpu_freq_khz);
                DEBUG_PRINT("  hrt_flags: %p\n", (void*)hrt->hrt_flags);
                DEBUG_PRINT("  max_mem_mapped: %p\n", (void*)hrt->max_mem_mapped);
                DEBUG_PRINT("  first_hrt_gpa: %p\n", (void*)hrt->first_hrt_gpa);
                DEBUG_PRINT("  gva_offset: %p\n", (void*)hrt->gva_offset);
                DEBUG_PRINT("  comm_page_gpa: %p\n", (void*)hrt->comm_page_gpa);
                DEBUG_PRINT("  hrt_int_vec: %u\n", hrt->hrt_int_vec);

                break;
                                        }
#endif
            default:
                DEBUG_PRINT("Unhandled tag type (0x%x)\n", tag->type);
                break;

        }
    }

    return mb_info;
}
