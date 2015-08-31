#ifndef __NAUTILUS_H__
#define __NAUTILUS_H__

#include <nautilus/printk.h>
#include <dev/serial.h>
#include <nautilus/naut_types.h>
#include <nautilus/instrument.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifdef NAUT_CONFIG_SERIAL_REDIRECT
#include <dev/serial.h>
#define DEBUG_PRINT(fmt, args...)   serial_print_redirect("DEBUG: " fmt, ##args)
#define ERROR_PRINT(fmt, args...)   serial_print_redirect("ERROR at %s(%d): " fmt, __FILE__, __LINE__, ##args)
#define WARN_PRINT(fmt, args...)    serial_print_redirect("WARNING: " fmt, ##args)
#else
#define DEBUG_PRINT(fmt, args...)   printk("DEBUG: " fmt, ##args)
#define ERROR_PRINT(fmt, args...)   printk("ERROR at %s(%d): " fmt, __FILE__, __LINE__, ##args)
#define WARN_PRINT(fmt, args...)    printk("WARNING: " fmt, ##args)
#endif

#define panic(fmt, args...)         panic("PANIC at %s(%d): " fmt, __FILE__, __LINE__, ##args)

#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif


#include <dev/ioapic.h>
#include <dev/timer.h>
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

    nk_barrier_t * core_barrier;

    struct nk_mem_info mem;

    uint32_t bsp_id;

    uint8_t pic_mode_enabled;

    uint_t num_tevents;
    struct nk_timer_event * time_events;

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
