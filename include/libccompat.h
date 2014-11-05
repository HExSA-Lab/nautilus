#ifndef __LIBCCOMPAT_H__
#define __LIBCCOMPAT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus.h>

#define UNDEF_FUN_ERR() ERROR_PRINT("Function (%s) undefined\n", __func__)

typedef int clockid_t;
typedef int time_t;
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

void abort(void);
int clock_gettime(clockid_t, struct timespec*);
void __assert_fail(const char*, const char*, unsigned, const char*);
int vfprintf(FILE*, const char*, va_list);
void vsnprintf(char*, size_t, const char*, va_list);
long int lrand48(void);
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


#define GEN_HDR(x) int x (void);

GEN_HDR(sprintf)
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
GEN_HDR(strdup)
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
GEN_HDR(gettext)
GEN_HDR(__strcoll_l)
GEN_HDR(stdout)
GEN_HDR(btowc)
GEN_HDR(memchr)
GEN_HDR(strtold_l)
GEN_HDR(wmemcmp)
GEN_HDR(__strtod_l)
GEN_HDR(setvbuf)
GEN_HDR(__popcountdi2)
GEN_HDR(__wctype_l)
GEN_HDR(__towupper_l)
GEN_HDR(printf)
GEN_HDR(__uselocale)
GEN_HDR(__strftime_l)
GEN_HDR(mbsnrtowcs)
#ifdef __cplusplus
}
#endif

#endif
