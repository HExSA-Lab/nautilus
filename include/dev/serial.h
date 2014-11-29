#ifndef __SERIAL_H__
#define __SERIAL_H__

#define COM1_IRQ 4
#define DEFAULT_SERIAL_ADDR 0x3F8

#ifndef SERIAL_PRINT_DEBUG_LEVEL
#define SERIAL_PRINT_DEBUG_LEVEL  10
#endif

#include <stddef.h>
#include <stdarg.h>
#include <dev/serial.h>

void serial_putchar(unsigned char c);

void serial_print_redirect(const char * format, ...);
void serial_print(const char * format, ...);
void panic_serial(const char * fmt, ...);
void serial_printlevel(int level, const char * format, ...);
void serial_print_list(const char * format, va_list ap);
void __serial_print(const char * format, va_list ap);

void serial_putln(char * line); 
void serial_putlnn(char * line, int len);


void serial_print_hex(unsigned char x);
void serial_mem_dump(unsigned char *start, int n);

void serial_init(void);
void serial_init_addr(unsigned short io_addr);

#endif
