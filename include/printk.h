#ifndef __PRINTK_H__
#define __PRINTK_H__

#define	PRINTK_BUFMAX	128
#include <stdarg.h>

void panic (const char * fmt, ...);
int printk (const char * fmt, ...);
int early_printk (const char * fmt, va_list args);

#endif
