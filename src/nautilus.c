#include <nautilus.h>
#include <cga.h>
#include <paging.h>
#include <idt.h>
#include <spinlock.h>
#include <cpu.h>
#include <cpuid.h>

#include <dev/apic.h>

#include <lib/liballoc_hooks.h>
#include <lib/liballoc.h>

void 
main (unsigned long mbd, unsigned long magic)
{
    int *x;
    term_init();

    printk("Welcome to the Nautilus Kernel\n\n");

    detect_cpu();

    init_page_frame_alloc(mbd);

    setup_idt();

    // test the new idt with a bad pointer...
    x = (int*)0xffffe000;
    *x = 10;

    spinlock_t lock;
    spinlock_init(&lock);
    spin_lock(&lock);
    *x = 30;
    spin_unlock(&lock);

    printk("testing memory allocator\n");
    init_liballoc_hooks();
    void * y = malloc(100*PAGE_SIZE);
    printk("memory allocator works: %p\n", y);
    
    printk("looking for APIC\n");
    if (check_apic_avail()) {
        printk("found it!\n");
    } else {
        printk("APIC not found\n");
    }

}

