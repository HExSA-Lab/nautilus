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
#ifndef __SERIAL_H__
#define __SERIAL_H__

#define COM1_3_IRQ 4
#define COM2_4_IRQ 3
#define COM1_ADDR 0x3F8
#define COM2_ADDR 0x2F8
#define COM3_ADDR 0x3E8
#define COM4_ADDR 0x2E8

#ifndef SERIAL_PRINT_DEBUG_LEVEL
#define SERIAL_PRINT_DEBUG_LEVEL  10
#endif

#include <stddef.h>
#include <stdarg.h>
#include <dev/serial.h>

void serial_putchar(unsigned char c);
void serial_write(const char *buf);
void serial_puts(const char *buf);

void serial_print(const char * format, ...);
void serial_printlevel(int level, const char * format, ...);
void serial_print_list(const char * format, va_list ap);
void __serial_print(const char * format, va_list ap);


void serial_print_hex(unsigned char x);
void serial_mem_dump(unsigned char *start, int n);

uint8_t serial_get_irq(void);
void serial_init(void);
void serial_init_addr(unsigned short io_addr);

#endif
