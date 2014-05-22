#include <nautilus.h>
#include <cga.h>
#include <paging.h>
#include <idt.h>
#include <spinlock.h>
#include <cpu.h>
#include <cpuid.h>
#include <serial.h>
#include <smp.h>
#include <irq.h>

#include <dev/apic.h>
#include <dev/ioapic.h>
#include <dev/timer.h>
#include <dev/kbd.h>

#include <lib/liballoc_hooks.h>
#include <lib/liballoc.h>

/* TODO: stuff devices etc into here */
struct naut_info nautilus_info;

void 
main (unsigned long mbd, unsigned long magic)
{
    struct naut_info * naut = &nautilus_info;

    term_init();

    setup_idt();
    serial_init();

    printk("Welcome to the Nautilus Kernel\n\n");

    detect_cpu();

    /* TODO: fixup paging */

    init_page_frame_alloc(mbd);

    init_liballoc_hooks();

    memset(naut, 0, sizeof(struct naut_info));

    naut->sys = (struct sys_info *)malloc(sizeof(struct sys_info));
    if (!naut->sys) {
        ERROR_PRINT("could not allocate sysinfo\n");
    }
    memset(naut->sys, 0, sizeof(struct sys_info));

    smp_init(naut);


    printk("Disabling legacy 8259 PIC\n");
    disable_8259pic();

    ioapic_init(naut->sys);

    apic_init(naut);

    timer_init(naut);
    kbd_init(naut);

    sti();

    while (1) { halt(); }
}

