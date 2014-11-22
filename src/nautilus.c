#include <nautilus.h>
#include <cga.h>
#include <paging.h>
#include <idt.h>
#include <spinlock.h>
#include <mb_utils.h>
#include <cpu.h>
#include <msr.h>
#include <cpuid.h>
#include <serial.h>
#include <smp.h>
#include <irq.h>
#include <thread.h>
#include <idle.h>
#include <percpu.h>
#include <errno.h>

#include <barrier.h>
#include <rwlock.h>
#include <condvar.h>

#include <dev/apic.h>
#include <dev/pci.h>
#include <dev/ioapic.h>
#include <dev/timer.h>
#include <dev/kbd.h>

#include <lib/liballoc_hooks.h>
#include <lib/liballoc.h>

#ifdef NAUT_CONFIG_NDPC_RT
#include "ndpc_preempt_threads.h"
#endif

static struct naut_info nautilus_info;
extern spinlock_t printk_lock;

#ifdef NAUT_CONFIG_CXX_SUPPORT
/* for global constructors */
extern void (*_init_array_start []) (void) __attribute__((weak));
extern void (*_init_array_end []) (void) __attribute__((weak));

static void do_ctors_init (void) {
    void (**p)(void) = NULL;
    printk("do_ctors_init\n");


    for (p = _init_array_start; p < _init_array_end; p++) {
        if (*p) {
            printk("Calling static constructor (%p)\n", (void*)(*p));
            (*p)();
        }
    }
}

#endif /* !NAUT_CONFIG_CXX_SUPPORT */


inline struct naut_info*
nk_get_nautilus_info (void)
{
    return &nautilus_info;
}


static void xcall_test (void * arg)
{
    printk("Running xcore test on core %u\n", my_cpu_id());
}


extern void ipi_test_setup(void);
extern void ipi_begin_test(cpu_id_t t);


static void tfun (void * in, void ** out)
{
    while (1) {
        printk("thread tfun running (tid=%u)\n", nk_get_tid());
        nk_yield();
    }
}


#ifdef NAUT_CONFIG_NDPC_RT
void ndpc_rt_test()
{
    printk("Testing NDPC Library and Executable\n");

    

#if 1
    // this function will be linked to nautilus
    test_ndpc();
#else
    thread_id_t tid;
    
    ndpc_init_preempt_threads();
    
    tid = ndpc_fork_preempt_thread();
    
    if (!tid) { 
        printk("Error in initial fork\n");
        return;
    } 


    if (tid!=ndpc_my_preempt_thread()) { 
        printk("Parent!\n");
        ndpc_join_preempt_thread(tid);
        printk("Joinend with foo\n");
    } else {
        printk("Child!\n");
        return;
    }

    ndpc_deinit_preempt_threads();

#endif 


}
#endif /* !NAUT_CONFIG_NDPC_RT */


static int 
sysinfo_init (struct sys_info * sys)
{
    sys->core_barrier = (nk_barrier_t*)malloc(sizeof(nk_barrier_t));
    if (!sys->core_barrier) {
        ERROR_PRINT("Could not allocate core barrier\n");
        return -1;
    }
    memset(sys->core_barrier, 0, sizeof(nk_barrier_t));

    if (nk_barrier_init(sys->core_barrier, sys->num_cpus) != 0) {
        ERROR_PRINT("Could not create core barrier\n");
        goto out_err;
    }

    return 0;

out_err:
    free(sys->core_barrier);
    return -EINVAL;
}


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

    nk_paging_init(&(naut->sys.mem), mbd);

    init_liballoc_hooks();

    naut->sys.mb_info = multiboot_parse(mbd, magic);
    if (!naut->sys.mb_info) {
        ERROR_PRINT("Problem parsing multiboot header\n");
    }

    disable_8259pic();

    smp_early_init(naut);

    // setup per-core area for BSP
    msr_write(MSR_GS_BASE, (uint64_t)naut->sys.cpus[0]);

    /* from this point on, we can use percpu macros (even if the APs aren't up) */

    sysinfo_init(&(naut->sys));

    ioapic_init(&(naut->sys));

    apic_init(naut);

    kbd_init(naut);

    timer_init(naut);

    pci_init(naut);

    nk_sched_init();

    smp_setup_xcall_bsp(naut->sys.cpus[0]);

    smp_bringup_aps(naut);

#ifdef NAUT_CONFIG_CXX_SUPPORT
    // We can assume that we won't encounter any
    // C++ before this point, invoke the constructors now
    do_ctors_init();
#endif

    sti();

#ifdef NAUT_CONFIG_NO_RT

    // screen saver
    nk_thread_start(side_screensaver, NULL, NULL, 0, TSTACK_DEFAULT, NULL, 1);

#endif

#ifdef NAUT_CONFIG_LEGION_RT

    extern void run_legion_tests(void);
    run_legion_tests();

#endif

#ifdef NAUT_CONFIG_NDPC_RT

    ndpc_rt_test();

#endif

#ifdef NAUT_CONFIG_NESL_RT

#endif


    printk("Nautilus main thread (core 0) yielding\n");
    while (1) {
        nk_yield();
    }

}

