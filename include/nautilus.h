#ifndef __NAUTILUS_H__
#define __NAUTILUS_H__

#include <printk.h>
#include <types.h>

//#define DEBUG_PRINT(fmt, args...)   printk("DEBUG: " fmt, ##args)
//#define ERROR_PRINT(fmt, args...)   printk("ERROR: " fmt, ##args)
#define DEBUG_PRINT(fmt, args...)   printk((fmt), ##args)
#define ERROR_PRINT(fmt, args...)   printk((fmt), ##args)
//define PrintError(vm, vcore, fmt, args...)  V3_Print(vm, vcore, "ERROR at %s(%d): " fmt, __FILE__, __LINE__, ##args)

#endif
