#ifndef __NAUTILUS_H__
#define __NAUTILUS_H__

#include <printk.h>
#include <serial.h>
#include <types.h>


#ifdef NAUT_CONFIG_SERIAL_REDIRECT
#include <serial.h>
#define printk(fmt, args...)        serial_print_redirect(fmt, ##args)
#define DEBUG_PRINT(fmt, args...)   serial_print_redirect("DEBUG: " fmt, ##args)
#define ERROR_PRINT(fmt, args...)   serial_print_redirect("ERROR at %s(%d): " fmt, __FILE__, __LINE__, ##args)
#define panic(fmt, args...)         panic_serial("PANIC at %s(%d): " fmt, __FILE__, __LINE__, ##args)
#else
#define panic(fmt, args...)         panic("PANIC at %s(%d): " fmt, __FILE__, __LINE__, ##args)
#define DEBUG_PRINT(fmt, args...)   printk("DEBUG: " fmt, ##args)
#define ERROR_PRINT(fmt, args...)   printk("ERROR at %s(%d): " fmt, __FILE__, __LINE__, ##args)
#endif

#ifndef NUAT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif


#include <dev/ioapic.h>
#include <dev/timer.h>
#include <smp.h>
#include <paging.h>

struct sys_info {

    struct cpu * cpus[MAX_CPUS];
    struct ioapic ioapics[MAX_IOAPICS];

    uint32_t num_cpus;
    uint32_t num_ioapics;

    struct mem_info mem;

    uint32_t bsp_id;

    uint8_t pic_mode_enabled;

    uint_t num_tevents;
    struct timer_event * time_events;
};

struct naut_info {
    struct sys_info sys;
};

void main (unsigned long mbd, unsigned long magic) __attribute__((section (".text")));

#define NAUT_WELCOME \
"Welcome to \n" \
"    _   __               __   _  __            \n" \
"   / | / /____ _ __  __ / /_ (_)/ /__  __ _____\n" \
"  /  |/ // __ `// / / // __// // // / / // ___/\n" \
" / /|  // /_/ // /_/ // /_ / // // /_/ /(__  ) \n" \
"/_/ |_/ \\__,_/ \\__,_/ \\__//_//_/ \\__,_//____/  \n" \
"+===============================================+\n" \
" Kyle C. Hale (c) 2014 | Northwestern University \n" \
"+===============================================+\n"
                                               

#endif
