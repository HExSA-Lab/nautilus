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
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __STRING_H__
#define __STRING_H__

#ifndef __LEGION__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/naut_types.h>

#define _U	0x01	/* upper */
#define _L	0x02	/* lower */
#define _D	0x04	/* digit */
#define _C	0x08	/* cntrl */
#define _P	0x10	/* punct */
#define _S	0x20	/* white space (space/lf/tab) */
#define _X	0x40	/* hex digit */
#define _SP	0x80	/* hard space (0x20) */

extern unsigned char _ctype[];

#define __ismask(x) (_ctype[(int)(unsigned char)(x)])

#define isalnum(c)	((__ismask(c)&(_U|_L|_D)) != 0)
#define isalpha(c)	((__ismask(c)&(_U|_L)) != 0)
#define iscntrl(c)	((__ismask(c)&(_C)) != 0)
#define isdigit(c)	((__ismask(c)&(_D)) != 0)
#define isgraph(c)	((__ismask(c)&(_P|_U|_L|_D)) != 0)
#define islower(c)	((__ismask(c)&(_L)) != 0)
#define isprint(c)	((__ismask(c)&(_P|_U|_L|_D|_SP)) != 0)
#define ispunct(c)	((__ismask(c)&(_P)) != 0)
#define isspace(c)	((__ismask(c)&(_S)) != 0)
#define isupper(c)	((__ismask(c)&(_U)) != 0)
#define isxdigit(c)	((__ismask(c)&(_D|_X)) != 0)

#define isascii(c) (((unsigned char)(c))<=0x7f)
#define toascii(c) (((unsigned char)(c))&0x7f)

static inline unsigned char __tolower(unsigned char c)
{
	if (isupper(c))
		c -= 'A'-'a';
	return c;
}

static inline unsigned char __toupper(unsigned char c)
{
	if (islower(c))
		c -= 'a'-'A';
	return c;
}

#define tolower(c) __tolower(c)
#define toupper(c) __toupper(c)


#ifdef NAUT_CONFIG_USE_NAUT_BUILTINS
void * memcpy (void * dst, const void * src, size_t n);
int memcmp (const void * s1_, const void * s2_, size_t n);
void * memset (void * dst, char c, size_t n);
void * memmove (void * dst, const void * src, size_t n);

size_t strlen (const char * str);
size_t strnlen (const char * str, size_t max);
int strcmp (const char * s1, const char * s2);
int strcasecmp (const char * s1, const char * s2);
int strncmp (const char * s1, const char * s2, size_t limit);
int strncasecmp (const char * s1, const char * s2, size_t limit);
char * strdup (const char * s);
char * strpbrk (const char * cs, const char * ct);
char * strptok (const char * cs, const char * ct);
char * strsep (char ** s, const char * ct);
char * strsep (char ** s, const char * ct);
char * strcat (char * s1, const char * s2);
char * strncat (char * s1, const char * s2, size_t limit);
char * strcpy (char * dest, const char * src);
char * strncpy (char * dest, const char * src, size_t limit);
char * strchr (const char * s, int c);
char * strrchr (const char * s, int c);
size_t strspn (const char * s, const char * accept);
size_t strcspn (const char * s, const char * reject);
char * strstr (const char * haystack, const char * needle);


#define OP_T_THRES 8
#define OPSIZ sizeof(unsigned long int)

#define WORD_COPY_BWD(dst_ep, src_ep, nbytes_left, nbytes)            \
  do                                          \
    {                                         \
      int __d0;                                   \
      asm volatile(/* Set the direction flag, so copying goes backwards.  */  \
           "std\n"                            \
           /* Copy longwords.  */                     \
           "rep\n"                            \
           "movsl\n"                              \
           /* Clear the dir flag.  Convention says it should be 0. */ \
           "cld" :                            \
           "=D" (dst_ep), "=S" (src_ep), "=c" (__d0) :            \
           "0" (dst_ep - 4), "1" (src_ep - 4), "2" ((nbytes) / 4) :   \
           "memory");                             \
      dst_ep += 4;                                \
      src_ep += 4;                                \
      (nbytes_left) = (nbytes) % 4;                       \
    } while (0)

#define BYTE_COPY_BWD(dst_ep, src_ep, nbytes)                     \
  do                                          \
    {                                         \
      int __d0;                                   \
      asm volatile(/* Set the direction flag, so copying goes backwards.  */  \
           "std\n"                            \
           /* Copy bytes.  */                         \
           "rep\n"                            \
           "movsb\n"                              \
           /* Clear the dir flag.  Convention says it should be 0. */ \
           "cld" :                            \
           "=D" (dst_ep), "=S" (src_ep), "=c" (__d0) :            \
           "0" (dst_ep - 1), "1" (src_ep - 1), "2" (nbytes) :         \
           "memory");                             \
      dst_ep += 1;                                \
      src_ep += 1;                                \
    } while (0)

#else

#include <stddef.h>

#define memcpy  __builtin_memcpy
#define memset  __builtin_memset
#define memcmp  __builtin_memcmp
#define strlen  __builtin_strlen
#define strnlen __builtin_strnlen
#define strcmp  __builtin_strcmp
#define strncmp __builtin_strncmp
#define strcat  __builtin_strcat
#define strncat __builtin_strncat
#define strstr  __builtin_strstr
#define strspn  __builtin_strspn
#define strcspn __builtin_strcspn
#define strchr  __builtin_strchr
#define strrchr __builtin_strrchr
#define strpbrk __builtin_strpbrk

#endif

int atoi (const char * buf);
int strtoi (const char * nptr, char ** endptr);
long strtol(const char * str, char ** endptr, int base);
long atol(const char *nptr);
uint64_t atox (const char * buf);
uint64_t strtox (const char * nptr, char ** endptr);
void str_toupper (char * s);
void str_tolower (char * s);


// Conversion to strings
// Caller must make buffers large enough...
int ultoa(unsigned long x, char *buf, int numdigits);
int ltoa(long x, char *buf, int numdigits);
int utoa(unsigned x, char *buf, int numdigits);
int itoa(int x, char *buf, int numdigits);
int ustoa(unsigned short x, char *buf, int numdigits);
int stoa(short x, char *buf, int numdigits);
int uctoa(unsigned char x, char *buf, int numdigits);
int ctoa(char x, char *buf, int numdigits);

// dtoa_r and strtod are declared here, but implemented
// in dtoa.c, not naut_string.c
char *dtoa_r(double x, int mode, int ndigits, int *decpt, int *sign, char **rve, char *buf, size_t blen);
#define ftoa_r(x,m,nd,dp,sn,rve,b,bl) dtoa(x,m,nd,dp,sn,rve,b,bl)
int  dtoa_printf_helper(double x, char pf_mode, int ndigits, int prec, char *buf, size_t blen);
double strtod(const char *s, char **se);

char * strtok(char *s, const char *delim);


// low level assembly versions for use early in the boot process
// or elsewhere where we want to guarantee that only generic 64 bit
// integer code is used regardless of the compiler's feelings
// word versions assume count in *words*, etc
void *nk_low_level_memset(void *dest, char data, size_t count);
void *nk_low_level_memset_word(void *dest, short data, size_t count);
void *nk_low_level_memcpy(void *dest, char *src, size_t count);
void *nk_low_level_memcpy_word(void *dest, short *src, size_t count);

			  
#ifdef __cplusplus
}
#endif

#endif /* !__LEGION__ */

#endif 
