#include <nautilus.h>
#include <cga.h>
#include <paging.h>
#include <idt.h>
#include <spinlock.h>
#include <cpu.h>
#include <msr.h>
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

static struct naut_info nautilus_info;
extern spinlock_t printk_lock;


void 
main (unsigned long mbd, unsigned long magic) 
{
    struct naut_info * naut = &nautilus_info;

    memset(naut, 0, sizeof(struct naut_info));

    term_init();

    spinlock_init(&printk_lock);

    printk(NAUT_WELCOME);

    setup_idt();

    serial_init();

    detect_cpu();

    paging_init(&(naut->sys.mem), mbd);

    init_liballoc_hooks();

    printk("Disabling legacy 8259 PIC\n");
    disable_8259pic();

    smp_early_init(naut);

    ioapic_init(&(naut->sys));

    apic_init(naut);

    kbd_init(naut);

    timer_init(naut);

    // setup per-core area for BSP
    msr_write(MSR_GS_BASE, (uint64_t)&(naut->sys.cpus[0]));

    smp_bringup_aps(naut);

    sti();

    printk("idle...\n");

    while (1) { halt(); }


}

