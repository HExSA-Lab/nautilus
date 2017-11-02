/*
 * Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University
 */

#include <math.h>
#include "defins.h"
#include <cvl.h>
#include <stdio.h>
#include <assert.h>
#include "ndpc_preempt_threads.h"

void chunk_range(int * vals, int index, int length, int num_procs) {
	if (length < num_procs) {
		if (index < length) {
			vals[0] = index;
			vals[1] = index + 1;
		} else {
			vals[0] = -1;
			vals[1] = -1;
		}
		return;
	}
	int spill_over = length % num_procs;
	int chunk_size = length / num_procs;
	if (index < spill_over) {
		vals[0] =  (index) + (index * chunk_size);
		vals[1] = vals[0] + chunk_size + 1;
	} else {
		vals[0] = spill_over + (index * chunk_size);
		vals[1] = vals[0] + chunk_size;
	}
	return;
}

/* CHUNK_RANGE TESTS */
int test_len_less_than_num_procs() {

	int NUM_PROCS = 12;
	int length_of_work = NUM_PROCS-1;
	int vals[2];

	int i;
	for (i = 0; i < NUM_PROCS; i ++) {
		chunk_range(vals, i, length_of_work, NUM_PROCS);
		if (i != NUM_PROCS - 1) {
			assert(vals[0] == i);
			assert(vals[1] == i + 1);
		} else {
			assert(vals[0] == -1);
			assert(vals[1] == -1);
		}
	}
}

int test_len_double_num_procs() {

	int NUM_PROCS = 12;
	int length_of_work = 2 * NUM_PROCS;
	int vals[2];

	int i;
	for (i = 0; i < NUM_PROCS; i ++) {
		chunk_range(vals, i, length_of_work, NUM_PROCS);
			assert(vals[0] == 2*i);
			assert(vals[1] == 2*i+2);
	}
}

int test_len_greater_num_proc_prime_spillover() {

	int NUM_PROCS = 13;
	int length_of_work = 2 * NUM_PROCS + 5;
	int vals[2];
	int i;
	for (i = 0; i < NUM_PROCS; i ++) {
		chunk_range(vals, i, length_of_work, NUM_PROCS);
			assert(vals[0] == 2*i + i*(i < 5 ? 1 : 0)+ (i >= 5 ? 5 : 0));
			assert(vals[1] == vals[0] + 2 + (i < 5 ? 1 :0));
	}
}
/*END CHUNK_RANGE TESTS*/


/* This file has lots of ugly C preprocessor macros for generating
 * elementwise CVL operations.
 */

/* Some loop unrolling macros */

/* use the tmp macros when we want to allow overlap between 
 * successive iteration.  the normal loop unroll macros don't
 * capture this and generate bad code
 */
#define unrolltmp1d1s(op,len,_type)             \
		{                                              \
	register int _lenS2;                        \
	register _type t1,t2,t3,t4;                 \
	switch (len & 0x3) {                        \
	case 3: *dest++ = op(*src++);           \
	case 2: *dest++ = op(*src++);           \
	case 1: *dest++ = op(*src++);           \
	default: ;                              \
	}                                           \
	\
	_lenS2 = len >> 2;                          \
	while (_lenS2) {                            \
		t1 = op(src[0]);                        \
		t2 = op(src[1]);                        \
		t3 = op(src[2]);                        \
		t4 = op(src[3]);                        \
		dest[0] = t1;                           \
		dest[1] = t2;                           \
		dest[2] = t3;                           \
		dest[3] = t4;                           \
		--_lenS2;                               \
		dest += 4; src += 4;                    \
	}                                           \
		}

#define unroll1d1s(op,len,_type)                \
		{                                              \
	int NUM_PROCS = 4;							\
	thread_id_t tid[NUM_PROCS];					\
	int i;										\
	int range[2];								\
	for (i = 0; i < NUM_PROCS; i++) {			\
		chunk_range(range, i, len, NUM_PROCS);  \
		if (range[0] == -1)						\
			continue; 							\
		tid[i] = ndpc_fork_preempt_thread();	\
		if (tid[i] == ndpc_my_preempt_thread()) {	\
			printk("unroll1d1s Child %d (%p) - break\n",i, ndpc_my_preempt_thread()); break;								\
		}										\
	}											\
	if (tid[i] == ndpc_my_preempt_thread()) {		\
                int j; printk("unroll1d1s Child %d (%p) - do work\n",i, ndpc_my_preempt_thread()); \
		for (j = range[0];j < range[1]; j ++) {				\
			dest[j] = op(src[j]); 		\
		}	printk("unroll1d1s Child %d (%p)- done work\n",i, ndpc_my_preempt_thread()); return; 									\
	} else {  									\
	    printk("unroll1d1s Parent (%p) - join\n", ndpc_my_preempt_thread()); ndpc_join_child_preempt_threads();printk("unroll1d1s Parent (%p)- join done\n", ndpc_my_preempt_thread()); \
	}											\
	}

// when is it better split over multiple processors, optimize later perhaps
#define unrolltmp1d2s(op,len,_type)        		\
	{                                       \
	int NUM_PROCS = 4;							\
	thread_id_t tid[NUM_PROCS];					\
	int i;										\
	int range[2];								\
	for (i = 0; i < NUM_PROCS; i++) {			\
		chunk_range(range, i, len, NUM_PROCS);  \
		if (range[0] == -1)						\
			continue; 							\
		tid[i] = ndpc_fork_preempt_thread();	\
		if (tid[i] == ndpc_my_preempt_thread()) {	\
			printk("unrolltmp1d2s Child %d (%p) - break\n", i, ndpc_my_preempt_thread()); break;								\
		}										\
	}											\
	if (tid[i] == ndpc_my_preempt_thread()) {		\
                int j; printk("unrolltmp1d2s Child %d (%p) - do work\n",i, ndpc_my_preempt_thread()); \
		for (j = range[0];j < range[1]; j ++) {				\
			dest[j] = op(src1[j], src2[j]); 		\
		} printk("unrolltmp1d2s Child %d (%p) - done work\n", i, ndpc_my_preempt_thread()); return; 										\
	} else {  									\
		printk("unrolltmp1d2s Parent (%p) - join\n", ndpc_my_preempt_thread()); ndpc_join_child_preempt_threads();  printk("unrolltmp1d2s Parent (%p) - join done\n", ndpc_my_preempt_thread());		\
	}											\
	}

#define unrolltmp1d3s(op,len,_type)        \
		{                                       \
		int NUM_PROCS = 4;							\
		thread_id_t tid[NUM_PROCS];					\
		int i;										\
		int range[2];								\
		for (i = 0; i < NUM_PROCS; i++) {			\
			chunk_range(range, i, len, NUM_PROCS);  \
			if (range[0] == -1)						\
				continue; 							\
			tid[i] = ndpc_fork_preempt_thread();	\
			if (tid[i] == ndpc_my_preempt_thread()) {	\
				printk("unrolltmp1d3s Child %d (%p) - break\n", i, ndpc_my_preempt_thread()); break;								\
			}										\
		}											\
		if (tid[i] == ndpc_my_preempt_thread()) {		\
			int j; printk("unrolltmp1d3s Child %d (%p) - do work\n", i, ndpc_my_preempt_thread()); \
			for (j = range[0];j < range[1]; j ++) {				\
				dest[j] = op(src1[j], src2[j], src3[j]); 	\
			}	printk("unrolltmp1d3s Child %d (%p) - done work\n", i, ndpc_my_preempt_thread()); return;									\
		} else {  									\
			printk("unrolltmp1d3s Parent - join\n", ndpc_my_preempt_thread()); ndpc_join_child_preempt_threads();  printk("unrolltmp1d3s Parent (%p) - join done\n", ndpc_my_preempt_thread());		\
		}											\
		}


/* --------------Function definition macros --------------------*/

#define onefuntmp(_name, _funct, _srctype, _desttype)       \
		void _name (d, s, len, scratch)                         \
		vec_p d, s, scratch;                                    \
		int len;                                                \
		{                                                       \
			register _desttype *dest = (_desttype *) d;         \
			register _srctype *src = (_srctype *) s;            \
			unrolltmp1d1s(_funct, len, _desttype)		    \
		}                                                       \
		make_no_scratch(_name)                                  \
		make_inplace(_name,INPLACE_1)

#define onefun(_name, _funct, _srctype, _desttype)          \
		void _name (d, s, len, scratch)                         \
		vec_p d, s, scratch;                                    \
		int len;                                                \
		{                                                       \
			register _desttype *dest = (_desttype *) d;         \
			register _srctype *src = (_srctype *) s;            \
			unroll1d1s(_funct, len, _desttype)		    \
		}                                                       \
		make_no_scratch(_name)                                  \
		make_inplace(_name,INPLACE_1)

#define twofun(_name, _funct, _srctype, _desttype)          \
		void _name (d, s1, s2, len, scratch)                    \
		vec_p d, s1, s2, scratch;                               \
		int len;                                                \
		{                                                       \
			register _desttype *dest = (_desttype *) d;         \
			register _srctype *src1 = (_srctype *) s1;          \
			register _srctype *src2 = (_srctype *) s2;          \
			unrolltmp1d2s(_funct, len, _desttype)		    \
		}                                                       \
		make_no_scratch(_name)                                  \
		make_inplace(_name,INPLACE_1|INPLACE_2)

#define selfun(_name, _funct, _type)                        \
		void _name (d, s1, s2, s3, len, scratch)                \
		vec_p d, s1, s2, s3, scratch;                           \
		int len;                                                \
		{                                                       \
			_type *dest = (_type *) d;                          \
			cvl_bool *src1 = (cvl_bool *) s1;                   \
			_type *src2 = (_type *) s2;                         \
			_type *src3 = (_type *) s3;                         \
			unrolltmp1d3s(_funct, len, _type)		    \
		}                                                       \
		make_no_scratch(_name)                                  \
		make_inplace(_name,INPLACE_1|INPLACE_2|INPLACE_3)

#define make_two_zd(_basename, _funct)                     \
		twofun(GLUE(_basename,z), _funct, int, int)            \
		twofun(GLUE(_basename,d), _funct, double, double)

#define make_two_cvl_bool_bzd(_basename, _funct)           \
		twofun(GLUE(_basename,b), _funct, cvl_bool, cvl_bool)  \
		twofun(GLUE(_basename,z), _funct, int, cvl_bool)       \
		twofun(GLUE(_basename,d), _funct, double, cvl_bool)

#define make_two_cvl_bool_zd(_basename, _funct)            \
		twofun(GLUE(_basename,z), _funct, int, cvl_bool)       \
		twofun(GLUE(_basename,d), _funct, double, cvl_bool)

/* Arithmetic functions, min and max, only defined on z, f, d */
make_two_zd(max_wu, max)
make_two_zd(min_wu, min)
make_two_zd(add_wu, plus)
make_two_zd(sub_wu, minus)
make_two_zd(mul_wu, times)
make_two_zd(div_wu, divide)

make_two_cvl_bool_zd(grt_wu, gt)
make_two_cvl_bool_zd(les_wu, lt)
make_two_cvl_bool_zd(geq_wu, geq)
make_two_cvl_bool_zd(leq_wu, leq)


/* shifts, mod and random only on integers */
twofun(lsh_wuz, lshift, int, int)
twofun(rsh_wuz, rshift, int, int)
twofun(mod_wuz, mod, int, int)
onefun(rnd_wuz, cvlrand, int, int)

/* comparison functions: valid on all input types and returns a cvl_bool */
make_two_cvl_bool_bzd(eql_wu, eq)
make_two_cvl_bool_bzd(neq_wu, neq)

/* selection functions */
selfun(sel_wuz, select, int) 
selfun(sel_wub, select, cvl_bool) 
selfun(sel_wud, select, double) 

/* logical functions */
onefuntmp(not_wub, not, cvl_bool, cvl_bool)
twofun(xor_wub, xor, cvl_bool, cvl_bool)

twofun(ior_wub, or, cvl_bool, cvl_bool)    /* be careful!: && and || short circuit */
twofun(and_wub, and, cvl_bool, cvl_bool)

/* bitwise functions */
twofun(ior_wuz, bor, int, int)
twofun(and_wuz, band, int, int)
onefuntmp(not_wuz, bnot, int, int)
twofun(xor_wuz, xor, int, int)

/* conversion routines */
onefuntmp(int_wud, d_to_z, double, int)
onefuntmp(int_wub, b_to_z, cvl_bool, int)
onefuntmp(dbl_wuz, z_to_d, int, double)
onefuntmp(boo_wuz, z_to_b, int, cvl_bool)

/* double routines */
onefun(flr_wud, cvl_floor, double, int)
onefun(cei_wud, cvl_ceil, double, int)
onefun(trn_wud, d_to_z, double, int)
onefun(rou_wud, cvl_round, double, int)
onefun(exp_wud, exp, double, double)
onefun(log_wud, log, double, double)
onefun(sqt_wud, sqrt, double, double)
onefun(sin_wud, sin, double, double)
onefun(cos_wud, cos, double, double)
onefun(tan_wud, tan, double, double)
onefun(asn_wud, asin, double, double)
onefun(acs_wud, acos, double, double)
onefun(atn_wud, atan, double, double)
onefun(snh_wud, sinh, double, double)
onefun(csh_wud, cosh, double, double)
onefun(tnh_wud, tanh, double, double)

onefuntmp(cpy_wuz, ident, int, int)
onefuntmp(cpy_wub, ident, cvl_bool, cvl_bool)
onefuntmp(cpy_wud, ident, double, double)

void cpy_wus(d, s, n, m, scratch)
vec_p d, s, scratch;
int n, m;
{
	cpy_wuz(d, s, m, scratch);
}
int cpy_wus_scratch(n, m) int n, m; { return cpy_wuz_scratch(m); }
make_inplace(cpy_wus,INPLACE_1)
