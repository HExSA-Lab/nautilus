/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University
*/

/* This file contains the rank functions:
 * o upward and downward,
 * o integer and double,
 * o segmented and unsegmented.
 * The result of these functions is a permutation (suitable for use with
 * the smp_p** functions) that gives the sorted order of the elements
 * of the source vector.  The rank functions are stable, and use a radix
 * rank to get linear performance.
 */

#include <cvl.h>
#include <limits.h>

/* ------------------------ Radix Rank ----------------------------- */

/* From Sedgewick, Algorithms in C */

/*
 * n = number of keys
 * result, source, and tmp need to be of length n
 * buckets needs to be of length 2^bits
 *
 * This function does a radix rank on bit fields the size of an 
 * unsigned int.  Compose the function to rank larger objects.
 * tmp should be initialized with the starting permutation of 
 * the index set (identity, or a previous result of the rank).
 * To get the final rank do:
 * 	for (i = 0; i < n; i ++)
 *   	    result[tmp[i]] = i;
 * Tmp and result hold indicies into source, and are of type integer.
 */

/* ANSI defines CHAR_BIT in limits.h as the number of bits in a char.
 * char is defined as the unit of size that sizeof deals with.  So:
 */
#define BitsPerWord	(sizeof(int) * CHAR_BIT)

#define BitsPerPass	8			/* use 8 bit word */
#define NumBuckets	(1<<BitsPerPass)	/* 512 bucktes */

/* Use these to extract the correct bits in each pass */
#define BitsForPassMask	~(~0 << BitsPerPass)
#define bits(_x,_k) 	((((unsigned)_x) >> _k) & BitsForPassMask)

static void field_rank(result, source, tmp, n)
int *result, *tmp; 
unsigned int *source;
int n;
{
  int i, j;
  int startbit = 0;
  int buckets[NumBuckets];
  while (startbit < BitsPerWord)
    {
      for (j = 0; j < NumBuckets; j++) 		/* clear buckets */
	buckets[j] = 0;
      for (i = 0; i < n; i++)			/* histogram */
	buckets[bits(source[tmp[i]], startbit)]++;
      for (j = 1; j < NumBuckets; j++)		/* scan */
	buckets[j] = buckets[j-1] + buckets[j];
      for (i = n-1; i >= 0; i--)		/* move the data */
	result[--buckets[bits(source[tmp[i]], startbit)]] = tmp[i];
      for (i = 0; i < n; i++) 			/* copy into tmp */
	tmp[i] = result[i];
      startbit += BitsPerPass;
    }
}

/* ---------------------- Integer Rank ----------------------------*/
/* This function does all the integer ranks: up, down, segmented,
 * unsegmented.  Algorithm is: 
 * 1. xor the elements of the data to handle signed numbers correctly:
 *       if upward rank, then flip the sign bit
 *       if down rank, then flip all the bits except the sign bit
 * 2. Use field_rank to do the rank
 * 3. Restore the flipped bits from 1
 * 4. If the sort is segemented, then use the segment number to do
 *    an additional rank.
 * 5. Get the final rank out of tmp
 */

#define SIGNBIT (1 << ((BitsPerWord)-1))

static void int_rank_sort(d, s, segd, vec_len, seg_count, scratch, isSeg, isUp)
vec_p d, s, segd, scratch;
int vec_len, seg_count;
int isSeg, isUp;
{
    int i;
    unsigned xorMask = isUp ? SIGNBIT : ~SIGNBIT;
    unsigned int *src = (unsigned int *)s;
    unsigned int *rank = (unsigned int *)d;

    /* need 2 * vec_len scratch space for segmented rank */
    int *tmp = (int *)scratch;			/* tmp needs vec_len ints */
    unsigned int *seg_aux = (unsigned int *)scratch + vec_len;

    unsigned int *auxp;
    int *segdp;
    int offset;
    
    /* xor sign bits, initialize tmp to identity permutation */
    for (i = 0; i < vec_len; i ++) {
	src[i] ^= xorMask;
	tmp[i] = i;
    }
	
    field_rank(rank, src, tmp, vec_len);	/* do the sort */

    for (i = 0; i < vec_len; i ++) {
	src[i] ^= xorMask;			/* fix up bits */
    }

    /* if doing segmented rank, sort segments */
    if (isSeg && seg_count > 1) {
	/* label each element in seg_aux with the segment number of the data */
	for (auxp = seg_aux, segdp = (int *) segd;
	     segdp < (int *)segd + seg_count;		/* loop over segments */
	     segdp ++) {
	    for (i = 0; i < *segdp; i++) { 		/* loop within seg */
		*auxp = segdp - (int *)segd;
		auxp++;
	    }
	}

	/* rank with seg_aux */
	field_rank(rank, seg_aux, tmp, vec_len);
    }

    for (i = 0; i < vec_len; i ++)		/* get the rank */
	rank[tmp[i]] = i;
	
    if (isSeg && seg_count > 1) {
	/* rescale the rank so that the indices are per segment */
	offset = 0;
	for (auxp = rank, segdp = (int *) segd;
	     segdp < (int *)segd + seg_count;	/* loop over segments */
	     segdp++) {
	    for (i = 0; i < *segdp; i++) {		/* loop with segment */
		 *auxp -= offset;
		 auxp++;
	    }
	    offset += *segdp;
	}
    }
}

void rku_luz(d, s, len, scratch)
vec_p d, s, scratch;
int len;
{
    int_rank_sort(d, s, (vec_p)0, len, 0, scratch, 0, 1);
}

int rku_luz_scratch(len) int len; { return siz_foz(len); }
unsigned int rku_luz_inplace() { return INPLACE_NONE;}

void rkd_luz(d, s, len, scratch)
vec_p d, s, scratch;
int len;
{
    int_rank_sort(d, s, (vec_p)0, len, 0, scratch, 0, 0);
}

int rkd_luz_scratch(len) int len; {return siz_foz(len);}
unsigned int rkd_luz_inplace() { return INPLACE_NONE;}

void rku_lez(d, s, segd, vec_len, seg_count, scratch)
vec_p d, s, segd, scratch;
int vec_len, seg_count;
{
    int_rank_sort(d, s, segd, vec_len, seg_count, scratch, 1, 1);
}

int rku_lez_scratch(vec_len, seg_count) 
int vec_len, seg_count; 
{ return 2*siz_foz(vec_len); }

unsigned int rku_lez_inplace() { return INPLACE_NONE;}

void rkd_lez(d, s, segd, vec_len, seg_count, scratch)
vec_p d, s, segd, scratch;
int vec_len, seg_count;
{
    int_rank_sort(d, s, segd, vec_len, seg_count, scratch, 1, 0);
}

int rkd_lez_scratch(vec_len, seg_count) 
int vec_len, seg_count; 
{ return 2*siz_foz(vec_len); }

unsigned int rkd_lez_inplace() { return INPLACE_NONE;}

/* ----------------------- double rank -----------------------*/

/* This function does all the double ranks: up, down, segmented,
 * unsegmented.  Algorithm is: 
 * 1. xor the elements of the data to handle signed numbers correctly:
 *   if upward rank, then flip the sign bit
 *   if down rank, then flip all the bits except the sign bit
 * 2. Use field_rank to do the rank
 * 3. Restore the flipped bits from 1
 * 4. If the sort is segemented, then use the segment number to do
 *    an additional rank.
 * 5. Get the final rank out of tmp
 */

/* This defines the number of (unsigned int) fields in double */
#define IntsInDouble (sizeof(double) / sizeof(unsigned))

/* Need to know which words of a double are most significant */
/* Everything seems to be little endian these days except for
 * some mips machines.
 * 
 * We assume that doubles are stored with a format like:
 *      s, e, m
 * where s is a sign bit with 1 = negative
 *       e is the exponent stored in an excess form
 *       m is the mantissa
 */
#undef FP_LITTLE_ENDIAN
/* These architectures can be little endian */
#if mips | alpha | __i860 | i386 
/* ...but SGI boxes aren't. */
#ifndef sgi
#define FP_LITTLE_ENDIAN 1
#endif                  /* not sgi */
#endif

static void double_rank_sort(d, s, segd, vec_len, seg_count, scratch, isSeg, isUp)
vec_p d, s, segd, scratch;
int vec_len, seg_count;
int isSeg, isUp;
{
    int i,j;
    double *src = (double *)s;
    unsigned int *field = (unsigned int *)scratch;
    unsigned int *rank = (unsigned int *)d;

    /* need 2 * vec_len scratch space for segmented rank */
    int *tmp = (int *)scratch + vec_len;	/* tmp needs vec_len ints */
    unsigned int *seg_aux;

    unsigned int *auxp;
    int *segdp;
    int offset;

    /* initialize tmp to identity perm */
    for (i = 0; i < vec_len; i ++) {
	tmp[i] = i;			
    }

    /* work on the low order bits of the double */
    for (j = 0; j < IntsInDouble; j++) {
	for (i = 0; i < vec_len; i++) {
	    unsigned int *u = (unsigned int *)&src[i];
	    unsigned int t, sign, fld;

	    /* extract the correct bits from the double */
#ifdef FP_LITTLE_ENDIAN
	    t = u[j];
            sign = u[IntsInDouble-1] & SIGNBIT;
#else /* FP_BIG_ENDIAN */
	    t = u[IntsInDouble - j - 1];
            sign = u[0] & SIGNBIT;
#endif /* ENDIAN */
            /* this needs optimized: some tests below are loop invariant */
	    if (j != IntsInDouble - 1)	{ 
	        fld = sign ? ~t : t;
	    } else {
		fld = sign ? ~t : t^SIGNBIT;
	    }
            field[i] = isUp ? fld : ~fld;
	}
        field_rank(rank, field, tmp, vec_len);		/* rank this field */
    }

    seg_aux = (unsigned int *) scratch;		/* field not used any more */
    /* if doing segmented rank, sort segments */
    if (isSeg && seg_count > 1) {
	/* label each element in seg_aux with the segment number of the data */
	for (auxp = seg_aux, segdp = (int *) segd;
	     segdp < (int *)segd + seg_count;		/* loop over segments */
	     segdp ++) {
	    for (i = 0; i < *segdp; i++) { 		/* loop within seg */
		*auxp = segdp - (int *)segd;
		auxp++;
	    }
	}

	/* rank with seg_aux */
	field_rank(rank, seg_aux, tmp, vec_len);
    }

    for (i = 0; i < vec_len; i ++)		/* get the rank */
	rank[tmp[i]] = i;
	
    if (isSeg && seg_count > 1) {
	/* rescale the rank so that the indices are per segment */
	offset = 0;
	for (auxp = rank, segdp = (int *) segd;
	     segdp < (int *)segd + seg_count;	/* loop over segments */
	     segdp++) {
	    for (i = 0; i < *segdp; i++) {		/* loop with segment */
		 *auxp -= offset;
		 auxp++;
	    }
	    offset += *segdp;
	}
    }
}

void rku_lud(d, s, len, scratch)
vec_p d, s, scratch;
int len;
{
    double_rank_sort(d, s, (vec_p)0, len, 0, scratch, 0, 1);
}

int rku_lud_scratch(len) int len; { return 2*siz_foz(len); }
unsigned int rku_lud_inplace() { return INPLACE_NONE;}

void rkd_lud(d, s, len, scratch)
vec_p d, s, scratch;
int len;
{
    double_rank_sort(d, s, (vec_p)0, len, 0, scratch, 0, 0);
}

int rkd_lud_scratch(len) int len; { return 2*siz_foz(len); }
unsigned int rkd_lud_inplace() { return INPLACE_NONE;}


void rku_led(d, s, segd, vec_len, seg_count, scratch)
vec_p d, s, segd, scratch;
int vec_len, seg_count;
{
    double_rank_sort(d, s, segd, vec_len, seg_count, scratch, 1, 1);
}

int rku_led_scratch(vec_len, seg_count) 
int vec_len,seg_count; 
{ return 2*siz_foz(vec_len); }

unsigned int rku_led_inplace() { return INPLACE_NONE;}

void rkd_led(d, s, segd, vec_len, seg_count, scratch)
vec_p d, s, segd, scratch;
int vec_len, seg_count;
{
    double_rank_sort(d, s, segd, vec_len, seg_count, scratch, 1, 0);
}

int rkd_led_scratch(vec_len, seg_count) 
int vec_len,seg_count; 
{ return 2*siz_foz(vec_len); }

unsigned int rkd_led_inplace() { return INPLACE_NONE;}
