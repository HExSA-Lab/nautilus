/* 
 *
 * This file is intended to support libc functions
 * 
 *
 */
#include <nautilus.h>
#include <libccompat.h>
#include <thread.h>


void 
abort(void) 
{
    exit(NULL);
}


int 
clock_gettime (clockid_t clk_id, struct timespec * tp)
{
    UNDEF_FUN_ERR();
    return -1;
}


void 
__assert_fail (const char * assertion, const char * file, unsigned line, const char * function)
{
    UNDEF_FUN_ERR();
}


int 
vfprintf (FILE * stream, const char * format, va_list arg)
{
    UNDEF_FUN_ERR();
    return -1;
}


void
vsnprintf (char * s, size_t n, const char * format, va_list arg)
{
    UNDEF_FUN_ERR();
}


long int
lrand48 (void)
{
    UNDEF_FUN_ERR();
    return -1;
}


char *
strerror (int errnum)
{
    UNDEF_FUN_ERR();
    return NULL;
}

int 
fclose (FILE * f)
{
    UNDEF_FUN_ERR();
    return -1;
}


FILE * 
fopen (const char * path, FILE * f)
{
    UNDEF_FUN_ERR();
    return NULL;
}


int 
fflush (FILE * f)
{
    UNDEF_FUN_ERR();
    return -1;
}


int 
fprintf (FILE * f, const char * s, ...)
{
    UNDEF_FUN_ERR();
    return -1;
}


int 
fputc (int c, FILE * f) 
{
    UNDEF_FUN_ERR();
    return -1;
}


int 
fputs (const char * s, FILE * f)
{
    UNDEF_FUN_ERR();
    return -1;
}


size_t 
fwrite (const void * ptr, size_t size, size_t count, FILE * stream)
{
    UNDEF_FUN_ERR();
    return -1;
}
