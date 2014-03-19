#ifndef __PRINTK_H__
#define __PRINTK_H__

#define	PRINTK_BUFMAX	128

void panic (const char * fmt, ...);
int printk (const char * fmt, ...);

#endif
