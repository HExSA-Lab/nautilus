#include <nautilus.h>
#include <cga.h>
#include <paging.h>
#include <idt.h>
#include <spinlock.h>
#include <cpu.h>
#include <cpuid.h>
#include <smp.h>

#include <dev/apic.h>

#include <lib/liballoc_hooks.h>
#include <lib/liballoc.h>

void 
main (unsigned long mbd, unsigned long magic)
{
    struct apic_dev * apic;
    term_init();

    printk("Welcome to the Nautilus Kernel\n\n");

    detect_cpu();

    init_page_frame_alloc(mbd);

    setup_idt();

    init_liballoc_hooks();

    smp_init();

    apic = (struct apic_dev*)malloc(sizeof(struct apic_dev));
    if (!apic) {
        ERROR_PRINT("could not allocate apic struct\n");
    }
    memset(apic, 0, sizeof(struct apic_dev));

    apic_init(apic);

}

