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
#include <thread.h>
#include <idle.h>
#include <percpu.h>


#include <dev/apic.h>
#include <dev/pci.h>
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

    show_splash();

    setup_idt();

    serial_init();

    detect_cpu();

    paging_init(&(naut->sys.mem), mbd);

    init_liballoc_hooks();

    disable_8259pic();

    smp_early_init(naut);

    ioapic_init(&(naut->sys));

    apic_init(naut);

    kbd_init(naut);

    timer_init(naut);

    pci_init(naut);

    // setup per-core area for BSP
    msr_write(MSR_GS_BASE, (uint64_t)naut->sys.cpus[0]);

    sched_init();

    smp_bringup_aps(naut);
    
    sti();

    thread_start(side_screensaver, NULL, NULL, 0, TSTACK_DEFAULT, NULL, CPU_ANY);

    printk("Nautilus main thread halting on core %d\n", my_cpu_id());

    while (1) {
        yield();
    }
}

