/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University
*/

#include "defins.h"
#include <cvl.h>

/* -----------------Unsegmented Scans----------------------------------*/

/* simple scan template:
   d = destination vector
   s = source vector
   len = length of d, s
   _init = initial value (identity element)
   d and s should be vectors of the same size and type
*/
#define simpscan(_name, _func, _type, _init)			\
    void _name(d, s, len, scratch)				\
    vec_p d, s, scratch;					\
    int len;							\
    	{							\
	register _type *dest = (_type *)d;			\
	register _type *src = (_type *)s;			\
	register _type tmp, sum = _init;			\
	unroll(len, tmp = sum; sum = _func(sum, *src); *dest++=tmp;src++;)\
	}							\
    make_no_scratch(_name)					\
    make_inplace(_name,INPLACE_1)

simpscan(add_suz, plus, int, 0)		/* add scans */
simpscan(add_sud, plus, double, (double) 0.0)

simpscan(mul_suz, times, int, 1)	/* multiply scans */
simpscan(mul_sud, times, double, (double) 1.0)

simpscan(min_suz, min, int, MAX_INT)	/* min scans */
simpscan(min_sud, min, double, MAX_DOUBLE)

simpscan(max_suz, max, int, MIN_INT)	/* max scans */
simpscan(max_sud, max, double, MIN_DOUBLE)

simpscan(and_sub, and, cvl_bool, 1)	/* logical and scan */
simpscan(and_suz, band, int, ~0)	/* bitwise and scan */

simpscan(ior_sub, or, cvl_bool, 0)	/* logical or scan */
simpscan(ior_suz, bor, int, 0)		/* bitwise or scan */

simpscan(xor_sub, lxor, cvl_bool, 0)	/* logical or scan */
simpscan(xor_suz, xor, int, 0)		/* bitwise xor scan */

/* ----------------- Segmented Scans --------------------------*/

/* segmented simple scan template:
   d = destination vector
   s = source vector
   sd = segment descriptor of source
   n = number of elements in whole vector
   m = number of segments

   d and s should be vectors of the same size and type
*/
#define simpsegscan(_name, _funct, _type, _init, _unseg)		\
    void _name (d, s, sd, n, m, scratch)				\
    vec_p d, s, sd, scratch;						\
    int n, m;								\
    { 									\
	register _type *src_end = (_type *) s;				\
	register _type *src = (_type *)s;				\
	int *segd = (int *)sd;						\
	int *segd_end = (int *)sd + m;					\
	register _type sum;						\
	register _type *dest = (_type *)d;				\
	register _type tmp;						\
									\
	if (m == 1) {_unseg(d,s,n,scratch); return;}			\
	while (segd < segd_end) {					\
	    src_end += *segd++;						\
	    sum = (_type) _init; 					\
	    while (src < src_end) {					\
		tmp = sum;						\
		sum = _funct(sum, *src);				\
		*dest++ = tmp;						\
		src++;							\
	    }								\
	}								\
    }									\
    make_no_seg_scratch(_name)						\
    make_inplace(_name,INPLACE_1)

simpsegscan(add_sez, plus, int, 0, add_suz)		/* add scans */
simpsegscan(add_sed, plus, double, (double) 0.0, add_sud)

simpsegscan(mul_sez, times, int, 1, mul_suz)		/* multiply scans */
simpsegscan(mul_sed, times, double, (double) 1.0, mul_sud)

simpsegscan(min_sez, min, int, MAX_INT, min_suz)	/* min scans */
simpsegscan(min_sed, min, double, MAX_DOUBLE, min_sud)

simpsegscan(max_sez, max, int, MIN_INT, max_suz)	/* max scans */
simpsegscan(max_sed, max, double, MIN_DOUBLE, max_sud)

simpsegscan(and_seb, and, cvl_bool, 1, and_sub)		/* logical and scan */
simpsegscan(and_sez, band, int, ~0, and_suz)		/* bitwise and scan */

simpsegscan(ior_seb, or, cvl_bool, 0, ior_sub)		/* logical or scan */
simpsegscan(ior_sez, bor, int, 0, ior_suz)		/* bitwise or scan */

simpsegscan(xor_seb, lxor, cvl_bool, 0, xor_sub)	/* logical or scan */
simpsegscan(xor_sez, xor, int, 0, xor_suz)		/* bitwise xor scan */

/* --------------------Reduce Functions--------------------------------*/
/* reduce template */
	
#define reduce(_name, _funct, _type, _identity)         \
    _type _name(s, len, scratch)			\
    vec_p s, scratch;					\
    int len;						\
    {							\
      _type sum = _identity;  		                \
      _type *src = (_type *)s;				\
      unroll(len, sum = _funct(sum, *src); src++;)	\
      return sum;					\
    }							\
    make_no_scratch(_name)				\
    make_inplace(_name,INPLACE_NONE)

reduce(add_ruz, plus, int, 0)			/* add reduces */
reduce(add_rud, plus, double, (double) 0.0)

reduce(mul_ruz, times, int, 1)			/* multiply reduce */
reduce(mul_rud, times, double, (double) 1.0)

reduce(min_ruz, min, int, MAX_INT)		/* min reduces */
reduce(min_rud, min, double, MAX_DOUBLE)

reduce(max_ruz, max, int, MIN_INT)		/* max reduces */
reduce(max_rud, max, double, MIN_DOUBLE)

reduce(and_rub, and, cvl_bool, TRUE)		/* logical and reduce */
reduce(and_ruz, band, int, (~0))		/* bitwise and scan */

reduce(ior_rub, or, cvl_bool, FALSE)		/* logical or reduce */
reduce(ior_ruz, bor, int, 0)			/* bitwise or reduce */

reduce(xor_rub, lxor, cvl_bool, 0)	/* logical or reduce */
reduce(xor_ruz, xor, int, 0)		/* bitwise xor reduce */

/* ------------------Segmented Reduces ---------------------------------*/
/* segmented reduce template:
 *	d = destination vector
 *	s = source vector
 *	sd = segment descriptor of source, with components n and m
 */
/* see implementation note above */
#define segreduce(_name, _funct, _type, _identity, _unseg)	\
    void _name (d, s, sd, n, m, scratch)		\
    vec_p d, s, sd, scratch;				\
    int n, m; 						\
    {							\
	int *segd = (int *)sd;				\
	int *segd_end = (int *)sd + m;			\
	register _type *src = (_type *)s;		\
	register _type *src_end = (_type *)s;		\
	register _type sum;				\
	register _type *_dest = (_type *)d;		\
							\
	if (m == 1) {*_dest = _unseg(s,n,scratch); return;}	\
	while (segd < segd_end) {			\
	    src_end += *(segd++);			\
	    sum = _identity;				\
	    while (src < src_end)  {			\
	      sum = _funct(sum, *src);			\
	      src++;					\
	    }						\
	*(_dest++) = sum;				\
	}						\
    }							\
    make_no_seg_scratch(_name)				\
    make_inplace(_name,INPLACE_NONE)


segreduce(add_rez, plus, int, 0, add_ruz)		/* add reduces */
segreduce(add_red, plus, double, (double) 0.0, add_rud)

segreduce(mul_rez, times, int, 1, mul_ruz)		/* multiply scans */
segreduce(mul_red, times, double, (double) 1.0, mul_rud)

segreduce(min_rez, min, int, MAX_INT, min_ruz)		/* min reduces */
segreduce(min_red, min, double, MAX_DOUBLE, min_rud)

segreduce(max_rez, max, int, MIN_INT, max_ruz)		/* max reduces */
segreduce(max_red, max, double, MIN_DOUBLE, max_rud)

segreduce(and_reb, and, cvl_bool, TRUE, and_rub)	/* logical and reduce */
segreduce(and_rez, band, int, ~0, and_ruz)		/* bitwise and reduce */

segreduce(ior_reb, or, cvl_bool, FALSE, ior_rub)	/* logical or reduce */
segreduce(ior_rez, bor, int, 0, ior_ruz)		/* bitwise or reduce */

segreduce(xor_reb, lxor, cvl_bool, 0, xor_rub)		/* logical xor scan */
segreduce(xor_rez, xor, int, 0, xor_ruz)		/* bitwise xor scan */

/* -------------------Extract-------------------------------------*/

/* extract ith element from V */
/* extract template */
#define make_extract(_name, _type)			\
    _type _name (V, i, len, scratch)			\
	vec_p V, scratch;				\
	int i, len;					\
	{ return ((_type *)V)[i];}			\
    make_no_scratch(_name)				\
    make_inplace(_name,INPLACE_NONE)

make_extract(ext_vuz, int)
make_extract(ext_vub, cvl_bool)
make_extract(ext_vud, double)

/* segmented extract:
 *	d = destination vector (unsegmented),
 *	s = source vector (segmented, same type as d)
 *	i = index vector (unsegmented), length as d
 *  sd, n, m = segment descriptor for v
 */
#define make_seg_ext(_name, _type)			\
    void _name (d, s, i, sd, n, m, scratch)		\
    vec_p d, s, i, sd, scratch;				\
    int n, m;						\
    {	                                  		\
	int *index = (int *)i;				\
	int *index_end = (int *)i + m;			\
	_type *dest = (_type *)d;			\
	_type *val_vec = (_type *)s;			\
	int *segd = (int *)sd;				\
							\
	while (index < index_end) {			\
	    *(dest++) = *(val_vec + *(index++));	\
	    val_vec += *(segd++);			\
	}						\
    }							\
    make_no_seg_scratch(_name)				\
    make_inplace(_name,INPLACE_2)

make_seg_ext(ext_vez, int)
make_seg_ext(ext_veb, cvl_bool)
make_seg_ext(ext_ved, double)

/* ------------------Replace-------------------------------------*/

/* replace ith element of V with val */

#define make_replace(_name, _type, _funct)		\
  void _name(V, i, val, len, scratch)	      		\
	vec_p V, scratch;				\
	int i, len;					\
	_type val;					\
	{ ((_type *)V)[i] = _funct(val); }		\
  make_no_scratch(_name)				\
  make_inplace(_name,INPLACE_NONE)

make_replace(rep_vuz, int, ident)
make_replace(rep_vub, cvl_bool, notnot)
make_replace(rep_vud, double, ident)

/* segmented replace:
 *	d = destination vector  (segmented)
 *	s = index vector	(unsegmented, one entry per segment of d)
 *	v = value vector    (ditto)
 *	sd, n, m = segment descriptor for d.
 */
#define make_seg_replace(_name, _type)		\
    void _name(d, s, v, sd, n, m, scratch)	\
    vec_p d, s, v, sd, scratch;			\
    int n,m;					\
    {						\
	int *src = (int *)s;			\
	int *send = src + m;			\
	_type *dest = (_type *)d;		\
	_type *val = (_type *)v;		\
	int *segd = (int *)sd;			\
						\
	while (src < send) {			\
	   *(dest + *(src++)) = *(val++);	\
	   dest += *(segd++);			\
	}					\
    }						\
    make_no_seg_scratch(_name)			\
    make_inplace(_name,INPLACE_NONE)

make_seg_replace(rep_vez, int)
make_seg_replace(rep_veb, cvl_bool)
make_seg_replace(rep_ved, double)

/* ----------------Distribute-----------------------------------*/

/* distribute v to length len, return in d */
#define make_distribute(_name, _type)		\
    void _name(d, v, len, scratch)		\
    vec_p d, scratch;				\
    _type v;					\
    int len;					\
    { 						\
	register _type * dest = (_type *)d;	\
	unroll(len, *(dest++) = v;) 		\
    }						\
    make_no_scratch(_name)			\
    make_inplace(_name,INPLACE_NONE)

make_distribute(dis_vuz, int)
make_distribute(dis_vub, cvl_bool)
make_distribute(dis_vud, double)

/* segmented distribute:
 *  d = destination vector (segmented)
 *  v = value vector (unsegmented), same type as d
 *  sd, n, m = segment descriptor for d
 */
#define make_seg_distribute(_name, _type, _unseg)	\
    void _name(d, v, sd, n, m, scratch) 	\
    vec_p d, v, sd, scratch;			\
    int n, m;					\
    {						\
	_type *dest_end = (_type *)d;		\
	_type *dest = (_type *) d;		\
	int *segd = (int *)sd;			\
	int *segd_end = segd + m;		\
	_type val;				\
	_type *val_vec = (_type *)v;		\
						\
	if (m == 1) {_unseg(d,*(_type*)v,n,scratch); return;}	\
	while (segd < segd_end) {		\
	    val = *(val_vec++);			\
	    dest_end += *segd++;		\
	    while (dest < dest_end) {		\
		*dest++ = val;	               	\
	    }                         		\
	}					\
    }						\
    make_no_seg_scratch(_name)			\
    make_inplace(_name,INPLACE_NONE)

make_seg_distribute(dis_vez, int, dis_vuz)
make_seg_distribute(dis_veb, cvl_bool, dis_vub)
make_seg_distribute(dis_ved, double, dis_vud)


/* --------------Permute---------------------------------------*/

/* simple permute: 
 *	d = destination vector
 *	s = source vector, same type as d
 *	i = index vector
 *	len = length of vectors
 */

#define make_smpper(_name, _type)			\
    void _name(d, s, i, len, scratch)			\
    vec_p d, s, i, scratch;				\
    int len;						\
    {							\
	register int *indexp = (int *)i;		\
	register _type *dest = (_type *)d;		\
	register _type *src = (_type *)s;		\
	unroll(len, dest[*(indexp++)] = *(src++);)	\
    }							\
    make_no_scratch(_name)				\
    make_inplace(_name,INPLACE_NONE)

make_smpper(smp_puz, int)
make_smpper(smp_pub, cvl_bool)
make_smpper(smp_pud, double)

/* segmented simple permute:
 *  d = destination vector (segmented)
 *  s = source vector (segmented), same type as d
 *  i = index vector (segmented)
 *  sd, n, m = segment descriptor
 */
#define make_seg_smpper(_name, _type, _unseg)		\
    void _name(d, s, i, sd, n, m, scratch)		\
    vec_p d, s, i, sd, scratch;				\
    int n, m;						\
    {							\
	_type *src = (_type *)s;			\
	_type *src_end = (_type *)s;			\
	int *segd = (int *)sd;				\
	int *segd_end = segd + m;			\
	_type *dest = (_type *)d;			\
	int *index = (int *)i;				\
							\
	if (m == 1) {_unseg(d,s,i,n,scratch); return;}	\
	while (segd < segd_end) {			\
	    src_end += *(segd);				\
	    while (src < src_end)  {			\
		*(dest + *(index++)) = *(src++);	\
	    }						\
	    dest += *(segd++);				\
	}						\
    }							\
    make_no_seg_scratch(_name)				\
    make_inplace(_name,INPLACE_NONE)

make_seg_smpper(smp_pez, int, smp_puz)
make_seg_smpper(smp_peb, cvl_bool, smp_pub)
make_seg_smpper(smp_ped, double, smp_pud)

/*----------------------Back Permute-------------------*/
/* back permute: 
 *	d = destination vector
 *	s = source vector, same type as d
 *	i = index vector
 *	s_len = length of s
 *	d_len = length of d and i
 */

#define make_bckper(_name, _type)			\
	void _name(d, s, i, s_len, d_len, scratch)	\
	vec_p d, s, i, scratch;				\
	int s_len, d_len;				\
	{						\
	    register int *index = (int *)i;		\
	    register _type *dest = (_type *)d;		\
	    register _type *src = (_type *)s;		\
	    unroll(d_len, *dest++ = src[*index++];) 	\
	}						\
	make_no2_scratch(_name)				\
	make_inplace(_name,INPLACE_2)

make_bckper(bck_puz, int)
make_bckper(bck_pub, cvl_bool)
make_bckper(bck_pud, double)

/* segmented bck permute:
 *  d = destination vector (segmented)
 *  s = source vector (segmented), same type as d
 *  i = index vector (compatible with d)
 *  sd_s, n_s, m_s = source segment descriptor
 *  sd_d, n_d, n_d = dest segment descriptor
 */
#define make_seg_bckper(_name, _type, _unseg)	\
    void _name(d, s, i, sd_s, n_s, m_s, sd_d, n_d, m_d, scratch)	\
    vec_p d, s, i, sd_s, sd_d, scratch;		\
    int n_s, m_s, n_d, m_d;			\
    {						\
	_type *src = (_type *)s;		\
	int *segd_dest = (int *)sd_d;		\
	int *segd_src = (int *)sd_s;		\
	int *segd_end = segd_dest + m_d;	\
	_type *dest = (_type *)d;		\
	int *index = (int *)i;			\
	int *index_end = index;			\
						\
	if (m_s == 1) {_unseg(d,s,i,n_s,n_d,scratch); return;}	\
	while (segd_dest < segd_end) {		\
	    index_end += *(segd_dest++);	\
	    while (index < index_end)  {	\
		*(dest++) = *(src + *index++);	\
	    }					\
	    src += *(segd_src++);		\
	}					\
    }						\
    make_no_seg2_scratch(_name)			\
    make_inplace(_name,INPLACE_2)

make_seg_bckper(bck_pez, int, bck_puz)
make_seg_bckper(bck_peb, cvl_bool, bck_pub)
make_seg_bckper(bck_ped, double, bck_pud)
