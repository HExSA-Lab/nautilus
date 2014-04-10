#include <cga.h>
#include <printk.h>
#include <multiboot2.h>

static void 
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
            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
                printk("low memory available: %u KB\n", ((struct multiboot_tag_basic_meminfo *) tag)->mem_lower);
                printk("high memory available: %u KB\n", ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);
                break;
            case MULTIBOOT_TAG_TYPE_MMAP: {
                multiboot_memory_map_t * mmap;
                printk("Memory Map:\n");
                for (mmap=((struct multiboot_tag_mmap*)tag)->entries;
                        (multiboot_uint8_t*)mmap < (multiboot_uint8_t*)tag + tag->size;
                        mmap = (multiboot_memory_map_t*)((unsigned long)mmap + 
                            ((struct multiboot_tag_mmap*)tag)->entry_size)) {
                    printk(" base_addr = 0x%x%x,"
                           " length = 0x%x%x, type = 0x%x\n", 
                           (unsigned) (mmap->addr >> 32),
                           (unsigned) (mmap->addr & 0xffffffff),
                           (unsigned) (mmap->len >> 32),
                           (unsigned) (mmap->len & 0xffffffff),
                           (unsigned) mmap->type);
                }
                break;
                                          }
            }

    }
}


void 
main (unsigned long mbd, unsigned long magic)
{
    term_init();
    parse_multiboot(mbd, magic);
}

