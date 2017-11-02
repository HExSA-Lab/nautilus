/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University
*/

/* This file contains some of the permute functions */
#include "defins.h"
#include <cvl.h>
#include <string.h>

/*---------------------fpermute----------------------------*/
/* The fpm functions perform a select permute on the source 
 * vector according to a set of flags.
 * 	d = dest   
 *	s = source (same type as d)
 *	i = index (compatible with d)
 *	f = flags (compatible with d)
 *	len_src = length of source vector
 *	len_dest = length of dest (number T in f)
 */

#define make_fpm(_name, _type)					\
	void _name(d, s, i, f, len_src, len_dest, scratch)	\
	vec_p d, s, i, f, scratch;				\
	int len_src, len_dest;					\
    {								\
	_type *dest = (_type *)d;				\
	_type *src = (_type *)s;				\
	cvl_bool *flags = (cvl_bool *)f;			\
	int *index = (int *)i;					\
    unroll(len_src,						\
	if (*flags++) dest[*(index++)] = *src++;		\
	else {index++; src++;}					\
       )							\
    }								\
make_no2_scratch(_name)						\
make_inplace(_name, INPLACE_NONE)


make_fpm(fpm_puz, int)
make_fpm(fpm_pub, cvl_bool)
make_fpm(fpm_pud, double)

#define make_seg_fpm(_name, _type, _unseg)				\
	void _name(d, s, i, f, sd_s, n_s, m_s, sd_d, n_d, m_d, scratch)	\
	vec_p d, s, i, f, sd_s, sd_d, scratch;			\
	int n_s, m_s, n_d, m_d;					\
    {								\
	register _type *dest = (_type *)d;			\
	register _type *src = (_type *)s;			\
	_type *src_end = (_type *)s;				\
	register cvl_bool *flags = (cvl_bool *)f;		\
	register int *index = (int *)i;				\
	int *segd_src = (int *)sd_s;				\
	int *segd_src_end = segd_src + m_s;			\
	int *segd_dest = (int *) sd_d;				\
								\
	if (m_s == 1) {_unseg(d,s,i,f,n_s,n_d,scratch); return;}\
	while (segd_src < segd_src_end) {			\
	    src_end += *(segd_src++);				\
	    while (src < src_end) {				\
		if (*flags++) *(dest + *(index++)) = *src++;	\
		else {index++; src++;}				\
	    }							\
	    dest += *(segd_dest++);				\
	}							\
    }								\
make_no_seg2_scratch(_name)					\
make_inplace(_name, INPLACE_NONE)

make_seg_fpm(fpm_pez, int, fpm_puz)
make_seg_fpm(fpm_peb, cvl_bool, fpm_pub)
make_seg_fpm(fpm_ped, double, fpm_pud)

/*------------------------Bfpermute-------------------------*/
/* inverse-permute with flags.  Unfilled positions in vector
 * set to 0 (int, double) or F (bools).
	d = destination
	s = source	 (same type as d)
	i = index vector (same length as d)
	f = flags	 (same length as d)
	len_src          
	len_dest
 */

#define make_bfp(_name, _type)					\
	void _name(d, s, i, f, len_src, len_dest, scratch)	\
	vec_p d, s, i, f, scratch;				\
	int len_src, len_dest;					\
    {								\
	_type *dest = (_type *)d;				\
	_type *src = (_type *)s;				\
	cvl_bool *flags = (cvl_bool *)f;			\
	int *index = (int *)i;					\
    unroll(len_dest,						\
	if (*flags++) *dest = src[*index];			\
	else *dest = (_type) 0;          			\
        index++; dest++;                                        \
       )							\
    }								\
make_no2_scratch(_name)						\
make_inplace(_name, INPLACE_NONE)

make_bfp(bfp_puz, int)
make_bfp(bfp_pub, cvl_bool)
make_bfp(bfp_pud, double)

#if __STDC__
#define bzero(p,c) memset((p), (char)0, (c))
#else
extern void bzero();
#endif

#define make_seg_bfp(_name, _type, _unseg)			\
    void _name(d, s, i, f, sd_s, n_s, m_s, sd_d, n_d, m_d, scratch)  \
    vec_p d, s, i, f, sd_s, sd_d, scratch;			\
    int n_s, m_s, n_d, m_d;					\
    {								\
	_type *src = (_type *)s;				\
	_type *dest; 						\
	int *segd_dest = (int *) sd_d;				\
	int *segd_src = (int *) sd_s;				\
	int *segd_end = segd_src + m_s;				\
	cvl_bool *flags = (cvl_bool *)f;			\
	cvl_bool *flags_end = flags;				\
	int *index = (int *)i;					\
								\
	if (m_s == 1) {_unseg(d,s,i,f,n_s,n_d,scratch); return;}\
	dest = (_type *)d;					\
	bzero(dest, n_d * sizeof(_type));			\
	while (segd_src < segd_end) {			\
	    flags_end += *segd_dest++;			\
	    while (flags < flags_end) {			\
		if (*flags++) *dest++ = src[*index++];	\
		else {index++; dest++;}			\
	    }						\
	    src += *(segd_src++);				\
	}							\
    }							\
make_no_seg2_scratch(_name)					\
make_inplace(_name, INPLACE_NONE)

make_seg_bfp(bfp_pez, int, bfp_puz)
make_seg_bfp(bfp_peb, cvl_bool, bfp_pub)
make_seg_bfp(bfp_ped, double, bfp_pud)

/* ------------------------dpermute ----------------------------*/
/* permute with default.  Any element not filled by the permute
 * gets set to the corresponding element of the default vector.
 * first copy default into dest, then do permute.
 * 	d = destination 
 *	s = source (same type as d)
 *	i = index vector (same length as s)
 *	v = default vector (same length as d)
 *	len_src 
 *	len_dest
 */
void dpe_puz(d, s, i, v, len_src, len_dest, scratch)
    vec_p d, s, i, v, scratch;
    int len_src, len_dest;
    {
    if (d != v) cpy_wuz(d, v, len_dest, scratch);
    smp_puz(d, s, i, len_src, scratch);
    }
make_inplace(dpe_puz, INPLACE_3)
make_no2_scratch(dpe_puz)

void dpe_pub(d, s, i, v, len_src, len_dest, scratch)
    vec_p d, s, i, v, scratch;
    int len_src, len_dest;
    {
    if (d != v) cpy_wub(d, v, len_dest, scratch);
    smp_pub(d, s, i, len_src, scratch);
    }
make_inplace(dpe_pub, INPLACE_3)
make_no2_scratch(dpe_pub)

void dpe_pud(d, s, i, v, len_src, len_dest, scratch)
    vec_p d, s, i, v, scratch;
    int len_src, len_dest;
    {
    if (d != v) cpy_wud(d, v, len_dest, scratch);
    smp_pud(d, s, i, len_src, scratch);
    }
make_inplace(dpe_pud, INPLACE_3)
make_no2_scratch(dpe_pud)

/* Can't do segmented version the same way, since simple permute
 * requires the same segdes for src and dest.  (compare this to code
 * for dpe_wez).
 */

#define make_seg_dpe(_type_let,_type,_unseg)			\
void GLUE(dpe_pe,_type_let) (d, s, i, v, sd_s, n_s, m_s, sd_d, n_d, m_d, scratch)\
    vec_p d, s, i, v, sd_s, sd_d, scratch;			\
    int n_s, n_d, m_s, m_d;					\
{								\
    _type *src = (_type *)s;					\
    _type *dest = (_type *)d;					\
    int *index = (int *)i;					\
    int *segd_src = (int *)sd_s;				\
    int *segd_src_end = segd_src + m_s;				\
    int *segd_dest = (int *)sd_d;				\
    _type *src_end = src;					\
								\
    if (m_s == 1) {_unseg(d,s,i,v,n_s,n_d,scratch); return;}	\
    if (d != v) GLUE(cpy_wu,_type_let) (d, v, n_d, scratch);	\
    while (segd_src < segd_src_end) {				\
	src_end += *segd_src++;					\
	while (src < src_end) {					\
	    *(dest + *(index++)) = *src++;			\
	}							\
	dest += *segd_dest++;					\
    }								\
}

make_seg_dpe(z,int,dpe_puz)
make_seg_dpe(b,cvl_bool,dpe_pub)
make_seg_dpe(d,double,dpe_pud)

make_inplace(dpe_pez, INPLACE_3)
int dpe_pez_scratch(n_in, m_in, n_out, m_out) int n_in, m_in, n_out, m_out; {return cpy_wuz_scratch(n_out);}

make_inplace(dpe_peb, INPLACE_3)
int dpe_peb_scratch(n_in, m_in, n_out, m_out) int n_in, m_in, n_out, m_out; {return cpy_wub_scratch(n_out);}

make_inplace(dpe_ped, INPLACE_3)
int dpe_ped_scratch(n_in, m_in, n_out, m_out) int n_in, m_in, n_out, m_out; {return cpy_wud_scratch(n_out);}

/*----------------------dfpermute----------------------------*/
/* default permute with flags.  Any element not filled by the fpermute
 * gets set to the corresponding element of the default vector.
 * first copy default into dest, then do fpermute.
 * 	d = destination 
 *	s = source (same type as d)
 *	i = index vector (same length as s)
 *	v = default vector (same length as d)
 *	len_src 
 *	len_dest
 */
#define make_dfp(_type_let)					\
void GLUE(dfp_pu,_type_let) (d, s, i, f, v, len_src, len_dest, scratch)\
    vec_p d, s, i, f, v, scratch;				\
    int len_src, len_dest;					\
{								\
    if (d != v) GLUE(cpy_wu,_type_let) (d, v, len_dest, scratch);	\
    GLUE(fpm_pu,_type_let) (d, s, i, f, len_src, len_dest, scratch);	\
}
make_dfp(z)
make_dfp(b)
make_dfp(d)

make_inplace(dfp_puz, INPLACE_4)
make_no2_scratch(dfp_puz)

make_inplace(dfp_pub, INPLACE_4)
make_no2_scratch(dfp_pub)

make_inplace(dfp_pud, INPLACE_4)
make_no2_scratch(dfp_pud)

#define make_seg_dfp(_type_let)					\
void GLUE(dfp_pe,_type_let) (d, s, i, f, v, sd_s, n_s, m_s, sd_d, n_d, m_d, scratch)\
    vec_p d, s, i, f, v, sd_s, sd_d, scratch;			\
    int n_s, n_d, m_s, m_d;					\
{								\
    if (d != v) GLUE(cpy_wu,_type_let) (d, v, n_d, scratch);	\
    if (m_s == 1) {						\
	GLUE(fpm_pu,_type_let) (d, s, i, f, n_s, n_d, scratch); \
    } else {							\
        GLUE(fpm_pe,_type_let) (d, s, i, f, sd_s, n_s, m_s, sd_d, n_d, m_d, scratch);\
    }								\
}

make_seg_dfp(z)
make_seg_dfp(b)
make_seg_dfp(d)

make_inplace(dfp_pez, INPLACE_4)
make_no_seg2_scratch(dfp_pez)

make_inplace(dfp_peb, INPLACE_4)
make_no_seg2_scratch(dfp_peb)

make_inplace(dfp_ped, INPLACE_4)
make_no_seg2_scratch(dfp_ped)
