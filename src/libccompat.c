/* 
 *
 * This file is intended to support libc functions
 * 
 *
 */
#include <nautilus.h>
#include <libccompat.h>
#include <thread.h>
#include <errno.h>

static uint64_t compat_nsec;
static uint64_t compat_sec;

#define GEN_DEF(x) \
    int x (void) { \
        UNDEF_FUN_ERR(); \
        return 0; \
    } 


void 
abort(void) 
{
    printk("Thread called abort\n");
    nk_thread_exit(NULL);
}

void 
exit(int status)
{
    printk("Thread called exit (status=%d)\n", status);
    nk_thread_exit((void*)(long)status);
}

int 
clock_gettime (clockid_t clk_id, struct timespec * tp)
{
    /* ignore clk_id */

    if (!tp) {
        return -EINVAL;
    }

    tp->tv_nsec = compat_nsec++;
    tp->tv_sec  = compat_sec++;

    return 0;
}


void 
__assert_fail (const char * assertion, const char * file, unsigned line, const char * function)
{
    panic("%s:%u: %s: Assertion %s failed\n",
            file,
            line,
            function,
            assertion);
}


int 
vfprintf (FILE * stream, const char * format, va_list arg)
{
    UNDEF_FUN_ERR();
    return -1;
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


FILE * 
fopen64 (const char * path, FILE * f)
{
    UNDEF_FUN_ERR();
    return NULL;
}


FILE * 
fdopen (int fd, const char * mode)
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

size_t 
fread (void * ptr, size_t size, size_t count, FILE * stream)
{
    UNDEF_FUN_ERR();
    return -1;
}

int
getwc (FILE * stream)
{
    UNDEF_FUN_ERR();
    return -1;
}


size_t 
__ctype_get_mb_cur_max (void)
{
    UNDEF_FUN_ERR();
    return 0;
}
 

int 
fseeko64 (FILE *fp, uint64_t offset, int whence)
{
    UNDEF_FUN_ERR();
    return -1;
}


int 
ungetc (int character, FILE * stream)
{
    UNDEF_FUN_ERR();
    return -1;
}
 

uint64_t 
lseek64 (int fd, uint64_t offset, int whence)
{
    UNDEF_FUN_ERR();
    return 0;
}

uint64_t 
ftello64 (FILE *stream)
{
    UNDEF_FUN_ERR();
    return 0;
}

int 
poll (struct pollfd *fds, nfds_t nfds, int timeout)
{
    UNDEF_FUN_ERR();
    return -1;
}

int 
ioctl (int d, unsigned long request, ...)
{
    UNDEF_FUN_ERR();
    return -1;
}

int 
syscall (int number, ...)
{
    UNDEF_FUN_ERR();
    return -1;
}



char * 
setlocale (int category, const char *locale)
{
    UNDEF_FUN_ERR();
    return NULL;
}

locale_t 
__duplocale (locale_t locobj)
{
    UNDEF_FUN_ERR();
    return NULL;
}

char * 
bindtextdomain (const char * domainname, const char * dirname)
{
    UNDEF_FUN_ERR();
    return NULL;
}

char * 
textdomain (const char * domainname)
{
    UNDEF_FUN_ERR();
    return NULL;
}

locale_t 
__newlocale (int category_mask, const char *locale, locale_t base)
{
    printk("category_mask: %d ,locale: %s, base: %p\n", category_mask, locale, base);

    return (locale_t)((ulong_t)base | (ulong_t)category_mask);
}

char *
__nl_langinfo_l (nl_item item, locale_t locale)
{
    UNDEF_FUN_ERR();
    return NULL;
}

char *
gettext (const char * msgid)
{
    char * ret = (char*)msgid;
    UNDEF_FUN_ERR();
    return ret;
}


/* became lazy... */
GEN_DEF(writev)
GEN_DEF(ungetwc)
GEN_DEF(__errno_location)
GEN_DEF(write)
GEN_DEF(wcrtomb)
GEN_DEF(mbrtowc)
GEN_DEF(getc)
GEN_DEF(__iswctype_l)
GEN_DEF(wcslen)
GEN_DEF(__strtof_l)
GEN_DEF(stderr)
GEN_DEF(wmemset)
GEN_DEF(stdin)
GEN_DEF(fileno)
GEN_DEF(__fxstat64)
GEN_DEF(putc)
GEN_DEF(__wcscoll_l)
GEN_DEF(__towlower_l)
GEN_DEF(wctob)
GEN_DEF(mbsrtowcs)
GEN_DEF(read)
GEN_DEF(wmemmove)
GEN_DEF(strdup)
GEN_DEF(__strxfrm_l)
GEN_DEF(wmemchr)
GEN_DEF(__freelocale)
GEN_DEF(__wcsftime_l)
GEN_DEF(wmemcpy)
GEN_DEF(putwc)
GEN_DEF(__stack_chk_fail)
GEN_DEF(__wcsxfrm_l)
GEN_DEF(wcscmp)
GEN_DEF(wcsnrtombs)
GEN_DEF(__strcoll_l)
GEN_DEF(stdout)
GEN_DEF(btowc)
GEN_DEF(memchr)
GEN_DEF(strtold_l)
GEN_DEF(wmemcmp)
GEN_DEF(__strtod_l)
GEN_DEF(setvbuf)
GEN_DEF(__popcountdi2)
GEN_DEF(__wctype_l)
GEN_DEF(__towupper_l)
GEN_DEF(printf)
GEN_DEF(__uselocale)
GEN_DEF(__strftime_l)
GEN_DEF(mbsnrtowcs)
GEN_DEF(pthread_mutex_init)
GEN_DEF(pthread_mutex_lock)
GEN_DEF(pthread_mutex_unlock)
