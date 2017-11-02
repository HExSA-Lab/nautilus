/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University
*/

/* This file contains some library functions */
#include "defins.h"
#include <cvl.h>

/*----------------------------index (iota)-----------------*/
/* Index(len) creates an integer vector of the numbers */

void ind_luz(d, init, stride, count, scratch)
vec_p d, scratch;
int init, stride, count;
{                                 
  int *dest = (int *)d;
  int i;
  int curr = init;
  for (i = 0; i < count; i++) {
    dest[i] = curr;
    curr += stride;
  }
}
make_no_scratch(ind_luz)
make_inplace(ind_luz,INPLACE_NONE)

/* Segmented index creates a segmented vector of index results, given a 
 * vector of lengths.
 */
void ind_lez(d, init, stride, dest_segd, vec_len, seg_count, scratch)    
vec_p d, init, stride, dest_segd, scratch;
int vec_len, seg_count;      
{                                 
  int *dest = (int *)d;
  int i, seg;
  int curr;
  for (seg = 0; seg < seg_count; seg++) {
	int this_stride = ((int *)stride)[seg];
	int this_init = ((int *)init)[seg];
	int elts_in_seg = ((int *)dest_segd)[seg];
	curr = this_init;
	for (i = 0; i < elts_in_seg; i++) {
		*(dest++) = curr;
		curr += this_stride;
	}                             
  }                               
}
make_no_seg_scratch(ind_lez)
make_inplace(ind_lez,INPLACE_NONE)

/* -------------------------- pack --------------------------*/

/* The pack functions take a source vector and a flag vector and return
 * in a destintation vector all those elements corresponding to True.
 *
 * Pack is divided into 2 phases to allow memeory to be allocated for
 * the result of the pack:
 * pk1 returns the number of true flags in the flag vector
 * pk2 does the pack.
 *
 * pk1:
 *	f = cvl_bool flag vector
 *	len = length of vectors
 */

int pk1_luv(f, len, scratch)
vec_p f, scratch;
int len;
{
    cvl_bool *flags = (cvl_bool *)f;
    int count = 0;

    unroll(len, if (*flags++) count++;)
    return count;
}
make_no_scratch(pk1_luv)
make_inplace(pk1_luv,INPLACE_NONE)

#define make_pk2(_name, _type)					\
	void _name (d, s, f, src_len, dest_len, scratch)	\
	vec_p d, s, f, scratch;					\
	int src_len, dest_len;					\
{								\
	_type *dest = (_type *)d;				\
	_type *src = (_type *)s;				\
	cvl_bool *flags = (cvl_bool *)f;			\
	unroll(src_len,						\
	if (*flags++) *dest++ = *src;				\
		src++;)						\
	return;							\
}								\
	make_no_scratch(_name)					\
	make_inplace(_name,INPLACE_NONE)

make_pk2(pk2_luz, int)
make_pk2(pk2_lub, cvl_bool)
make_pk2(pk2_lud, double)

/* segmented pack: Packs a segmented vector into dest and creates
 * segment descriptor for it in seg_dest
 * Pack is split up into two parts.  The first takes the 
 * packing flag vector and its segd and returns the lengths vector
 * describing the result of the final pack.
 */

/* pack1: returns the lengths vector of the result of the pack
 *	ds = destination segment descriptor
 * 	f = boolean flags (same length as s)
 * 	sd = source segment descriptor
 *	n = length of f
 *	m = number of segments of f
 */

void pk1_lev(ds, f, sd, n, m, scratch)
vec_p ds, f, sd, scratch;
int n, m;
{		
    int *dest_sd = (int *)ds;
    int *segd_flag = (int *)sd;
    int *segd_flag_end = segd_flag + m;
    cvl_bool *flag = (cvl_bool *)f;
    cvl_bool *flag_end = flag;

    while (segd_flag < segd_flag_end) {
	int count = 0;
	flag_end += *(segd_flag++);
	while (flag < flag_end) {
	    if (*flag++) count++;
	}
	*dest_sd++ = count;	
    }
    return;
}				
make_no_seg_scratch(pk1_lev)
make_inplace(pk1_lev, INPLACE_NONE)

/* pack2: returns the pack 
 */
#define make_pack2(_name, _type)				\
	void _name (d, s, f, sd_s, n_s , m_s, sd_d, n_d, m_d, scratch)\
	vec_p d, s, f, sd_s, sd_d, scratch;			\
	int n_s, m_s, n_d, m_d;					\
    {								\
	_type *dest = (_type *)d;				\
	cvl_bool *flag = (cvl_bool *)f;				\
	_type *src = (_type *)s;				\
	int *segd_src = (int *)sd_s;				\
	int *segd_src_end = segd_src + m_s;			\
	_type *src_end = src;					\
								\
	while (segd_src < segd_src_end) {			\
	    src_end += *(segd_src++);				\
	    while (src < src_end) {				\
		if (*flag++) *dest++ = *src;			\
		src++;						\
	    }							\
	}							\
    }								\
    make_no_seg2_scratch(_name)					\
    make_inplace(_name, INPLACE_NONE)

make_pack2(pk2_lez, int)
make_pack2(pk2_leb, cvl_bool)
make_pack2(pk2_led, double)

