#ifndef __NAUTILUS_H__
#define __NAUTILUS_H__

#include <printk.h>
#include <serial.h>
#include <naut_types.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifdef NAUT_CONFIG_SERIAL_REDIRECT
#include <serial.h>
#define DEBUG_PRINT(fmt, args...)   serial_print_redirect("DEBUG: " fmt, ##args)
#define ERROR_PRINT(fmt, args...)   serial_print_redirect("ERROR at %s(%d): " fmt, __FILE__, __LINE__, ##args)
#else
#define DEBUG_PRINT(fmt, args...)   printk("DEBUG: " fmt, ##args)
#define ERROR_PRINT(fmt, args...)   printk("ERROR at %s(%d): " fmt, __FILE__, __LINE__, ##args)
#endif

#define panic(fmt, args...)         panic("PANIC at %s(%d): " fmt, __FILE__, __LINE__, ##args)

#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif


#include <dev/ioapic.h>
#include <dev/timer.h>
#include <smp.h>
#include <paging.h>
#include <limits.h>


struct sys_info {

    struct cpu * cpus[MAX_CPUS];
    struct ioapic ioapics[MAX_IOAPICS];

    uint32_t num_cpus;
    uint32_t num_ioapics;

    struct nk_mem_info mem;

    uint32_t bsp_id;

    uint8_t pic_mode_enabled;

    uint_t num_tevents;
    struct timer_event * time_events;

    struct pci_info * pci;
};

struct naut_info {
    struct sys_info sys;
};

void main (unsigned long mbd, unsigned long magic) __attribute__((section (".text")));
inline struct naut_info* nk_get_nautilus_info(void);

#ifdef __cplusplus
}
#endif
                                               

#endif
