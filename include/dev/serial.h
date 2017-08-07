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
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *         Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <stddef.h>
#include <stdarg.h>

#ifndef SERIAL_PRINT_DEBUG_LEVEL
#define SERIAL_PRINT_DEBUG_LEVEL  10
#endif

void serial_putchar(unsigned char c);
void serial_write(const char *buf);
void serial_puts(const char *buf);
void serial_print(const char * format, ...);

void serial_printlevel(int level, const char * format, ...);

void serial_print_poll(const char *format, ...);

#ifdef NAUT_CONFIG_SERIAL_DEBUGGER
int serial_debugger_put(uint8_t c);
int serial_debugger_get(uint8_t *c);
#endif

void  serial_early_init(void);

void  serial_init(void);

#endif
