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
 * http://xtack.sandia.gov/hobbes
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
#include <nautilus/nautilus.h>
#include <nautilus/smp.h>
#include <nautilus/paging.h>
#include <nautilus/irq.h>
#include <nautilus/msr.h>
#include <nautilus/mtrr.h>
#include <nautilus/gdt.h>
#include <nautilus/cpu.h>
#include <nautilus/naut_assert.h>
#include <nautilus/thread.h>
#include <nautilus/queue.h>
#include <nautilus/idle.h>
#include <nautilus/atomic.h>
#include <nautilus/numa.h>
#include <nautilus/mm.h>
#include <nautilus/fpu.h>
#include <nautilus/percpu.h>
#include <dev/ioapic.h>
#include <dev/apic.h>

#ifdef NAUT_CONFIG_ASPACES
#include <nautilus/aspace.h>
#endif

#ifdef NAUT_CONFIG_FIBER_ENABLE
#include <nautilus/fiber.h>
#endif

#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING 
#include <nautilus/gdb-stub.h>
#endif

#ifdef NAUT_CONFIG_CACHEPART
#include <nautilus/cachepart.h>
#endif


#ifndef NAUT_CONFIG_DEBUG_SMP
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define SMP_PRINT(fmt, args...) printk("SMP: " fmt, ##args)
#define SMP_DEBUG(fmt, args...) DEBUG_PRINT("SMP: " fmt, ##args)



extern struct cpu * smp_ap_stack_switch(uint64_t, uint64_t, struct cpu*);

static volatile unsigned smp_core_count = 1; // assume BSP is booted

extern addr_t init_smp_boot;
extern addr_t end_smp_boot;

uint8_t cpu_info_ready = 0;



int 
smp_early_init (struct naut_info * naut)
{
    return arch_early_init(naut);
}


static int
init_ap_area (struct ap_init_area * ap_area, 
              struct naut_info * naut,
              int core_num)
{
    memset((void*)ap_area, 0, sizeof(struct ap_init_area));

    /* setup pointer to this CPUs stack */
    uint32_t boot_stack_ptr = AP_BOOT_STACK_ADDR;

    ap_area->stack   = boot_stack_ptr;
    ap_area->cpu_ptr = naut->sys.cpus[core_num];

    /* protected mode temporary GDT */
    ap_area->gdt[2]      = 0x0000ffff;
    ap_area->gdt[3]      = 0x00cf9a00;
    ap_area->gdt[4]      = 0x0000ffff;
    ap_area->gdt[5]      = 0x00cf9200;

    // bases and limits for GDT and GDT64 are computed in the copy
    // that is placed in AP_INFO_AREA, hence not here

    /* long mode temporary GDT */
    ap_area->gdt64[1]    = 0x00af9a000000ffff;
    ap_area->gdt64[2]    = 0x00af92000000ffff;

    /* pointer to BSP's PML4 */
    ap_area->cr3         = read_cr3();

    /* pointer to our entry routine */
    ap_area->entry       = smp_ap_entry;

    return 0;
}


static int 
smp_wait_for_ap (struct naut_info * naut, unsigned int core_num)
{
    struct cpu * core = naut->sys.cpus[core_num];
#ifdef NAUT_CONFIG_XEON_PHI
    while (!core->booted) {
        udelay(1);
    }
#else
    BARRIER_WHILE(!core->booted);
#endif

    return 0;
}


int
smp_bringup_aps (struct naut_info * naut)
{
    struct ap_init_area * ap_area;

    addr_t boot_target     = (addr_t)&init_smp_boot;
    size_t smp_code_sz     = (addr_t)&end_smp_boot - boot_target;
    addr_t ap_trampoline   = (addr_t)AP_TRAMPOLINE_ADDR;
    uint8_t target_vec     = ap_trampoline >> 12U;
    struct apic_dev * apic = naut->sys.cpus[naut->sys.bsp_id]->apic;

    int status = 0; 
    int err = 0;
    int i, j, maxlvt;

    if (naut->sys.num_cpus == 1) {
        return 0;
    }

    maxlvt = apic_get_maxlvt(apic);

    SMP_DEBUG("Passing target page num %x to SIPI\n", target_vec);

    /* clear APIC errors */
    if (maxlvt > 3) {
        apic_write(apic, APIC_REG_ESR, 0);
    }
    apic_read(apic, APIC_REG_ESR);

    SMP_DEBUG("Copying in page for SMP boot code at (%p)...\n", (void*)ap_trampoline);
    memcpy((void*)ap_trampoline, (void*)boot_target, smp_code_sz);

    /* create an info area for APs */
    /* initialize AP info area (stack pointer, GDT info, etc) */
    ap_area = (struct ap_init_area*)AP_INFO_AREA;

    SMP_DEBUG("Passing AP area at %p\n", (void*)ap_area);

    /* START BOOTING AP CORES */
    
    /* we, of course, skip the BSP (NOTE: assuming it's 0...) */
    for (i = 0; i < naut->sys.num_cpus; i++) {
        int ret;

        /* skip the BSP */
        if (naut->sys.cpus[i]->is_bsp) {
            SMP_DEBUG("Skipping BSP (core id=%u, apicid=%u\n", i, naut->sys.cpus[i]->lapic_id);
            continue;
        }

        SMP_DEBUG("Booting secondary CPU %u\n", i);

        ret = init_ap_area(ap_area, naut, i);
        if (ret == -1) {
            ERROR_PRINT("Error initializing ap area\n");
            return -1;
        }


        /* Send the INIT sequence */
        SMP_DEBUG("sending INIT to remote APIC (0x%x)\n", naut->sys.cpus[i]->lapic_id);
        apic_send_iipi(apic, naut->sys.cpus[i]->lapic_id);

        /* wait for status to update */
        status = apic_wait_for_send(apic);

        mbarrier();

        /* 10ms delay */
        udelay(10000);

        /* deassert INIT IPI (level-triggered) */
        apic_deinit_iipi(apic, naut->sys.cpus[i]->lapic_id);

        for (j = 1; j <= 2; j++) {
            if (maxlvt > 3) {
                apic_write(apic, APIC_REG_ESR, 0);
            }
            apic_read(apic, APIC_REG_ESR);

            SMP_DEBUG("Sending SIPI %u to core %u (vec=%x)\n", j, i, target_vec);

            /* send the startup signal */
            apic_send_sipi(apic, naut->sys.cpus[i]->lapic_id, target_vec);

            udelay(300);

            status = apic_wait_for_send(apic);

            udelay(200);

            err = apic_read(apic, APIC_REG_ESR) & 0xef;

            if (status || err) {
                break;
            }

            /* if it already booted up, we don't need to send the 2nd SIPI */
            if (naut->sys.cpus[i]->booted == 1) {
                break;
            }

        }

        if (status) {
            ERROR_PRINT("APIC wasn't delivered!\n");
        }

        if (err) {
            ERROR_PRINT("ERROR delivering SIPI\n");
        }

        /* wait for AP to set its boot flag */
        smp_wait_for_ap(naut, i);

        SMP_DEBUG("Bringup for core %u done.\n", i);
    }

    BARRIER_WHILE(smp_core_count != naut->sys.num_cpus);

    SMP_DEBUG("ALL CPUS BOOTED\n");

    /* we can now use gs-based percpu data */
    cpu_info_ready = 1;

    return (status|err);
}


extern struct idt_desc idt_descriptor;
extern struct gdt_desc64 gdtr64;

static int xcall_handler(excp_entry_t * e, excp_vec_t v, void *state);


static int
smp_xcall_init_queue (struct cpu * core)
{
    core->xcall_q = nk_queue_create();
    if (!core->xcall_q) {
        ERROR_PRINT("Could not allocate xcall queue on cpu %u\n", core->id);
        return -1;
    }

    return 0;
}


int
smp_setup_xcall_bsp (struct cpu * core)
{
    SMP_PRINT("Setting up cross-core IPI event queue\n");
    smp_xcall_init_queue(core);

    if (register_int_handler(IPI_VEC_XCALL, xcall_handler, NULL) != 0) {
        ERROR_PRINT("Could not assign interrupt handler for XCALL on core %u\n", core->id);
        return -1;
    }

    return 0;
}


static int
smp_ap_setup (struct cpu * core)
{
    // Note that any use of SSE/AVX, for example produced by
    // clang/llvm optimation, that happens before fpu_init will
    // cause a panic.  Initialize FPU ASAP.
    
    // setup IDT
    lidt(&idt_descriptor);

    // setup GDT
    lgdt64(&gdtr64);

    uint64_t core_addr = (uint64_t) core->system->cpus[core->id];

    // set GS base (for per-cpu state)
    msr_write(MSR_GS_BASE, (uint64_t)core_addr);

    fpu_init(nk_get_nautilus_info(), FPU_AP_INIT);
    
    if (nk_mtrr_init_ap()) {
	ERROR_PRINT("Could not initialize MTRRs for core %u\n",core->id);
	return -1;
    }

#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING 
    if (nk_gdb_init_ap() != 0) {
        ERROR_PRINT("Could not initialize remote debugging for core %u\n", core->id);
	return -1;
    }
#endif

#ifdef NAUT_CONFIG_ASPACES
    if (nk_aspace_init_ap()) {
        ERROR_PRINT("Could not set up aspaces for core %u\n",core->id);
    }
#endif
    
    apic_init(core);

    if (smp_xcall_init_queue(core) != 0) {
        ERROR_PRINT("Could not setup xcall for core %u\n", core->id);
        return -1;
    }
    
    extern struct nk_sched_config sched_cfg;

    if (nk_sched_init_ap(&sched_cfg) != 0) {
        ERROR_PRINT("Could not setup scheduling for core %u\n", core->id);
        return -1;
    }

#ifdef NAUT_CONFIG_FIBER_ENABLE
    if (nk_fiber_init_ap() != 0) {
        ERROR_PRINT("Could not setup fiber thread for core %u\n", core->id);
        return -1;
    }
#endif

#ifdef NAUT_CONFIG_CACHEPART
    if (nk_cache_part_init_ap() != 0) {
        ERROR_PRINT("Could not setup cache partitioning for core %u\n", core->id);
        return -1;
    }
#endif

    return 0;
}


extern void nk_rand_init(struct cpu*);

static void
smp_ap_finish (struct cpu * core)
{
    nk_rand_init(core);

    nk_cpu_topo_discover(core);

    PAUSE_WHILE(atomic_cmpswap(core->booted, 0, 1) != 0);

#ifndef NAUT_CONFIG_HVM_HRT
    atomic_inc(smp_core_count);

    /* wait on all the other cores to boot up */
    BARRIER_WHILE(smp_core_count != core->system->num_cpus);
#endif

    nk_sched_start();

#ifdef NAUT_CONFIG_FIBER_ENABLE
    nk_fiber_startup();
#endif

#ifdef NAUT_CONFIG_CACHEPART
    if (nk_cache_part_start_ap() != 0) {
        ERROR_PRINT("Could not start cache partitioning for core %u\n", core->id);
    }
#endif

    SMP_DEBUG("Core %u ready - enabling interrupts\n", core->id);

    sti();

#ifdef NAUT_CONFIG_PROFILE
    nk_instrument_calibrate(INSTR_CAL_LOOPS);
#endif
}


extern void idle(void* in, void**out);

void 
smp_ap_entry (struct cpu * core) 
{ 
    struct cpu * my_cpu;
    SMP_DEBUG("Core %u starting up\n", core->id);
    if (smp_ap_setup(core) < 0) {
        panic("Error setting up AP!\n");
    }

    /* we should now be able to pull our CPU pointer out of GS
     * This is important, because the stack will be clobbered
     * for the next CPU boot! 
     */
    my_cpu = get_cpu();
    SMP_DEBUG("CPU (AP) %u operational\n", my_cpu->id);

    // switch from boot stack to my new stack (allocated in thread_init)
    nk_thread_t * cur = get_cur_thread();

    /* 
     * we have to call into assembly since GCC 
     * wont let us clobber rbp. Note how we reassign
     * my_cpu. This is so we don't lose it in the
     * switch (it's sitting on the stack!)
     */
    my_cpu = smp_ap_stack_switch(cur->rsp, cur->rsp, my_cpu);

    // wait for the other cores and turn on interrupts
    smp_ap_finish(my_cpu);
    
    ASSERT(irqs_enabled());

    sti();

    idle(NULL, NULL);
}


uint32_t
nk_get_num_cpus (void)
{
    struct sys_info * sys = per_cpu_get(system);
    return sys->num_cpus;
}

static void
init_xcall (struct nk_xcall * x, void * arg, nk_xcall_func_t fun, int wait)
{
    x->data       = arg;
    x->fun        = fun;
    x->xcall_done = 0;
    x->has_waiter = wait;
}


static inline void
wait_xcall (struct nk_xcall * x)
{

    while (atomic_cmpswap(x->xcall_done, 1, 0) != 1) {
        asm volatile ("pause");
    }
}


static inline void
mark_xcall_done (struct nk_xcall * x) 
{
    atomic_cmpswap(x->xcall_done, 0, 1);
}


static int
xcall_handler (excp_entry_t * e, excp_vec_t v, void *state) 
{
    nk_queue_t * xcq = per_cpu_get(xcall_q); 
    struct nk_xcall * x = NULL;
    nk_queue_entry_t * elm = NULL;

    if (!xcq) {
        ERROR_PRINT("Badness: no xcall queue on core %u\n", my_cpu_id());
        goto out_err;
    }

    elm = nk_dequeue_first_atomic(xcq);
    x = container_of(elm, struct nk_xcall, xcall_node);
    if (!x) {
        ERROR_PRINT("No XCALL request found on core %u\n", my_cpu_id());
        goto out_err;
    }

    if (x->fun) {

        // we ack the IPI before calling the handler funciton,
        // because it may end up blocking (e.g. core barrier)
        IRQ_HANDLER_END(); 

        x->fun(x->data);

        /* we need to notify the waiter we're done */
        if (x->has_waiter) {
            mark_xcall_done(x);
        }

    } else {
        ERROR_PRINT("No XCALL function found on core %u\n", my_cpu_id());
        goto out_err;
    }


    return 0;

out_err:
    IRQ_HANDLER_END();
    return -1;
}


/* 
 * smp_xcall
 *
 * initiate cross-core call. 
 * 
 * @cpu_id: the cpu to execute the call on
 * @fun: the function to invoke
 * @arg: the argument to the function
 * @wait: this function should block until the reciever finishes
 *        executing the function
 *
 */
int
smp_xcall (cpu_id_t cpu_id, 
           nk_xcall_func_t fun,
           void * arg,
           uint8_t wait)
{
    struct sys_info * sys = per_cpu_get(system);
    nk_queue_t * xcq  = NULL;
    struct nk_xcall x;
    uint8_t flags;

    SMP_DEBUG("Initiating SMP XCALL from core %u to core %u\n", my_cpu_id(), cpu_id);

    if (cpu_id > nk_get_num_cpus()) {
        ERROR_PRINT("Attempt to execute xcall on invalid cpu (%u)\n", cpu_id);
        return -1;
    }

    if (cpu_id == my_cpu_id()) {

        flags = irq_disable_save();
        fun(arg);
        irq_enable_restore(flags);

    } else {
        struct nk_xcall * xc = &x;

        if (!wait) {
            xc = &(sys->cpus[cpu_id]->xcall_nowait_info);
        }

        init_xcall(xc, arg, fun, wait);

        xcq = sys->cpus[cpu_id]->xcall_q;
        if (!xcq) {
            ERROR_PRINT("Attempt by cpu %u to initiate xcall on invalid xcall queue (for cpu %u)\n", 
                        my_cpu_id(),
                        cpu_id);
            return -1;
        }

        flags = irq_disable_save();

        if (!nk_queue_empty_atomic(xcq)) {
            ERROR_PRINT("XCALL queue for core %u is not empty, bailing\n", cpu_id);
            irq_enable_restore(flags);
            return -1;
        }

        nk_enqueue_entry_atomic(xcq, &(xc->xcall_node));

        irq_enable_restore(flags);

        struct apic_dev * apic = per_cpu_get(apic);

        apic_ipi(apic, sys->cpus[cpu_id]->apic->id, IPI_VEC_XCALL);

        if (wait) {
            wait_xcall(xc);
        }

    }

    return 0;
}
