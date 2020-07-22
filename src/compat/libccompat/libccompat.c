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

/* 
 *
 * This file is intended to support libc functions
 * 
 *
 */


#define LIBCCOMPAT 1

#include <nautilus/nautilus.h>
#include <nautilus/libccompat.h>
#include <nautilus/thread.h>
#include <nautilus/errno.h>
#include <nautilus/random.h>
#include <nautilus/env.h>
#include <dev/hpet.h>


#ifdef NAUT_CONFIG_DEBUG_BASE_LIBC_COMPAT
#define DEBUG(fmt, args...) DEBUG_PRINT("libc: " fmt, ##args)
#else
#define DEBUG(fmt, args...)
#endif
#define INFO(fmt, args...) INFO_PRINT("libc: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("libc: " fmt, ##args)
#define WARN(fmt, args...)  WARN_PRINT("libc: " fmt, ##args)


int errno=0;

#define GEN_UNDEF(t,f,v)			\
    t f (void) {				\
        UNDEF_FUN_ERR();			\
        return v;				\
    } 

#define BOGUS()    BOGUS_FUN_ERR()


// Structs needed for LUA 



static int maxExponent = 511;	/* Largest possible base 10 exponent.  Any
				 * exponent larger than this will already
				 * produce underflow or overflow, so there's
				 * no need to worry about additional digits.
				 */
static double powersOf10[] = {	/* Table giving binary powers of 10.  Entry */
    10.,			/* is 10^2^i.  Used to convert decimal */
    100.,			/* exponents into floating-point numbers. */
    1.0e4,
    1.0e8,
    1.0e16,
    1.0e32,
    1.0e64,
    1.0e128,
    1.0e256
};



struct lconv {
    char *decimal_point;      //"."          LC_NUMERIC
    char *grouping;           //""           LC_NUMERIC
    char *thousands_sep;      //""           LC_NUMERIC

    char *mon_decimal_point;  //""           LC_MONETARY
    char *mon_grouping;       //""           LC_MONETARY
    char *mon_thousands_sep;  //""           LC_MONETARY

    char *negative_sign;      //""           LC_MONETARY
    char *positive_sign;      //""           LC_MONETARY
    char *currency_symbol;    //""           LC_MONETARY
    char frac_digits;         //CHAR_MAX     LC_MONETARY
    char n_cs_precedes;       //CHAR_MAX     LC_MONETARY
    char n_sep_by_space;      //CHAR_MAX     LC_MONETARY
    char n_sign_posn;         //CHAR_MAX     LC_MONETARY
    char p_cs_precedes;       //CHAR_MAX     LC_MONETARY
    char p_sep_by_space;      //CHAR_MAX     LC_MONETARY
    char p_sign_posn;         //CHAR_MAX     LC_MONETARY

    char *int_curr_symbol;
    char int_frac_digits;
    char int_n_cs_precedes;
    char int_n_sep_by_space;
    char int_n_sign_posn;
    char int_p_cs_precedes;
    char int_p_sep_by_space;
    char int_p_sign_posn;
};





//=========================================================
static uint64_t dummy_mono_clock = 0;

time_t 
time (time_t * timer)
{
    BOGUS();
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);

    if (timer) {
        *timer = tp.tv_nsec;
    }
    return tp.tv_nsec;
}


void 
abort(void) 
{
    BOGUS();
    printk("Thread called abort\n");
    nk_thread_exit(NULL);
}


int 
__popcountdi2 (long long a)
{
    unsigned long long x2 = (unsigned long long)a;
    x2 = x2 - ((x2 >> 1) & 0x5555555555555555uLL);
    /* Every 2 bits holds the sum of every pair of bits (32) */
    x2 = ((x2 >> 2) & 0x3333333333333333uLL) + (x2 & 0x3333333333333333uLL);
    /* Every 4 bits holds the sum of every 4-set of bits (3 significant bits) (16) */
    x2 = (x2 + (x2 >> 4)) & 0x0F0F0F0F0F0F0F0FuLL;
    /* Every 8 bits holds the sum of every 8-set of bits (4 significant bits) (8) */
    unsigned x = (unsigned)(x2 + (x2 >> 32));
    /* The lower 32 bits hold four 16 bit sums (5 significant bits). */
    /*   Upper 32 bits are garbage */
    x = x + (x >> 16);
    /* The lower 16 bits hold two 32 bit sums (6 significant bits). */
    /*   Upper 16 bits are garbage */
    return (x + (x >> 8)) & 0x0000007F;  /* (7 significant bits) */
}

void 
exit(int status)
{
    BOGUS();
    printk("Thread called exit (status=%d)\n", status);
    nk_thread_exit((void*)(long)status);
}

int 
clock_gettime (clockid_t clk_id, struct timespec * tp)
{

    BOGUS();
    if (clk_id != CLOCK_MONOTONIC) {
        printk("NAUTILUS WARNING: using invalid clock type\n");
        return -EINVAL;
    }

    if (!tp) {
        printk("NAUTILUS WARNING: using invalid timespec\n");
        return -EINVAL;
    }

#ifdef NAUT_CONFIG_HPET
    //uint64_t freq = nk_hpet_get_freq();
    uint64_t cnt  = nk_hpet_get_cntr();
    //uint64_t nsec = (1000000000/freq) * cnt;
    uint64_t nsec = cnt * nk_hpet_nanos_per_tick();
    tp->tv_sec    = nsec / 1000000000;
    tp->tv_nsec   = nsec % 1000000000;
#else
    /* runs at "10kHz" */
    tp->tv_nsec = dummy_mono_clock*100000;
    tp->tv_sec  = dummy_mono_clock/10000;
    ++dummy_mono_clock;
#endif

    return 0;
}


void 
__assert_fail (const char * assertion, const char * file, unsigned line, const char * function)
{
    panic("Failed assertion in %s: %s at %s, line %d, RA=%lx\n",
            function,
            assertion,
            file,
            line,
            __builtin_return_address(0));
}


int 
vfprintf (FILE * stream, const char * format, va_list arg)
{
#if 0
    UNDEF_FUN_ERR();
    return -1;
#else
    if (stream!=stdout && stream!=stderr) { BOGUS(); }
    return vprintk(format,arg);
#endif
}


int 
rand (void) {
    int r;

    nk_get_rand_bytes((uint8_t*)&r, 4);

    r = r<0 ? -r : r;

    return r % RAND_MAX;
}

void 
srand (unsigned int seed)
{
    uint64_t s = ((uint64_t)seed) << 32;
    nk_rand_seed(s | 0x330e);
}
    

/* NOTE: these are likely not in any way compliant
 */
void
srand48 (long int seedval)
{
    uint64_t tmp = (((uint64_t) seedval) & 0xffffffffull) << 32;
    nk_rand_seed(tmp | 0x330eULL);
}


// extract this functionality so that we can implemnt erand, nrand, mrand, jrand, lcong later
static inline uint64_t _pump_rand(uint64_t xi, uint64_t a, uint64_t c)
{
    uint64_t m      = 0x1ULL << 48;
    uint64_t xi_new = (a*xi + c) % m;

    return xi_new;
}    

// advance the random number generator and return the new
// x_i value (48 bit value)
static inline uint64_t pump_rand()
{
    struct nk_rand_info * rand = per_cpu_get(rand);

    uint64_t xi_new = _pump_rand(rand->xi, 0x5deece66dULL, 0xbULL);
    
    nk_rand_set_xi(xi_new);

    return xi_new;
}

   
long int
lrand48 (void)
{
    uint64_t val = pump_rand();

    // return top 31 bits
    return (val >> 17) & 0x8fffffffULL;
}


union ieee754dbl {
    double d;
    uint64_t ui;
    struct {
        uint32_t mantissa1 : 32;
        uint32_t mantissa0 : 20;
        uint32_t exponent  : 11;
        uint32_t sign      :1;
    } __packed;
} __packed;

double
drand48(void) 
{
    uint64_t val = pump_rand();
    
    // now have 48 bits of randomness, but we need 52 bits for a double
    // we will copy the 48 bits into the high order bits of the mantissa

    uint16_t *xic   = (uint16_t*)&val;

    union ieee754dbl ret;

    ret.sign     = 0;
    ret.exponent = 0x3ff;
    // 16 bits from xic[2], 4 bits from xic[1]
    ret.mantissa0 = (((uint32_t)xic[2]) << 4) | (((uint32_t)xic[1]) >> 12);
    // 12 bits from xic[1], 16 bits from xic[0], remaining 4 bits are zeros
    ret.mantissa1 = ((((uint32_t)xic[1]) & 0xfff) << 20) | (xic[0] << 4);

    return ret.d - 1.0;
}


char* strerror_r(int errnum, char* buf, size_t buflen)
{
    BOGUS();
    snprintf(buf,buflen,"error %d (dunno?)",errnum);
    return buf;
}


char *strerror (int errnum)
{
    BOGUS();
    static char strerr_buf[128];
    return strerror_r(errnum,strerr_buf,128);
}


FILE *tmpfile(void)
{

    UNDEF_FUN_ERR();
    return NULL;

}
int 
ferror (FILE * f)
{
    UNDEF_FUN_ERR();
    return -1;
}
FILE *freopen(const char *fname, const char *mode,FILE *stream)
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


int setenv(const char *key, const char *value, int overwrite)
{
    char *curval;
    struct nk_env * env = nk_env_find(NK_ENV_GLOBAL_NAME);
    if (nk_env_search(env,(char*)key,&curval)==0) {
	if (overwrite) {
	    return nk_env_update(env,(char*)key,(char*)value);
	} else {
	    //no action 
	   return 0; 
	   // return -1;
	}
    } else {
	return nk_env_insert(env, (char*)key, (char*)value);
    }
}


char *getenv(const char *key)
{
    struct nk_env *env = nk_env_find(NK_ENV_GLOBAL_NAME);
    if (!env) {
	return 0;
    }
    char *value =NULL;
    nk_env_search(env,(char*)key,&value);
    return value;
}


//For LUA Support
clock_t clock()
{

    UNDEF_FUN_ERR();
    return -1;
}
//For LUA Support
char *tmpnam(char *s)
{

    UNDEF_FUN_ERR();
    return NULL;
}

//For LUA Support
int remove(const char *path)
{

    UNDEF_FUN_ERR();
    return -1;
}
//For LUA Support
int rename(const char *old, const char *new)
{

    UNDEF_FUN_ERR();
    return -1;
}
//For LUA Support
int system(const char *command)
{

    UNDEF_FUN_ERR();
    return -1;
}
//For LUA Support

int fflush (FILE * f)
{
    return 0;
}

void (*signal(int sig, void (*func)(int)))(int )
{
    //    printk("\nSIGNAL Function:");
    BOGUS();
    return 0;
}

//For LUA Support

int 
fprintf (FILE * f, const char * s, ...)
{
#if 0
    UNDEF_FUN_ERR();
    return -1;
#else
    if (f!=stdout && f!=stderr) { BOGUS(); }
    va_list arg;
    va_start(arg,s);
    vprintk(s, arg);
    va_end(arg);
    return 0;
#endif
}

//For LUA
int setvbuf(FILE *restrict stream, char *restrict buf, int type,
       size_t size)
{

    UNDEF_FUN_ERR();
    return -1;
}


//For LUA

int fscanf(FILE *restrict stream, const char *restrict format, ... )
{

    UNDEF_FUN_ERR();
    return -1;

}
//For LUA
void clearerr(FILE *stream)
{

    UNDEF_FUN_ERR();
   // return NULL;
}

int printf (const char * s, ...)
{
#if 0
    UNDEF_FUN_ERR();
    return -1;
#else
    va_list arg;
    va_start(arg,s);
    vprintk(s, arg);
    va_end(arg);
    return 0;
#endif
}

int 
fputc (int c, FILE * f) 
{
    if (f!=stderr && f!=stdout) { BOGUS(); }
    printk("%c");
    return c;
}


int 
fputs (const char * s, FILE * f)
{
    if (f!=stderr && f!=stdout) { BOGUS(); }
    printk("%s\n", s);
    return 0;
}
size_t fwrite (const void *ptr, size_t size, size_t nmemb, FILE *f)
{
    
    if (f!=stderr && f!=stdout) { BOGUS(); }
    printk("\n %s",ptr); 
    //UNDEF_FUN_ERR();
    return (int)size;
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
//For LUA Support
int fseek(FILE *stream, long offset, int whence)
{

    UNDEF_FUN_ERR();
    return 1;
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
//For LUA Support
long ftell(FILE *x)
{
    UNDEF_FUN_ERR();
    return -1;
}
int poll (struct pollfd *fds, nfds_t nfds, int timeout)
{
    UNDEF_FUN_ERR();
    return 1;
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

int getc(FILE* arg)
{

    UNDEF_FUN_ERR();
    return -1;

}

int fileno(FILE* f)
{
    return f==stdin ? 0 : f==stdout ? 1 : f==stderr ? 2 : -1;
}

int isatty(int fd)
{
    BOGUS();
    return 0;
}

//LUA SPECIFIC....................
size_t strftime(char *str, size_t maxsize, const char *format, const struct tm *timeptr)
{
    UNDEF_FUN_ERR();
    return 0;
}
int feof(FILE * x)
{
    UNDEF_FUN_ERR();
    return 0;
}

char *fgets(char *str, int n, FILE *stream)
{
    UNDEF_FUN_ERR();
    return NULL;
}


/*void longjmp(int *x, int __y)
{
    UNDEF_FUN_ERR();
}

int setjmp(int *x)
{
    return 0;
} */

int ischar(unsigned char *str)
{
    BOGUS();
    // WTF is this???
    return 1;
}

// strtod is implemented in dtoa.c

/*----------*/
int abs(int x)
{
    return x<0 ? -x : x;
}    


time_t mktime(struct tm *timeptr)
{
    return 0;
}
struct tm *localtime(const time_t *timer)
{
    return NULL;
}
struct tm *gmtime(const time_t *timer)
{
    return NULL;
}
int strcoll(const char *str1, const char *str2)
{
    return 0;
}

double difftime(time_t time1, time_t time2)
{
    UNDEF_FUN_ERR();
    return 0;
}

unsigned long getpid(void)
{
    return ((nk_thread_t*)get_cur_thread())->tid;
}

unsigned long getppid(void)
{
    if (((nk_thread_t*)get_cur_thread())->parent) {
	return ((nk_thread_t*)get_cur_thread())->parent->tid;
    } else {
	return 0;
    }
}



#define suseconds_t uint64_t
struct timeval {
    time_t      tv_sec;     /* seconds */
    suseconds_t tv_usec;    /* microseconds */
};

struct timezone;

int gettimeofday(struct timeval *tv, struct timezone *tz_ignored)
{
    uint64_t ns = nk_sched_get_realtime();

    tv->tv_sec =  ns / 1000000000ULL;
    tv->tv_usec = (ns % 1000000000ULL)/1000ULL;

    return 0;
}

#define rlim_t uint32_t

struct rlimit {
    rlim_t rlim_cur;  /* Soft limit */
    rlim_t rlim_max;  /* Hard limit (ceiling for rlim_cur) */
};

int getrlimit(int resource, struct rlimit *rlim)
{
    DEBUG("getrlimit %d\n", resource); 
    rlim->rlim_cur = 0xF0000000;
    rlim->rlim_max = 0xF0000000;
    return 0;
}



/* became lazy... */
GEN_UNDEF(int,writev,0)
GEN_UNDEF(int,ungetwc,0)
GEN_UNDEF(int,__errno_location,0)
GEN_UNDEF(int,write,0)
GEN_UNDEF(int,wcrtomb,0)
GEN_UNDEF(int,mbrtowc,0)
//GEN_UNDEF(int,getc,0)
GEN_UNDEF(int,__iswctype_l,0)
GEN_UNDEF(int,wcslen,0)
GEN_UNDEF(int,__strtof_l,0)
//GEN_UNDEF(int,stderr,0)
GEN_UNDEF(int,wmemset,0)
//GEN_UNDEF(int,stdin,0)
//GEN_UNDEF(int,fileno,0)
GEN_UNDEF(int,__fxstat64,0)
GEN_UNDEF(int,putc,0)
GEN_UNDEF(int,__wcscoll_l,0)
GEN_UNDEF(int,__towlower_l,0)
GEN_UNDEF(int,wctob,0)
GEN_UNDEF(int,mbsrtowcs,0)
GEN_UNDEF(int,read,0)
GEN_UNDEF(int,wmemmove,0)
GEN_UNDEF(int,__strxfrm_l,0)
GEN_UNDEF(int,wmemchr,0)
GEN_UNDEF(int,__freelocale,0)
GEN_UNDEF(int,__wcsftime_l,0)
GEN_UNDEF(int,wmemcpy,0)
GEN_UNDEF(int,putwc,0)
GEN_UNDEF(int,__stack_chk_fail,0)
GEN_UNDEF(int,__wcsxfrm_l,0)
GEN_UNDEF(int,wcscmp,0)
GEN_UNDEF(int,wcsnrtombs,0)
GEN_UNDEF(int,__strcoll_l,0)
//GEN_UNDEF(int,stdout,0)
GEN_UNDEF(int,btowc,0)
//GEN_UNDEF(int,memchr,0)
GEN_UNDEF(int,strtold_l,0)
GEN_UNDEF(int,wmemcmp,0)
GEN_UNDEF(int,__strtod_l,0)
//GEN_UNDEF(int,setvbuf,0)
GEN_UNDEF(int,__wctype_l,0)
GEN_UNDEF(int,__towupper_l,0)
GEN_UNDEF(int,__uselocale,0)
GEN_UNDEF(int,__strftime_l,0)
GEN_UNDEF(int,mbsnrtowcs,0)
GEN_UNDEF(int,wcscoll,0)
//GEN_UNDEF(int,strcoll,0)
GEN_UNDEF(int,towupper,0)
GEN_UNDEF(int,towlower,0)
GEN_UNDEF(int,iswctype,0)
//GEN_UNDEF(int,strftime,0)
GEN_UNDEF(int,wcsftime,0)
GEN_UNDEF(int,wctype,0)
GEN_UNDEF(int,strtold,0)
//GEN_UNDEF(int,strtod,0)
GEN_UNDEF(int,strtof,0)
GEN_UNDEF(int,__ctype_b_loc,0)
GEN_UNDEF(int,__ctype_toupper_loc,0)
GEN_UNDEF(int,__ctype_tolower_loc,0)
GEN_UNDEF(int,strxfrm,0)
GEN_UNDEF(int,wcsxfrm,0)
GEN_UNDEF(int,__kernel_standard,0);
GEN_UNDEF(int,__get_cpu_features,0);

// pthread functionality is in the pthread compatability
// directory, if it is included
#ifndef NAUT_CONFIG_BASE_PTHREAD_COMPAT
GEN_UNDEF(int,pthread_mutex_init,0)
GEN_UNDEF(int,pthread_mutex_lock,0)
GEN_UNDEF(int,pthread_mutex_unlock,0)
GEN_UNDEF(int,pthread_atfork,0)
GEN_UNDEF(int,pthread_attr_destroy,0)
GEN_UNDEF(int,pthread_attr_getstack,0)
GEN_UNDEF(int,pthread_attr_init,0)
GEN_UNDEF(int,pthread_attr_setdetachstate,0)
GEN_UNDEF(int,pthread_attr_setstacksize,0)
GEN_UNDEF(int,pthread_cancel,0)
GEN_UNDEF(int,pthread_cond_destroy,0)
GEN_UNDEF(int,pthread_cond_init,0)
GEN_UNDEF(int,pthread_cond_signal,0)
GEN_UNDEF(int,pthread_cond_wait,0)
GEN_UNDEF(int,pthread_condattr_init,0)
GEN_UNDEF(int,pthread_create,0)
GEN_UNDEF(int,pthread_exit,0)
GEN_UNDEF(int,pthread_getattr_np,0)
GEN_UNDEF(int,pthread_getspecific,0)
GEN_UNDEF(int,pthread_join,0)
GEN_UNDEF(int,pthread_key_create,0)
GEN_UNDEF(int,pthread_key_delete,0)
GEN_UNDEF(int,pthread_mutex_destroy,0)
GEN_UNDEF(int,pthread_mutex_trylock,0)
GEN_UNDEF(int,pthread_mutexattr_init,0)
GEN_UNDEF(int,pthread_self,0)
GEN_UNDEF(int,pthread_setcancelstate,0)
GEN_UNDEF(int,pthread_setcanceltype,0)
GEN_UNDEF(int,pthread_setspecific,0)
GEN_UNDEF(int,sched_yield,0)
#endif


GEN_UNDEF(int,atexit,0)
GEN_UNDEF(int,catclose,0)
GEN_UNDEF(int,catgets,0)
GEN_UNDEF(int,catopen,0)
GEN_UNDEF(int,close,0)
GEN_UNDEF(int,closedir,0)
GEN_UNDEF(int,dlsym,0)
GEN_UNDEF(int,environ,0)
GEN_UNDEF(int,fgetc,0)
GEN_UNDEF(int,gethostname,0)
GEN_UNDEF(int,getrusage,0)
GEN_UNDEF(int,open,0)
GEN_UNDEF(int,opendir,0)
GEN_UNDEF(int,readdir,0)
GEN_UNDEF(int,sigaction,0)
GEN_UNDEF(int,sigaddset,0)
GEN_UNDEF(int,sigdelset,0)
GEN_UNDEF(int,sigemptyset,0)
GEN_UNDEF(int,sigfillset,0)
GEN_UNDEF(int,sigismember,0)
GEN_UNDEF(int,sleep,0)
GEN_UNDEF(int,strtok_r,0)
GEN_UNDEF(int,times,0)
GEN_UNDEF(int,unsetenv,0)
GEN_UNDEF(int,vfscanf,0)

	  
