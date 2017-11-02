/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University
*/

/* This module contains the facility CVL fucntions */

/* include declarations of library routines: 
 * malloc, free, getrusage, bcopy, memmove
 */
#if __STDC__
#if CMUCS
#include <libc.h>
#else /* CMUCS */
#include <stdlib.h>
#endif	/* CMUCS */
#endif  /* STDC */

#ifndef cray
#include <sys/time.h>
#include <sys/resource.h>
#else
#endif

#include <string.h>		/* for memmove declaration */
#include <cvl.h>
#include "defins.h"

/* -----------------------Timing functions-----------------------------*/
/* returns number of seconds and microseconds in argument
 * (number of clock ticks on cray)
 */
void tgt_fos(cvl_timer_p)	
cvl_timer_t *cvl_timer_p;
{
#ifdef cray
	*cvl_timer_p = rtclock();
#else
#if __hpux | __svr4__ | _AIX
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);
	cvl_timer_p->sec = tv.tv_sec;
	cvl_timer_p->usec = tv.tv_usec;
#else
 	struct rusage rusage;
 	getrusage(0, &rusage);          /* user time of process */
	cvl_timer_p->sec = rusage.ru_utime.tv_sec;
	cvl_timer_p->usec = rusage.ru_utime.tv_usec;
#endif
#endif
}

/* double precision difference in seconds between two cvl_timer_t values:
 *   t1 occurs after t2
 */
double tdf_fos(t1, t2)
cvl_timer_t *t1, *t2;
{
#ifdef cray
  /* .006 is the clock period (in usecs) of the Y-MP */
  return (double) (1.0e-6)*(.006)*(double)(*t1 - *t2);
#else
  return (double)(t1->sec - t2->sec) +
	 (1.0e-6)*(double)(t1->usec - t2->usec);
#endif /* cray */
}

/* --------------------Size Functions---------------------------------*/

/* siz_ (length): returns number of units of memory of vector.  Units are
   expressed in terms of the quantity CVL_SIZE_UNIT */

typedef union maxaligned { int i; double d; float f;} MAXALIGN;
#define CVL_SIZE_UNIT sizeof(MAXALIGN)

#define make_siz_func(_name, _type) 				\
   int _name (length)						\
   int length;							\
   {								\
	return (int) ( (length * sizeof(_type) + (CVL_SIZE_UNIT - 1)) / CVL_SIZE_UNIT);	\
   }		

make_siz_func(siz_fob, cvl_bool)		/* size of cvl_boolean vector */
make_siz_func(siz_foz, int)			/* size of integer vector */
make_siz_func(siz_fod, double)			/* size of double vector */

/* --------------------Segment Descriptors---------------------------*/

/* n = number of elements in segmented vector */
/* m = number of segments in vector */
/* The segment descriptor is an integer vector of m segment lengths */

int siz_fos(n, m)   /* size of segment descriptor */
int n, m;
{
  return (int) ((m * sizeof(int) + CVL_SIZE_UNIT-1) / CVL_SIZE_UNIT);
}

void mke_fov(sd, l, n, m, scratch)	/* create segment descriptor sd */
vec_p sd, l, scratch;			/* l is a vector of segment lengths */
int n, m;
{
  if (sd != l) cpy_wuz(sd, l, m, scratch);
}

make_inplace(mke_fov,INPLACE_1)

int mke_fov_scratch(n , m)		/* Must take two args */
int n, m;
	{return 0;}

void len_fos(l, sd, n, m, scratch)	/* inverse of mke_fov */
vec_p l, sd, scratch;
int n, m;
{
  if (sd != l) cpy_wuz(l, sd, m, scratch);
}

int len_fos_scratch(n, m)
int n,m;
	{
	return 0;
	}

make_inplace(len_fos,INPLACE_1)


/* -------------------Memory functions------------------------------*/

vec_p alo_foz(size)			/* allocate vector memory */
int size;				/* returns NULL if unsuccessful */
{
  return (vec_p) (MAXALIGN*) malloc((unsigned)size * CVL_SIZE_UNIT);
}

void fre_fov(pointer)			/* free vector memory */
vec_p pointer;
{
  free((char *)pointer);
}

/* mov_fov is a memory move instruction.  it must handle overlapping regions */
void mov_fov(d, s, size, scratch)
vec_p d, s, scratch;
int size;
{
    MAXALIGN *dest = (MAXALIGN*)d;
    MAXALIGN *src = (MAXALIGN*)s;
    if (dest == src) return;

    if ((MAXALIGN*) dest < (MAXALIGN*) src) {
	int i = 0;
	while (i < size) {
	    dest[i] = src[i];
	    i++;
	}
    } else {
	int i = size - 1;
	while (i >= 0) {
	    dest[i] = src[i];
	    i--;
	}
    }
}

int mov_fov_scratch(size)
int size;
{ return 0; }

/* ----------------Memory Arithmetic Functions---------------------*/

/* Need to make sure these pointers are maximally aligned.
 * Therefore vec_p should be a pointer to something large enough to hold
 * any CVL data type. 
 */

/* add_fov returns a vec_p V, such that a vector at v, of size a, does
 * not overlap into V.
 */
vec_p add_fov(v, a)
vec_p v;
int a;
{
	return (vec_p) ((MAXALIGN*)v + a);
}

/* subtract two pointers and return size of interval in CVL_SIZE_UNITs */
int sub_fov(v1, v2)
vec_p v1, v2;
{
	if ((MAXALIGN *)v1 > (MAXALIGN *)v2)
		return ((MAXALIGN *) v1 - (MAXALIGN *)v2);
	else
		return ((MAXALIGN *) v2 - (MAXALIGN *)v1);
}

/* compare two vec_p's.  We need this because vectors maybe more complicated
 * in some implementations.
 */
cvl_bool eql_fov(v1, v2)
vec_p v1, v2;
{
	return ((MAXALIGN*) v1 == (MAXALIGN*)v2);
}

/* compare two vec_p's for inequality.  Return is same as strcmp. */
int cmp_fov(v1, v2)
vec_p v1, v2;
{
    return ((MAXALIGN *)v1 - (MAXALIGN *)v2);
}

/* ----------------------CVL - C conversion functions -------------*/

#ifdef __STDC__
#include <memory.h>
#define BCOPY(from,to,size) memcpy(to,from,size)
#else
#define BCOPY(from,to,size) bcopy(from,to,size)
#endif /*  __STDC__ */

#define make_v2c(_name, _type)					\
    void _name(d, s, len, scratch)				\
    vec_p s, scratch;						\
    _type * d;							\
    int len;							\
	{							\
	BCOPY((char*)s, (char*)d, sizeof(_type) * len);		\
	}							\
    make_no_scratch(_name)					\
    make_inplace(_name,INPLACE_NONE)

make_v2c(v2c_fuz, int)
make_v2c(v2c_fub, cvl_bool)
make_v2c(v2c_fud, double)


#define make_c2v(_name, _type)					\
    void _name(d, s, len, scratch)				\
    vec_p d, scratch;						\
    _type * s;							\
    int len;							\
	{							\
	BCOPY((char*)s, (char*)d, sizeof(_type) * len);		\
	}							\
    make_no_scratch(_name)					\
    make_inplace(_name,INPLACE_NONE)

make_c2v(c2v_fuz, int)
make_c2v(c2v_fub, cvl_bool)
make_c2v(c2v_fud, double)

#if defined (CUCCARO_PRYOR)
int *genptr = NULL;
extern int *init_rng_d_int ();
extern void free_rng_int ();
#else /* CUCCARO_PRYOR */
#if __hpux || __svr4__
extern void srand48 ();
#else
#if linux
extern void srandom (unsigned);
#else
#if __STDC__
/* extern int srandom (int); */
#else 
/* extern int srandom (); */
#endif /* STDC */
#endif /* linux */
#endif /* HPUX */
#endif /* CUCCARO_PRYOR */

void rnd_foz (seed)
int seed;
{
#if defined (CUCCARO_PRYOR)
  /* init_rng_d_int has parameters (gennum, length, start, totalgen), */
  /* but we can't change length or start on subsequent calls. */
  if (genptr == NULL) {
    /* The first call -- use default values. */
    genptr = init_rng_d_int (0, 0, 0, 1);
  } else {
    /* Throw away old generator, pretend we're actually initializing */
    /* seed+1 generators, and use the seed'th. */
    free_rng_int (genptr);
    genptr = init_rng_d_int (seed, 0, 0, seed+1);
  }
#elif __hpux || __svr4__
    srand48 ((long int) seed);
#else /* HPUX */
    srandom (seed);
#endif
}


void CVL_init ()
{
  rnd_foz (0);
}
