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
#ifndef __NAUTILUS_H__
#define __NAUTILUS_H__

#include <nautilus/percpu.h>
#include <nautilus/printk.h>
#include <dev/serial.h>
#include <nautilus/naut_types.h>
#include <nautilus/instrument.h>
#include <nautilus/vc.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_PRINT(fmt, args...)   nk_vc_log_wrap("CPU %d: DEBUG: " fmt, my_cpu_id(),##args)
#define ERROR_PRINT(fmt, args...)   nk_vc_log_wrap("CPU %d: ERROR at %s(%lu): " fmt, my_cpu_id(),  __FILE__, __LINE__, ##args)
#define WARN_PRINT(fmt, args...)    nk_vc_log_wrap("CPU %d: WARNING: " fmt, my_cpu_id(), ##args)
#define INFO_PRINT(fmt, args...)    nk_vc_log_wrap("CPU %d: " fmt, my_cpu_id(), ##args)

#define panic(fmt, args...)         panic("PANIC at %s(%d): " fmt, __FILE__, __LINE__, ##args)

#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif


#include <dev/ioapic.h>
#include <nautilus/smp.h>
#include <nautilus/paging.h>
#include <nautilus/limits.h>
#include <nautilus/naut_assert.h>
#include <nautilus/barrier.h>
#include <nautilus/list.h>
#include <nautilus/numa.h>


struct ioapic;
struct irq_mapping {
    struct ioapic * ioapic;
    uint8_t vector;
    uint8_t assigned;
};


struct nk_int_info {
    struct list_head int_list; /* list of interrupts from MP spec */
    struct list_head bus_list; /* list of buses from MP spec */

    struct irq_mapping irq_map[256];
};

struct hpet_dev;
struct nk_locality_info;
struct sys_info {

    struct cpu * cpus[NAUT_CONFIG_MAX_CPUS];
    struct ioapic * ioapics[NAUT_CONFIG_MAX_IOAPICS];

    uint32_t num_cpus;
    uint32_t num_ioapics;

    uint64_t flags;
#define NK_SYS_LEGACY 1  // system has dual PIC and ISA devices

    nk_barrier_t * core_barrier;

    struct nk_mem_info mem;

    uint32_t bsp_id;

    uint8_t pic_mode_enabled;

    struct pci_info * pci;
    struct hpet_dev * hpet;

    struct multiboot_info * mb_info;

    struct nk_int_info int_info;

    struct nk_locality_info locality_info;
};

struct naut_info {
    struct sys_info sys;
};

#ifdef __NAUTILUS_MAIN__
struct naut_info nautilus_info;
#else
extern struct naut_info nautilus_info;
#endif

static inline struct naut_info*
nk_get_nautilus_info (void)
{
    return &nautilus_info;
}

#ifdef NAUT_CONFIG_XEON_PHI
#include <arch/k1om/main.h>
#elif defined NAUT_CONFIG_HVM_HRT
#include <arch/hrt/main.h>
#elif defined NAUT_CONFIG_X86_64_HOST
#include <arch/x64/main.h>
#else
#error "Unsupported architecture"
#endif

#ifdef __cplusplus
}
#endif
                                               

#endif
