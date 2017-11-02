/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

#ifndef _CONFIG_H
#define _CONFIG_H 1

/* Use this macro for prototypes */
#if __STDC__ 
#define PROTO_(s) s
#else
#define PROTO_(s) ()
#endif /* __STDC__ */

#ifndef __STDC__
#define const
#endif  /* not __STDC__ */

/* assert macros stolen from assert.h.  They did it wrong (not hygenic). */
#ifdef ASSERTS
#define _assert(ex)	{if (!(ex)){_fprintf(stderr,"vinterp: assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);vinterp_exit(1);}}
#define assert(ex)	do { _assert(ex) } while(0)
#else
#define _assert(ex)
#define assert(ex)
#endif

/* The get_bucket_num() routine in vstack.c uses a hack that relies
 * on knowing which are the most and least significant byte in the
 * floating point representation of a number.  
 * FP_LITTLE_ENDIAN should be true if the exponent is in the final
 * byte, false if the exponent is in the first byte.
 * We assume IEEE format (except for vax and cray -- see vstack.c).
 */
#ifdef SPEEDHACKS
#undef FP_LITTLE_ENDIAN
/* These architectures can be little endian */
#if mips | alpha | __i860 | i386
/* ...but SGI boxes aren't. */
#ifndef sgi
#define FP_LITTLE_ENDIAN 1
#endif			/* not sgi */
#endif			/* mips | alpha | i860 */
#endif 			/* SPEEDHACKS */

/* MPI foo */
#if MPI
extern const int Self;
#define _printf if (Self == 0) printf
#define _fprintf if (Self == 0) fprintf
#define _putc if (Self == 0) safeputc
#else
#ifdef __NAUTILUS__
#include <nautilus/nautilus.h>
#include <nautilus/naut_types.h>
#include <nautilus/naut_string.h>
#include <nautilus/libccompat.h>
#ifndef NAUT_CONFIG_NESL_RT_DEBUG
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define INFO(fmt, args...)     INFO_PRINT("nesl: " fmt, ##args)
#define DEBUG(fmt, args...)    DEBUG_PRINT("nesl: " fmt, ##args)
#define ERROR(fmt, args...)    ERROR_PRINT("nesl: " fmt, ##args)

#define PRINT(fmt, args...)    nk_vc_printf(fmt, ##args)
#define PUTC(c)    nk_vc_putchar(c)
#define printf PRINT
#define fprintf(f,fmt, args...) PRINT(fmt, ##args)
#define _printf PRINT
#define _fprintf fprintf
#define _putc PUTC

#else
#define _printf printf
#define _fprintf fprintf
#define _putc safeputc
#endif
#endif

/* posix bites */
#ifdef __STDC__
#define safeputc(c,s) putc((int)c,s)
#else
#define safeputc putc
#endif


#endif	/* _CONFIG */
