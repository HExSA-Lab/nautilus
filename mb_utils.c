#include <multiboot2.h>
#include <mb_utils.h>
#include <types.h>
#include <printk.h>
#include <paging.h>

unsigned long long
get_phys_mem (unsigned long mbd) 
{
    struct multiboot_tag * tag;

    if (mbd & 7) {
        printk("ERROR: Unaligned multiboot info struct\n");
    }

    tag = (struct multiboot_tag*)(mbd+8);
    while (tag->type != MULTIBOOT_TAG_TYPE_BASIC_MEMINFO) {
        tag = (struct multiboot_tag*)((multiboot_uint8_t*)tag + ((tag->size+7) & ~7));
    }

    if (tag->type != MULTIBOOT_TAG_TYPE_BASIC_MEMINFO) {
        printk("ERROR: couldn't find multiboot mem info\n");
    }

    
    unsigned long long lo;
    unsigned long long hi;

    lo = ((struct multiboot_tag_basic_meminfo *) tag)->mem_lower;
    hi = ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper;

    return (lo+hi)<<10;
}

void 
reserve_mem_regions (unsigned long mbd) 
{
    struct multiboot_tag * tag;

    if (mbd & 7) {
        // TODO: PANIC!
        printk("ERROR: Unaligned multiboot info struct\n");
    }

    tag = (struct multiboot_tag*)(mbd+8);
    while (tag->type != MULTIBOOT_TAG_TYPE_MMAP) {
        tag = (struct multiboot_tag*)((multiboot_uint8_t*)tag + ((tag->size+7)&~7));
    }

    if (tag->type != MULTIBOOT_TAG_TYPE_MMAP) {
        // TODO: PANIC!
        printk("ERROR: no mmap tag found\n");
    }

    multiboot_memory_map_t * mmap;

    extern uint8_t * page_map;

    for (mmap=((struct multiboot_tag_mmap*)tag)->entries;
            (multiboot_uint8_t*)mmap < (multiboot_uint8_t*)tag + tag->size;
            mmap = (multiboot_memory_map_t*)((unsigned long)mmap + 
                ((struct multiboot_tag_mmap*)tag)->entry_size)) {

        addr_t base_addr = (mmap->addr >> 32) | (mmap->addr & 0xffffffff);
        addr_t len       = (mmap->len >> 32) | (mmap->len & 0xffffffff);

        if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE) {
            printk("Reserving memory region at 0x%x (len: 0x%x)\n", base_addr, len);
            mark_range_reserved(page_map, base_addr, base_addr + len-1);
        }
    }
}

void 
parse_multiboot (unsigned long mbd, unsigned long magic)
{
    unsigned size;
    struct multiboot_tag * tag;

    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        printk("ERROR: Not loaded by multiboot compliant bootloader\n");
    }

    printk("Our multiboot info structure is at: 0x%x\n", mbd);

    if (mbd & 7) {
        printk("ERROR: Unaligned multiboot info struct\n");
    }

    size = *(unsigned*)mbd;
    printk("Multiboot info size 0x%x\n", size);

    for (tag = (struct multiboot_tag*)(mbd+8);
            tag->type != MULTIBOOT_TAG_TYPE_END;
            tag = (struct multiboot_tag *) ((multiboot_uint8_t*)tag
                + ((tag->size + 7) & ~7))) {

        
        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
                printk("Boot loader: %s\n", ((struct multiboot_tag_string*)tag)->string);
                break;
            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO: {
                unsigned long long lo;
                unsigned long long hi;

                lo = ((struct multiboot_tag_basic_meminfo *) tag)->mem_lower;
                hi = ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper;
                printk("Total available physical memory: %u KB\n", lo+hi);
                
                break;
            }
            case MULTIBOOT_TAG_TYPE_MMAP: {
                multiboot_memory_map_t * mmap;
                printk("Memory Map:\n");
                for (mmap=((struct multiboot_tag_mmap*)tag)->entries;
                        (multiboot_uint8_t*)mmap < (multiboot_uint8_t*)tag + tag->size;
                        mmap = (multiboot_memory_map_t*)((unsigned long)mmap + 
                            ((struct multiboot_tag_mmap*)tag)->entry_size)) {

                    addr_t base_addr = (mmap->addr >> 32) | (mmap->addr & 0xffffffff);
                    addr_t len       = (mmap->len >> 32) | (mmap->len & 0xffffffff);

                    printk(" base_addr = 0x%x,"
                           " length = 0x%x, type = 0x%x\n", 
                           base_addr,
                           len,
                           (unsigned) mmap->type);

                }
                break;
                                          }
            }

    }
}
