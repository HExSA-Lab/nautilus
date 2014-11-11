#ifndef __PRINTK_H__
#define __PRINTK_H__

#ifdef __cplusplus
extern "C" {
#endif

#define	PRINTK_BUFMAX	128
#include <stdarg.h>
#include <naut_types.h>

void panic (const char * fmt, ...);
int printk (const char * fmt, ...);
int vprintk(const char * fmt, va_list args);
int early_printk (const char * fmt, va_list args);
int printk_color (uint8_t color, const char * fmt, ...);
void show_splash(void);
unsigned long simple_strtoul(const char*, char**, unsigned int);
long simple_strtol(const char*, char**, unsigned int);
unsigned long long simple_strtoull(const char*, char**, unsigned int);
long long simple_strtoll(const char*, char**, unsigned int);
int strict_strtoul(const char *cp, unsigned int base, unsigned long *res);
int strict_strtol(const char *cp, unsigned int base, long *res);
int strict_strtoull(const char *cp, unsigned int base, unsigned long long *res);
int strict_strtoll(const char *cp, unsigned int base, long long *res);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
int vscnprintf(char *buf, size_t size, const char *fmt, va_list args);
int snprintf(char * buf, size_t size, const char *fmt, ...);
int scnprintf(char * buf, size_t size, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char * buf, const char *fmt, ...);
int vsscanf(const char * buf, const char * fmt, va_list args);
int sscanf(const char * buf, const char * fmt, ...);

void warn_slowpath(const char * file, int line, const char * fmt, ...);

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

#ifdef __cplusplus
}
#endif
#endif
