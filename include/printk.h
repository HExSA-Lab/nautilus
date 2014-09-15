#ifndef __PRINTK_H__
#define __PRINTK_H__

#define	PRINTK_BUFMAX	128
#include <stdarg.h>
#include <types.h>

void panic (const char * fmt, ...);
int printk (const char * fmt, ...);
int early_printk (const char * fmt, va_list args);
int printk_color (uint8_t color, const char * fmt, ...);
void show_splash(void);

#define NAUT_WELCOME \
"Welcome to                                         \n" \
"    _   __               __   _  __                \n" \
"   / | / /____ _ __  __ / /_ (_)/ /__  __ _____    \n" \
"  /  |/ // __ `// / / // __// // // / / // ___/    \n" \
" / /|  // /_/ // /_/ // /_ / // // /_/ /(__  )     \n" \
"/_/ |_/ \\__,_/ \\__,_/ \\__//_//_/ \\__,_//____/  \n" \
"+===============================================+  \n" \
" Kyle C. Hale (c) 2014 | Northwestern University   \n" \
"+===============================================+  \n\n"

#endif
