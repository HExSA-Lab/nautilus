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

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

void abort(void);
int clock_gettime(clockid_t, struct timespec*);
void __assert_fail(const char*, const char*, unsigned, const char*);
int vfprintf(FILE*, const char*, va_list);
void vsnprintf(char*, size_t, const char*, va_list);
long int lrand48(void);
char * strerror(int);

int fclose(FILE*);
FILE * fopen(const char*, FILE*);
int fflush(FILE*);
int fprintf(FILE*, const char*, ...);
int fputc(int, FILE*);
int fputs(const char*, FILE*);
size_t fwrite(const void*, size_t, size_t, FILE*);

#ifdef __cplusplus
}
#endif

#endif
