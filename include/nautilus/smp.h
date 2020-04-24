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
#ifndef __SMP_H__
#define __SMP_H__

#ifndef __ASSEMBLER__ 

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/msr.h>

/******* EXTERNAL INTERFACE *********/

uint32_t nk_get_num_cpus (void);
uint8_t nk_get_cpu_by_lapicid (uint8_t lapicid);

/******* !EXTERNAL INTERFACE! *********/



#include <dev/apic.h>
#include <nautilus/spinlock.h>
#include <nautilus/mm.h>
#include <nautilus/queue.h>

struct naut_info;
struct nk_topo_params;
struct nk_cpu_coords;

struct nk_queue;
struct nk_thread;

//typedef struct nk_wait_queue nk_wait_queue_t;
//typedef struct nk_thread nk_thread_t;
typedef void (*nk_xcall_func_t)(void * arg);
typedef uint32_t cpu_id_t;


struct nk_xcall {
    nk_queue_entry_t xcall_node;
    void * data;
    nk_xcall_func_t fun;
    uint8_t xcall_done;
    uint8_t has_waiter;
};


#ifdef NAUT_CONFIG_PROFILE
    struct nk_instr_data;
#endif

struct cpu {
    struct nk_thread * cur_thread;             /* +0  KCH: this must be first! */
    // track whether we are in an interrupt, nested or otherwise
    // this is intended for use by the scheduler (any scheduler)
    uint16_t interrupt_nesting_level;          /* +8  PAD: DO NOT MOVE */
    // track whether the scheduler (any scheduler) should be able to preempt
    // the current thread (whether cooperatively or via any
    uint16_t preempt_disable_level;            /* +10 PAD: DO NOT MOVE */

    // Track statistics of interrupts and exceptions
    // these counts are updated by the low-level interrupt handling code
    uint64_t interrupt_count;                  /* +16 PAD: DO NOT MOVE */
    uint64_t exception_count;                  /* +24 PAD: DO NOT MOVE */

    // this field is only used if aspace are enabled
    struct nk_aspace    *cur_aspace;            /* +32 PAD: DO NOT MOVE */

    #if NAUT_CONFIG_FIBER_ENABLE
    struct nk_fiber_percpu_state *f_state; /* Fiber state for each CPU */
    #endif

    #ifdef NAUT_CONFIG_WATCHDOG
    uint64_t watchdog_count; /* Number of times the watchdog timer has been triggered */
    #endif

    cpu_id_t id;
    uint32_t lapic_id;   
    uint8_t enabled;
    uint8_t is_bsp;
    uint32_t cpu_sig;
    uint32_t feat_flags;

    volatile uint8_t booted;

    struct apic_dev * apic;

    struct sys_info * system;

    spinlock_t lock;

    struct nk_sched_percpu_state *sched_state;

    nk_queue_t * xcall_q;
    struct nk_xcall xcall_nowait_info;

    ulong_t cpu_khz; 
    
    /* NUMA info */
    struct nk_topo_params * tp;
    struct nk_cpu_coords * coord;
    struct numa_domain * domain;

    struct kmem_data kmem;


    struct nk_rand_info * rand;

    /* temporary */
#ifdef NAUT_CONFIG_PROFILE
    struct nk_instr_data * instr_data;
#endif
};


struct ap_init_area {
    uint32_t stack;  // 0
    uint32_t rsvd; // to align the GDT on 8-byte boundary // 4
    uint32_t gdt[6] ; // 8
    uint16_t gdt_limit; // 32
    uint32_t gdt_base; // 34
    uint16_t rsvd1; // 38
    uint64_t gdt64[3]; // 40
    uint16_t gdt64_limit; // 64
    uint64_t gdt64_base; // 66
    uint64_t cr3; // 74
    struct cpu * cpu_ptr; // 82

    void (*entry)(struct cpu * core); // 90

} __packed;


int smp_early_init(struct naut_info * naut);
int smp_bringup_aps(struct naut_info * naut);
int smp_xcall(cpu_id_t cpu_id, nk_xcall_func_t fun, void * arg, uint8_t wait);
void smp_ap_entry (struct cpu * core);
int smp_setup_xcall_bsp (struct cpu * core);


/* ARCH-SPECIFIC */

int arch_early_init (struct naut_info * naut);



#ifdef __cplusplus
}
#endif

#endif /* !__ASSEMBLER__ */

#define AP_TRAMPOLINE_ADDR 0xf000 
#define AP_BOOT_STACK_ADDR 0x1000
#define AP_INFO_AREA       0x2000

#define BASE_MEM_LAST_KILO 0x9fc00
#define BIOS_ROM_BASE      0xf0000
#define BIOS_ROM_END       0xfffff


#endif
