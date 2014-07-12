#ifndef __NAUTILUS_H__
#define __NAUTILUS_H__

#include <printk.h>
#include <types.h>

#define panic(fmt, args...)         panic("PANIC at %s(%d): " fmt, __FILE__, __LINE__, ##args)

#ifdef NAUT_CONFIG_SERIAL_REDIRECT

#include <serial.h>
#define printk(fmt, args...) serial_print(fmt, ##args)
#define DEBUG_PRINT(fmt, args...)   serial_print("DEBUG: " fmt, ##args)
#define ERROR_PRINT(fmt, args...)   serial_print("ERROR at %s(%d): " fmt, __FILE__, __LINE__, ##args)
#else
#define DEBUG_PRINT(fmt, args...)   printk("DEBUG: " fmt, ##args)
#define ERROR_PRINT(fmt, args...)   printk("ERROR at %s(%d): " fmt, __FILE__, __LINE__, ##args)
#endif

#include <dev/ioapic.h>
#include <dev/timer.h>
#include <smp.h>

struct naut_info {
    struct sys_info * sys;
};


struct sys_info {

    struct cpu cpus[MAX_CPUS];
    struct ioapic ioapics[MAX_IOAPICS];

    uint32_t num_cpus;
    uint32_t num_ioapics;

    uint32_t bsp_id;

    uint8_t pic_mode_enabled;

    uint_t num_tevents;
    struct timer_event * time_events;
};

#endif
