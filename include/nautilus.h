#ifndef __NAUTILUS_H__
#define __NAUTILUS_H__

#include <printk.h>
#include <types.h>

#define DEBUG_PRINT(fmt, args...)   printk("DEBUG: " fmt, ##args)
#define ERROR_PRINT(fmt, args...)   printk("ERROR at %s(%d): " fmt, __FILE__, __LINE__, ##args)

#endif
