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
#ifndef __LIBCCOMPAT_H__
#define __LIBCCOMPAT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/nautilus.h>

#ifndef NAUT_CONFIG_SILENCE_UNDEF_ERR
#define UNDEF_FUN_ERR() ERROR_PRINT("Function (%s) undefined\n", __func__)
#else
#define UNDEF_FUN_ERR() 
#endif

#define RAND_MAX    2147483647

typedef int clockid_t;
#define CLOCK_REALTIME                  0
#define CLOCK_MONOTONIC                 1
#define CLOCK_PROCESS_CPUTIME_ID        2
#define CLOCK_THREAD_CPUTIME_ID         3
#define CLOCK_MONOTONIC_RAW             4
#define CLOCK_REALTIME_COARSE           5
#define CLOCK_MONOTONIC_COARSE          6
#define CLOCK_BOOTTIME                  7
#define CLOCK_REALTIME_ALARM            8
#define CLOCK_BOOTTIME_ALARM            9
#define CLOCK_SGI_CYCLE                 10      /* Hardware specific */
#define CLOCK_TAI                       11
 
typedef long time_t;
typedef void FILE;
typedef uint64_t off_t;

typedef int nl_item;
typedef unsigned long int nfds_t;

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

struct pollfd {
    int fd;
    short events;
    short revents;
};

typedef void* locale_t;

time_t time(time_t * timer);
void abort(void);
int __popcountdi2(long long a);
void exit(int status);
int clock_gettime(clockid_t, struct timespec*);
void __assert_fail(const char*, const char*, unsigned, const char*);
int vfprintf(FILE*, const char*, va_list);
int rand(void);
void srand(unsigned int seed);
void srand48(long int seedval);
long int lrand48(void);
double drand48(void);
char * strerror(int);

int fclose(FILE*);
FILE * fopen(const char*, FILE*);
FILE * fopen64(const char*, FILE*);
FILE *fdopen(int fd, const char *mode);
int fflush(FILE*);
int fprintf(FILE*, const char*, ...);
int fputc(int, FILE*);
int fputs(const char*, FILE*);
size_t fwrite(const void*, size_t, size_t, FILE*);
size_t fread(void * ptr, size_t size, size_t count, FILE * stream);
int getwc(FILE * stream);
size_t __ctype_get_mb_cur_max(void);
int fseeko64(FILE *fp, uint64_t offset, int whence);
int ungetc(int character, FILE * stream);
uint64_t lseek64(int fd, uint64_t offset, int whence);
uint64_t ftello64(FILE *stream);
int poll (struct pollfd *fds, nfds_t nfds, int timeout);
int ioctl(int d, unsigned long request, ...);
int syscall(int number, ...);
char *setlocale(int category, const char *locale);
locale_t __duplocale(locale_t locobj);
char * bindtextdomain (const char * domainname, const char * dirname);
char * textdomain (const char * domainname);
locale_t __newlocale(int category_mask, const char *locale,locale_t base);
char *__nl_langinfo_l(nl_item item, locale_t locale);
char * gettext(const char * msgid);

int printf(const char *, ...);

#define GEN_HDR(x) int x (void);

GEN_HDR(writev)
GEN_HDR(ungetwc)
GEN_HDR(__errno_location)
GEN_HDR(write)
GEN_HDR(wcrtomb)
GEN_HDR(mbrtowc)
GEN_HDR(getc)
GEN_HDR(__iswctype_l)
GEN_HDR(wcslen)
GEN_HDR(__strtof_l)
GEN_HDR(stderr)
GEN_HDR(wmemset)
GEN_HDR(stdin)
GEN_HDR(fileno)
GEN_HDR(__fxstat64)
GEN_HDR(putc)
GEN_HDR(__wcscoll_l)
GEN_HDR(__towlower_l)
GEN_HDR(wctob)
GEN_HDR(mbsrtowcs)
GEN_HDR(read)
GEN_HDR(wmemmove)
GEN_HDR(__strxfrm_l)
GEN_HDR(wmemchr)
GEN_HDR(__freelocale)
GEN_HDR(__wcsftime_l)
GEN_HDR(wmemcpy)
GEN_HDR(putwc)
GEN_HDR(__stack_chk_fail)
GEN_HDR(__wcsxfrm_l)
GEN_HDR(wcscmp)
GEN_HDR(wcsnrtombs)
GEN_HDR(__strcoll_l)
GEN_HDR(stdout)
GEN_HDR(btowc)
GEN_HDR(memchr)
GEN_HDR(strtold_l)
GEN_HDR(wmemcmp)
GEN_HDR(__strtod_l)
GEN_HDR(setvbuf)
GEN_HDR(__wctype_l)
GEN_HDR(__towupper_l)
GEN_HDR(__uselocale)
GEN_HDR(__strftime_l)
GEN_HDR(mbsnrtowcs)
GEN_HDR(pthread_mutex_init)
GEN_HDR(pthread_mutex_unlock)
GEN_HDR(pthread_mutex_lock)
#ifdef __cplusplus
}
#endif

#endif
