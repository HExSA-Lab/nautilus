/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#define __NAUTILUS_MAIN__

#include <nautilus/nautilus.h>
#include <nautilus/paging.h>
#include <nautilus/idt.h>
#include <nautilus/spinlock.h>
#include <nautilus/mb_utils.h>
#include <nautilus/cpu.h>
#include <nautilus/msr.h>
#include <nautilus/mtrr.h>
#include <nautilus/cpuid.h>
#include <nautilus/smp.h>
#include <nautilus/irq.h>
#include <nautilus/thread.h>
#include <nautilus/fiber.h>
#include <nautilus/waitqueue.h>
#include <nautilus/task.h>
#include <nautilus/future.h>
#include <nautilus/group.h>
#include <nautilus/group_sched.h>
#include <nautilus/timer.h>
#include <nautilus/semaphore.h>
#include <nautilus/msg_queue.h>
#include <nautilus/idle.h>
#include <nautilus/percpu.h>
#include <nautilus/errno.h>
#include <nautilus/fpu.h>
#include <nautilus/random.h>
#include <nautilus/acpi.h>
#include <nautilus/atomic.h>
#include <nautilus/mm.h>
#include <nautilus/libccompat.h>
#include <nautilus/barrier.h>
#include <nautilus/vc.h>
#include <nautilus/dev.h>
#ifdef NAUT_CONFIG_PARTITION_SUPPORT
#include <nautilus/partition.h>
#endif
#include <nautilus/chardev.h>
#include <nautilus/blkdev.h>
#include <nautilus/netdev.h>
#include <nautilus/fs.h>
#include <nautilus/loader.h>
#include <nautilus/linker.h>
#include <nautilus/shell.h>
#include <nautilus/pmc.h>
#include <nautilus/prog.h>
#include <nautilus/cmdline.h>
#include <test/test.h>

#ifdef NAUT_CONFIG_ASPACES
#include <nautilus/aspace.h>
#endif

#ifdef NAUT_CONFIG_WATCHDOG
#include <nautilus/watchdog.h>
#endif

#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING 
#include <nautilus/gdb-stub.h>
#endif

#ifdef NAUT_CONFIG_NET_ETHERNET
#include <net/ethernet/ethernet_packet.h>
#include <net/ethernet/ethernet_agent.h>
#include <net/ethernet/ethernet_arp.h>
#endif

#ifdef NAUT_CONFIG_NET_COLLECTIVE_ETHERNET
#include <net/collective/ethernet/ethernet_collective.h>
#endif

#include <dev/apic.h>
#include <dev/pci.h>
#include <dev/hpet.h>
#include <dev/ioapic.h>
#include <dev/i8254.h>
#include <dev/ps2.h>
#ifdef NAUT_CONFIG_GPIO
#include <dev/gpio.h>
#endif
#include <dev/serial.h>
#include <dev/vga.h>
#ifdef NAUT_CONFIG_VIRTIO_PCI
#include <dev/virtio_pci.h>
#endif
#ifdef NAUT_CONFIG_MLX3_PCI
#include <dev/mlx3_ib.h>
#endif
#ifdef NAUT_CONFIG_E1000_PCI
#include <dev/e1000_pci.h>
#endif
#ifdef NAUT_CONFIG_E1000E_PCI
#include <dev/e1000e_pci.h>
#endif
#ifdef NAUT_CONFIG_RAMDISK
#include <dev/ramdisk.h>
#endif
#ifdef NAUT_CONFIG_ATA
#include <dev/ata.h>
#endif
#ifdef NAUT_CONFIG_EXT2_FILESYSTEM_DRIVER
#include <fs/ext2/ext2.h>
#endif
#ifdef NAUT_CONFIG_FAT32_FILESYSTEM_DRIVER
#include <fs/fat32/fat32.h>
#endif

#ifdef NAUT_CONFIG_NDPC_RT
#include <rt/ndpc/ndpc.h>
#endif

#ifdef NAUT_CONFIG_PALACIOS
#include <nautilus/vmm.h>
#endif

#ifdef NAUT_CONFIG_REAL_MODE_INTERFACE 
#include <nautilus/realmode.h>
#endif

#ifdef NAUT_CONFIG_VESA
#include <dev/vesa.h>
#endif

#ifdef NAUT_CONFIG_ENABLE_BDWGC
#include <gc/bdwgc/bdwgc.h>
#endif

#ifdef NAUT_CONFIG_PROVENANCE
#include <nautilus/provenance.h>
#endif

#ifdef NAUT_CONFIG_ENABLE_PDSGC
#include <gc/pdsgc/pdsgc.h>
#endif

#ifdef NAUT_CONFIG_CACHEPART
#include <nautilus/cachepart.h>
#endif


#ifdef NAUT_CONFIG_OPENMP_RT
#include <rt/openmp/openmp.h>
#endif

#ifdef NAUT_CONFIG_NESL_RT
#include <rt/nesl/nesl.h>
#endif

#ifdef NAUT_CONFIG_ENABLE_MONITOR
#include <nautilus/monitor.h>
#endif


extern spinlock_t printk_lock;



#define QUANTUM_IN_NS (1000000000ULL/NAUT_CONFIG_HZ)

struct nk_sched_config sched_cfg = {
    .util_limit = NAUT_CONFIG_UTILIZATION_LIMIT*10000ULL, // convert percent to 10^-6 units
    .sporadic_reservation =  NAUT_CONFIG_SPORADIC_RESERVATION*10000ULL, // ..
    .aperiodic_reservation = NAUT_CONFIG_APERIODIC_RESERVATION*10000ULL, // ..
    .aperiodic_quantum = QUANTUM_IN_NS,
    .aperiodic_default_priority = QUANTUM_IN_NS,
};




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


static void
runtime_init (void)
{

#ifdef NAUT_CONFIG_LEGION_RT
#ifdef NAUT_CONFIG_PROFILE
        nk_instrument_start();
        nk_instrument_calibrate(INSTR_CAL_LOOPS);
#endif

        extern void run_legion_tests(void);
        run_legion_tests();

#ifdef NAUT_CONFIG_PROFILE
        nk_instrument_end();
        nk_instrument_query();
#endif
#endif /* !NAUT_CONFIG_LEGION_RT */


#ifdef NAUT_CONFIG_NDPC_RT
        nk_ndpc_init();
#endif

#ifdef NAUT_CONFIG_NESL_RT
	nk_nesl_init();
#endif

#ifdef NAUT_CONFIG_OPENMP_RT
	nk_openmp_init();
#endif
	
}



#ifdef NAUT_CONFIG_PALACIOS_MGMT_VM
static void *mgmt_vm;
#endif

static int launch_vmm_environment()
{
#ifdef NAUT_CONFIG_PALACIOS
  nk_vmm_init("none");

#ifdef NAUT_CONFIG_PALACIOS_MGMT_VM
  extern int guest_start;
  mgmt_vm = nk_vmm_start_vm("management-vm",&guest_start,0xffffffff);
  if (!mgmt_vm) { 
    ERROR_PRINT("Failed to start embedded management VM\n");
    return -1;
  }

#endif
  printk("VM environment launched\n");
#endif
  return 0;
}



#define NAUT_WELCOME \
"Welcome to                                         \n" \
"    _   __               __   _  __                \n" \
"   / | / /____ _ __  __ / /_ (_)/ /__  __ _____    \n" \
"  /  |/ // __ `// / / // __// // // / / // ___/    \n" \
" / /|  // /_/ // /_/ // /_ / // // /_/ /(__  )     \n" \
"/_/ |_/ \\__,_/ \\__,_/ \\__//_//_/ \\__,_//____/  \n" \
"+===============================================+  \n" \
" Kyle C. Hale (c) 2014 | Northwestern University   \n" \
"+===============================================+  \n\n"

extern struct naut_info * smp_ap_stack_switch(uint64_t, uint64_t, struct naut_info*);

void
init (unsigned long mbd,
      unsigned long magic)
{
    struct naut_info * naut = &nautilus_info;


     // At this point, we have no FPU, so we need to be
    // sure that nothing we invoke could be using SSE or
    // similar due to compiler optimization
    
    nk_low_level_memset(naut, 0, sizeof(struct naut_info));

    vga_early_init();

    // At this point we have VGA output only
    
    fpu_init(naut, FPU_BSP_INIT);

    // Now we are safe to use optimized code that relies
    // on SSE

    spinlock_init(&printk_lock);

    setup_idt();

    nk_int_init(&(naut->sys));

    // Bring serial device up early so we can have output
    serial_early_init();

    nk_mtrr_init();

#ifdef NAUT_CONFIG_GPIO
    nk_gpio_init();
    nk_gpio_cpu_mask_add(1); // consider cpu 1 writes only
#endif

#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING 
    nk_gdb_init();
#endif


    nk_dev_init();
    nk_char_dev_init();
    nk_block_dev_init();
    nk_net_dev_init();

    nk_vc_print(NAUT_WELCOME);
    
    detect_cpu();

    /* setup the temporary boot-time allocator */
    mm_boot_init(mbd);

    naut->sys.mb_info = multiboot_parse(mbd, magic);
    if (!naut->sys.mb_info) {
        ERROR_PRINT("Problem parsing multiboot header\n");
    }

    nk_acpi_init();

    /* enumerate CPUs and initialize them */
    smp_early_init(naut);

    /* this will populate NUMA-related structures and 
     * also initialize the relevant ACPI tables if they exist */
    nk_numa_init();

    /* this will finish up the identity map */
    nk_paging_init(&(naut->sys.mem), mbd);

    /* setup the main kernel memory allocator */
    nk_kmem_init();

    // setup per-core area for BSP
    msr_write(MSR_GS_BASE, (uint64_t)naut->sys.cpus[0]);

    /* now we switch to the real kernel memory allocator, pages
     * allocated in the boot mem allocator are kept reserved */
    mm_boot_kmem_init();

#ifdef NAUT_CONFIG_ASPACES
    nk_aspace_init();
#endif

#ifdef NAUT_CONFIG_ENABLE_BDWGC
    // Bring up the BDWGC garbage collector if enabled
    nk_gc_bdwgc_init();
#endif

#ifdef NAUT_CONFIG_ENABLE_PDSGC
    // Bring up the PDSGC garbage collector if enabled
    nk_gc_pdsgc_init();
#endif

    disable_8259pic();

    i8254_init(naut);

    /* from this point on, we can use percpu macros (even if the APs aren't up) */

    sysinfo_init(&(naut->sys));

    ioapic_init(&(naut->sys));

    nk_wait_queue_init();

    nk_future_init();
    
    nk_timer_init();

    apic_init(naut->sys.cpus[0]);

    nk_rand_init(naut->sys.cpus[0]);

    nk_semaphore_init();
    
    nk_msg_queue_init();

    ps2_init(naut);

    pci_init(naut);

    nk_sched_init(&sched_cfg);

#ifdef NAUT_CONFIG_PROVENANCE
	nk_prov_init();
#endif

#ifdef NAUT_CONFIG_CACHEPART
#ifdef NAUT_CONFIG_CACHEPART_INTERRUPT
    nk_cache_part_init(NAUT_CONFIG_CACHEPART_THREAD_DEFAULT_PERCENT,
		       NAUT_CONFIG_CACHEPART_INTERRUPT_PERCENT);
#else
    nk_cache_part_init(NAUT_CONFIG_CACHEPART_THREAD_DEFAULT_PERCENT,0);
#endif
#endif    

    nk_thread_group_init();
    nk_group_sched_init();

    /* we now switch away from the boot-time stack in low memory */
    naut = smp_ap_stack_switch(get_cur_thread()->rsp, get_cur_thread()->rsp, naut);

    mm_boot_kmem_cleanup();


    smp_setup_xcall_bsp(naut->sys.cpus[0]);

    nk_cpu_topo_discover(naut->sys.cpus[0]); 
#ifdef NAUT_CONFIG_HPET
    nk_hpet_init();
#endif

#ifdef NAUT_CONFIG_PROFILE
    nk_instrument_init();
#endif

#ifdef NAUT_CONFIG_REAL_MODE_INTERFACE 
    nk_real_mode_init();
#endif

#ifdef NAUT_CONFIG_VESA
    vesa_init();
    // vesa_test();
#endif

    smp_bringup_aps(naut);

#ifdef NAUT_CONFIG_ENABLE_MONITOR
    nk_monitor_init();
#endif

    extern void nk_mwait_init(void);
    nk_mwait_init();

#ifdef NAUT_CONFIG_CXX_SUPPORT
    extern void nk_cxx_init(void);
    // Assuming we don't encounter C++ before here
    nk_cxx_init();
#endif 

    // reinit the early-initted devices now that
    // we have malloc and the device framework functional
    vga_init();
    serial_init();

    nk_sched_start();
    
#ifdef NAUT_CONFIG_FIBER_ENABLE
    nk_fiber_init();
    nk_fiber_startup();
#endif

#ifdef NAUT_CONFIG_CACHEPART
    nk_cache_part_start();
#endif

    sti();

    /* interrupts are now on */

    nk_vc_init();

    
#ifdef NAUT_CONFIG_VIRTUAL_CONSOLE_CHARDEV_CONSOLE
    nk_vc_start_chardev_console(NAUT_CONFIG_VIRTUAL_CONSOLE_CHARDEV_CONSOLE_NAME);
#endif


#ifdef NAUT_CONFIG_PARTITION_SUPPORT
    nk_partition_init(naut);
#endif

#ifdef NAUT_CONFIG_RAMDISK
    nk_ramdisk_init(naut);
#endif

#ifdef NAUT_CONFIG_ATA
    nk_ata_init(naut);
#endif

#ifdef NAUT_CONFIG_VIRTIO_PCI
    virtio_pci_init(naut);
#endif

#ifdef NAUT_CONFIG_MLX3_PCI
    mlx3_init(naut);
#endif

#ifdef NAUT_CONFIG_E1000_PCI
    e1000_pci_init(naut);
#endif

#ifdef NAUT_CONFIG_E1000E_PCI
    e1000e_pci_init(naut);
#endif

#ifdef NAUT_CONFIG_NET_ETHERNET
    nk_net_ethernet_packet_init();
    nk_net_ethernet_agent_init();
    nk_net_ethernet_arp_init();
#endif

#ifdef NAUT_CONFIG_NET_COLLECTIVE_ETHERNET
    nk_net_ethernet_collective_init();
#endif
    
    nk_fs_init();

#ifdef NAUT_CONFIG_EXT2_FILESYSTEM_DRIVER
#ifdef NAUT_CONFIG_RAMDISK_EMBED
    nk_fs_ext2_attach("ramdisk0","rootfs", 1);
#endif
#endif

#ifdef NAUT_CONFIG_FAT32_FILESYSTEM_DRIVER
#ifdef NAUT_CONFIG_RAMDISK_EMBED
    nk_fs_fat32_attach("ramdisk0","rootfs", 1);
#endif
#endif

    nk_linker_init(naut);
    nk_prog_init(naut);

    nk_loader_init();

    nk_pmc_init(naut);

    launch_vmm_environment();

    nk_cmdline_init(naut);
    nk_test_init(naut);

    nk_cmdline_dispatch(naut);

#ifdef NAUT_CONFIG_RUN_TESTS_AT_BOOT
    nk_run_tests(naut);
#endif

#ifdef NAUT_CONFIG_WATCHDOG
    nk_watchdog_init(NAUT_CONFIG_WATCHDOG_DEFAULT_TIME_MS * 1000000UL);
#endif
    
    nk_launch_shell("root-shell",0,0,0);

    runtime_init();


    printk("Nautilus boot thread yielding (indefinitely)\n");

    /* we don't come back from this */
    idle(NULL, NULL);
}
