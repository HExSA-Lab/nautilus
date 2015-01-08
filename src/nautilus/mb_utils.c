#include <nautilus/multiboot2.h>
#include <nautilus/mb_utils.h>
#include <nautilus/nautilus.h>
#include <nautilus/naut_types.h>
#include <nautilus/paging.h>


uint_t 
multiboot_get_size (ulong_t mbd)
{
    return *(uint_t*)mbd;
}



/* 
 * Returns total physical memory in BYTES
 *
 * Multiboot basic memory info doesn't appear to give right num, so
 * better to actually use the e820 map
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


void 
multiboot_rsv_mem_regions(struct nk_mem_info * mem, ulong_t mbd) 
{
    struct multiboot_tag * tag;

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

        addr_t base_addr = mmap->addr;
        addr_t len       = mmap->len;

        if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE) {
            DEBUG_PRINT("Reserving pages %d to %d (0x%x - 0x%x) (pg addr=%p)\n", PADDR_TO_PAGE(base_addr), PADDR_TO_PAGE(base_addr+len-1),
                    base_addr, base_addr+len-1, (void*)PAGE_MASK(base_addr));
            nk_reserve_range(base_addr, base_addr+len);
        }
    }
}


extern void* malloc(size_t);

struct multiboot_info * 
multiboot_parse (ulong_t mbd, ulong_t magic)
{
    uint_t size;
    struct multiboot_tag * tag;
    struct multiboot_info * mb_info = (struct multiboot_info*)malloc(sizeof(struct multiboot_info));
    if (!mb_info) {
        ERROR_PRINT("Could not allocate multiboot info struct\n");
        return NULL;
    }

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
                mb_info->boot_loader = malloc(str->size);
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
                        elf->shndx);
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
                mb_info->boot_cmd_line = malloc(cmd->size);
                strncpy(mb_info->boot_cmd_line, cmd->string, cmd->size);
                DEBUG_PRINT("Cmd line: %s\n", mb_info->boot_cmd_line);
                break;
                                             }
            default:
                break;

        }
    }

    return mb_info;
}
