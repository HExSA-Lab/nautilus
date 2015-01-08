#include <nautilus/nautilus.h>
#include <nautilus/cga.h>
#include <nautilus/paging.h>
#include <nautilus/idt.h>
#include <nautilus/spinlock.h>
#include <nautilus/mb_utils.h>
#include <nautilus/cpu.h>
#include <nautilus/msr.h>
#include <nautilus/cpuid.h>
#include <nautilus/smp.h>
#include <nautilus/irq.h>
#include <nautilus/thread.h>
#include <nautilus/idle.h>
#include <nautilus/percpu.h>
#include <nautilus/errno.h>
#include <nautilus/fpu.h>
#include <nautilus/random.h>
#include <nautilus/numa.h>
#include <nautilus/acpi.h>
#include <nautilus/atomic.h>

#include <nautilus/libccompat.h>

#include <nautilus/barrier.h>
#include <nautilus/rwlock.h>
#include <nautilus/condvar.h>

#include <dev/apic.h>
#include <dev/pci.h>
#include <dev/hpet.h>
#include <dev/ioapic.h>
#include <dev/timer.h>
#include <dev/i8254.h>
#include <dev/kbd.h>
#include <dev/serial.h>

#include <lib/liballoc_hooks.h>
#include <lib/liballoc.h>

#ifdef NAUT_CONFIG_NDPC_RT
#include "ndpc_preempt_threads.h"
#endif

static struct naut_info nautilus_info;
extern spinlock_t printk_lock;


inline struct naut_info*
nk_get_nautilus_info (void)
{
    return &nautilus_info;
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

    nk_int_init(&(naut->sys));

    serial_init();

    detect_cpu();

    nk_paging_init(&(naut->sys.mem), mbd);

    nk_acpi_init();

    init_liballoc_hooks();

    naut->sys.mb_info = multiboot_parse(mbd, magic);
    if (!naut->sys.mb_info) {
        ERROR_PRINT("Problem parsing multiboot header\n");
    }

    disable_8259pic();

    i8254_init(naut);

    smp_early_init(naut);

    // setup per-core area for BSP
    msr_write(MSR_GS_BASE, (uint64_t)naut->sys.cpus[0]);

    /* from this point on, we can use percpu macros (even if the APs aren't up) */

    sysinfo_init(&(naut->sys));

    ioapic_init(&(naut->sys));

    nk_timer_init(naut);

    apic_init(naut->sys.cpus[0]);

    fpu_init(naut);

    nk_rand_init(naut->sys.cpus[0]);

    kbd_init(naut);

    pci_init(naut);

    nk_sched_init();

    smp_setup_xcall_bsp(naut->sys.cpus[0]);

    nk_cpu_topo_discover(naut->sys.cpus[0]);

#ifdef NAUT_CONFIG_HPET
    nk_hpet_init();
#endif

#ifdef NAUT_CONFIG_PROFILE
    nk_instrument_init();
#endif

    smp_bringup_aps(naut);


#ifdef NAUT_CONFIG_CXX_SUPPORT
    extern void nk_cxx_init(void);
    // Assuming we don't encounter C++ before here
    nk_cxx_init();
#endif 

    /* interrupts on */
    sti();

#ifdef NAUT_CONFIG_NO_RT

    // screen saver
    nk_thread_start(side_screensaver, NULL, NULL, 0, TSTACK_DEFAULT, NULL, 1);

#endif /* !NAUT_CONFIG_NO_RT */


#ifdef NAUT_CONFIG_LEGION_RT

#ifdef NAUT_CONFIG_PROFILE
    nk_instrument_start();
    nk_instrument_calibrate();
#endif

    extern void run_legion_tests(void);
    run_legion_tests();

#ifdef NAUT_CONFIG_PROFILE
    nk_instrument_end();
    nk_instrument_query();
#endif


#endif /* !NAUT_CONFIG_LEGION_RT */

#ifdef NAUT_CONFIG_NDPC_RT

    ndpc_rt_test();

#endif

#ifdef NAUT_CONFIG_NESL_RT

    nesl_nautilus_main();

#endif

    printk("Nautilus boot thread yielding (indefinitely)\n");
    /* we don't come back from this */
    idle(NULL, NULL);
}
