/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __PRINTK_H__
#define __PRINTK_H__

#ifdef __cplusplus
extern "C" {
#endif

#define	PRINTK_BUFMAX	128
#include <stdarg.h>
#include <nautilus/naut_types.h>

void panic (const char * fmt, ...) __attribute__((noreturn));
int printk (const char * fmt, ...);
int vprintk(const char * fmt, va_list args);
int early_printk (const char * fmt, va_list args);
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


#ifdef __cplusplus
}
#endif
#endif
