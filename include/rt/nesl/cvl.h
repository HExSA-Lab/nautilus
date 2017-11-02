/*
* Copyright (c) 1992, 1993, 1994, 1995
* Carnegie Mellon University SCAL project: 
* Guy Blelloch, Jonathan Hardwick, Jay Sipelstein, Marco Zagha
*
* All Rights Reserved.
*
* Permission to use, copy, modify and distribute this software and its
* documentation is hereby granted, provided that both the copyright
* notice and this permission notice appear in all copies of the
* software, derivative works or modified versions, and any portions
* thereof, and that both notices appear in supporting documentation.
*
* CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
* CONDITION.  CARNEGIE MELLON AND THE SCAL PROJECT DISCLAIMS ANY
* LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM
* THE USE OF THIS SOFTWARE.
*
* The SCAL project requests users of this software to return to 
*
*  Guy Blelloch				nesl-contribute@cs.cmu.edu
*  School of Computer Science
*  Carnegie Mellon University
*  5000 Forbes Ave.
*  Pittsburgh PA 15213-3890
*
* any improvements or extensions that they make and grant Carnegie Mellon
* the rights to redistribute these changes.
*/

#ifndef cvl_h
#define cvl_h

#if __STDC__
# define HAS_VOIDP 1
#endif  /* __STDC__ */

#if CM2
# include <cm/paris.h>
  typedef CM_field_id_t vec_p;
#else
#if _MPL | MASPAR | MPI
  typedef int vec_p;
#else
#if HAS_VOIDP
  typedef void *vec_p;
#else
  typedef char *vec_p;
#endif /* HAS_VOIDP */
#endif /* _MPL | MASPAR | MPI */
#endif /* CM2 */

#if MPI
  typedef char cvl_bool;
#else
#if _MPL | MASPAR | CM2 
  typedef unsigned cvl_bool;
#else
  typedef int cvl_bool;
#endif /* _MPL | MASPAR | CM2 */
#endif /* MPI */

#if cray
  typedef unsigned long cvl_timer_t;
#else
#if _MPL | MASPAR | CM2 | CM5 | MPI
  typedef double cvl_timer_t;
#else
  typedef struct CVL_TIMER_T {
    long sec, usec;
  } cvl_timer_t;
#endif /* _MPL | MASPAR | CM2 | CM5 | MPI */
#endif /* cray */

#if _MPL
# include <unc_globals.h>
/* #include <unx_unwrap.h> */
#else
#if MASPAR
# include <unc_wrap.h>
#endif /* MASPAR */
#endif /* _MPL */

#define CVL_SCRATCH_NULL ((vec_p) NULL)
#define INPLACE_NONE 0
#define INPLACE_1 0x001
#define INPLACE_2 0x002
#define INPLACE_3 0x004
#define INPLACE_4 0x008
#define INPLACE_5 0x010
#define INPLACE_6 0x020
#define INPLACE_7 0x040
#define INPLACE_8 0x080
#define INPLACE_9 0x100
#define INPLACE_10 0x200
#define INPLACE_11 0x400
#define INPLACE_12 0x800

#if __STDC__ | __cplusplus | c_plusplus
# define P_(s) s
#else
# define P_(s) ()
#endif

#if __cplusplus | c_plusplus
extern "C" {
#endif
void max_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int max_wuz_scratch P_((int len));
unsigned int max_wuz_inplace P_((void));
void max_wud P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int max_wud_scratch P_((int len));
unsigned int max_wud_inplace P_((void));
void min_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int min_wuz_scratch P_((int len));
unsigned int min_wuz_inplace P_((void));
void min_wud P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int min_wud_scratch P_((int len));
unsigned int min_wud_inplace P_((void));
void add_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int add_wuz_scratch P_((int len));
unsigned int add_wuz_inplace P_((void));
void add_wud P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int add_wud_scratch P_((int len));
unsigned int add_wud_inplace P_((void));
void sub_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int sub_wuz_scratch P_((int len));
unsigned int sub_wuz_inplace P_((void));
void sub_wud P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int sub_wud_scratch P_((int len));
unsigned int sub_wud_inplace P_((void));
void mul_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int mul_wuz_scratch P_((int len));
unsigned int mul_wuz_inplace P_((void));
void mul_wud P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int mul_wud_scratch P_((int len));
unsigned int mul_wud_inplace P_((void));
void div_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int div_wuz_scratch P_((int len));
unsigned int div_wuz_inplace P_((void));
void div_wud P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int div_wud_scratch P_((int len));
unsigned int div_wud_inplace P_((void));
void grt_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int grt_wuz_scratch P_((int len));
unsigned int grt_wuz_inplace P_((void));
void grt_wud P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int grt_wud_scratch P_((int len));
unsigned int grt_wud_inplace P_((void));
void les_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int les_wuz_scratch P_((int len));
unsigned int les_wuz_inplace P_((void));
void les_wud P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int les_wud_scratch P_((int len));
unsigned int les_wud_inplace P_((void));
void geq_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int geq_wuz_scratch P_((int len));
unsigned int geq_wuz_inplace P_((void));
void geq_wud P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int geq_wud_scratch P_((int len));
unsigned int geq_wud_inplace P_((void));
void leq_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int leq_wuz_scratch P_((int len));
unsigned int leq_wuz_inplace P_((void));
void leq_wud P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int leq_wud_scratch P_((int len));
unsigned int leq_wud_inplace P_((void));
void lsh_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int lsh_wuz_scratch P_((int len));
unsigned int lsh_wuz_inplace P_((void));
void rsh_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int rsh_wuz_scratch P_((int len));
unsigned int rsh_wuz_inplace P_((void));
void mod_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int mod_wuz_scratch P_((int len));
unsigned int mod_wuz_inplace P_((void));
void rnd_wuz P_((vec_p d, vec_p s, int len, vec_p scratch));
int rnd_wuz_scratch P_((int len));
unsigned int rnd_wuz_inplace P_((void));
void eql_wub P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int eql_wub_scratch P_((int len));
unsigned int eql_wub_inplace P_((void));
void eql_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int eql_wuz_scratch P_((int len));
unsigned int eql_wuz_inplace P_((void));
void eql_wud P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int eql_wud_scratch P_((int len));
unsigned int eql_wud_inplace P_((void));
void neq_wub P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int neq_wub_scratch P_((int len));
unsigned int neq_wub_inplace P_((void));
void neq_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int neq_wuz_scratch P_((int len));
unsigned int neq_wuz_inplace P_((void));
void neq_wud P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int neq_wud_scratch P_((int len));
unsigned int neq_wud_inplace P_((void));
void sel_wuz P_((vec_p d, vec_p s1, vec_p s2, vec_p s3, int len, vec_p scratch));
int sel_wuz_scratch P_((int len));
unsigned int sel_wuz_inplace P_((void));
void sel_wub P_((vec_p d, vec_p s1, vec_p s2, vec_p s3, int len, vec_p scratch));
int sel_wub_scratch P_((int len));
unsigned int sel_wub_inplace P_((void));
void sel_wud P_((vec_p d, vec_p s1, vec_p s2, vec_p s3, int len, vec_p scratch));
int sel_wud_scratch P_((int len));
unsigned int sel_wud_inplace P_((void));
void not_wub P_((vec_p d, vec_p s, int len, vec_p scratch));
int not_wub_scratch P_((int len));
unsigned int not_wub_inplace P_((void));
void xor_wub P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int xor_wub_scratch P_((int len));
unsigned int xor_wub_inplace P_((void));
void ior_wub P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int ior_wub_scratch P_((int len));
unsigned int ior_wub_inplace P_((void));
void and_wub P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int and_wub_scratch P_((int len));
unsigned int and_wub_inplace P_((void));
void ior_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int ior_wuz_scratch P_((int len));
unsigned int ior_wuz_inplace P_((void));
void and_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int and_wuz_scratch P_((int len));
unsigned int and_wuz_inplace P_((void));
void not_wuz P_((vec_p d, vec_p s, int len, vec_p scratch));
int not_wuz_scratch P_((int len));
unsigned int not_wuz_inplace P_((void));
void xor_wuz P_((vec_p d, vec_p s1, vec_p s2, int len, vec_p scratch));
int xor_wuz_scratch P_((int len));
unsigned int xor_wuz_inplace P_((void));
void int_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int int_wud_scratch P_((int len));
unsigned int int_wud_inplace P_((void));
void int_wub P_((vec_p d, vec_p s, int len, vec_p scratch));
int int_wub_scratch P_((int len));
unsigned int int_wub_inplace P_((void));
void dbl_wuz P_((vec_p d, vec_p s, int len, vec_p scratch));
int dbl_wuz_scratch P_((int len));
unsigned int dbl_wuz_inplace P_((void));
void boo_wuz P_((vec_p d, vec_p s, int len, vec_p scratch));
int boo_wuz_scratch P_((int len));
unsigned int boo_wuz_inplace P_((void));
void flr_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int flr_wud_scratch P_((int len));
unsigned int flr_wud_inplace P_((void));
void cei_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int cei_wud_scratch P_((int len));
unsigned int cei_wud_inplace P_((void));
void trn_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int trn_wud_scratch P_((int len));
unsigned int trn_wud_inplace P_((void));
void rou_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int rou_wud_scratch P_((int len));
unsigned int rou_wud_inplace P_((void));
void exp_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int exp_wud_scratch P_((int len));
unsigned int exp_wud_inplace P_((void));
void log_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int log_wud_scratch P_((int len));
unsigned int log_wud_inplace P_((void));
void sqt_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int sqt_wud_scratch P_((int len));
unsigned int sqt_wud_inplace P_((void));
void sin_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int sin_wud_scratch P_((int len));
unsigned int sin_wud_inplace P_((void));
void cos_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int cos_wud_scratch P_((int len));
unsigned int cos_wud_inplace P_((void));
void tan_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int tan_wud_scratch P_((int len));
unsigned int tan_wud_inplace P_((void));
void asn_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int asn_wud_scratch P_((int len));
unsigned int asn_wud_inplace P_((void));
void acs_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int acs_wud_scratch P_((int len));
unsigned int acs_wud_inplace P_((void));
void atn_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int atn_wud_scratch P_((int len));
unsigned int atn_wud_inplace P_((void));
void snh_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int snh_wud_scratch P_((int len));
unsigned int snh_wud_inplace P_((void));
void csh_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int csh_wud_scratch P_((int len));
unsigned int csh_wud_inplace P_((void));
void tnh_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int tnh_wud_scratch P_((int len));
unsigned int tnh_wud_inplace P_((void));
void cpy_wuz P_((vec_p d, vec_p s, int len, vec_p scratch));
int cpy_wuz_scratch P_((int len));
unsigned int cpy_wuz_inplace P_((void));
void cpy_wub P_((vec_p d, vec_p s, int len, vec_p scratch));
int cpy_wub_scratch P_((int len));
unsigned int cpy_wub_inplace P_((void));
void cpy_wud P_((vec_p d, vec_p s, int len, vec_p scratch));
int cpy_wud_scratch P_((int len));
unsigned int cpy_wud_inplace P_((void));
void cpy_wus P_((vec_p d, vec_p s, int n, int m, vec_p scratch));
int cpy_wus_scratch P_((int n, int m));
unsigned int cpy_wus_inplace P_((void));
void add_suz P_((vec_p d, vec_p s, int len, vec_p scratch));
int add_suz_scratch P_((int len));
unsigned int add_suz_inplace P_((void));
void add_sud P_((vec_p d, vec_p s, int len, vec_p scratch));
int add_sud_scratch P_((int len));
unsigned int add_sud_inplace P_((void));
void mul_suz P_((vec_p d, vec_p s, int len, vec_p scratch));
int mul_suz_scratch P_((int len));
unsigned int mul_suz_inplace P_((void));
void mul_sud P_((vec_p d, vec_p s, int len, vec_p scratch));
int mul_sud_scratch P_((int len));
unsigned int mul_sud_inplace P_((void));
void min_suz P_((vec_p d, vec_p s, int len, vec_p scratch));
int min_suz_scratch P_((int len));
unsigned int min_suz_inplace P_((void));
void min_sud P_((vec_p d, vec_p s, int len, vec_p scratch));
int min_sud_scratch P_((int len));
unsigned int min_sud_inplace P_((void));
void max_suz P_((vec_p d, vec_p s, int len, vec_p scratch));
int max_suz_scratch P_((int len));
unsigned int max_suz_inplace P_((void));
void max_sud P_((vec_p d, vec_p s, int len, vec_p scratch));
int max_sud_scratch P_((int len));
unsigned int max_sud_inplace P_((void));
void and_sub P_((vec_p d, vec_p s, int len, vec_p scratch));
int and_sub_scratch P_((int len));
unsigned int and_sub_inplace P_((void));
void and_suz P_((vec_p d, vec_p s, int len, vec_p scratch));
int and_suz_scratch P_((int len));
unsigned int and_suz_inplace P_((void));
void ior_sub P_((vec_p d, vec_p s, int len, vec_p scratch));
int ior_sub_scratch P_((int len));
unsigned int ior_sub_inplace P_((void));
void ior_suz P_((vec_p d, vec_p s, int len, vec_p scratch));
int ior_suz_scratch P_((int len));
unsigned int ior_suz_inplace P_((void));
void xor_sub P_((vec_p d, vec_p s, int len, vec_p scratch));
int xor_sub_scratch P_((int len));
unsigned int xor_sub_inplace P_((void));
void xor_suz P_((vec_p d, vec_p s, int len, vec_p scratch));
int xor_suz_scratch P_((int len));
unsigned int xor_suz_inplace P_((void));
void add_sez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int add_sez_scratch P_((int vec_len, int seg_len));
unsigned int add_sez_inplace P_((void));
void add_sed P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int add_sed_scratch P_((int vec_len, int seg_len));
unsigned int add_sed_inplace P_((void));
void mul_sez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int mul_sez_scratch P_((int vec_len, int seg_len));
unsigned int mul_sez_inplace P_((void));
void mul_sed P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int mul_sed_scratch P_((int vec_len, int seg_len));
unsigned int mul_sed_inplace P_((void));
void min_sez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int min_sez_scratch P_((int vec_len, int seg_len));
unsigned int min_sez_inplace P_((void));
void min_sed P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int min_sed_scratch P_((int vec_len, int seg_len));
unsigned int min_sed_inplace P_((void));
void max_sez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int max_sez_scratch P_((int vec_len, int seg_len));
unsigned int max_sez_inplace P_((void));
void max_sed P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int max_sed_scratch P_((int vec_len, int seg_len));
unsigned int max_sed_inplace P_((void));
void and_seb P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int and_seb_scratch P_((int vec_len, int seg_len));
unsigned int and_seb_inplace P_((void));
void and_sez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int and_sez_scratch P_((int vec_len, int seg_len));
unsigned int and_sez_inplace P_((void));
void ior_seb P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int ior_seb_scratch P_((int vec_len, int seg_len));
unsigned int ior_seb_inplace P_((void));
void ior_sez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int ior_sez_scratch P_((int vec_len, int seg_len));
unsigned int ior_sez_inplace P_((void));
void xor_seb P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int xor_seb_scratch P_((int vec_len, int seg_len));
unsigned int xor_seb_inplace P_((void));
void xor_sez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int xor_sez_scratch P_((int vec_len, int seg_len));
unsigned int xor_sez_inplace P_((void));
int add_ruz P_((vec_p s, int len, vec_p scratch));
int add_ruz_scratch P_((int len));
unsigned int add_ruz_inplace P_((void));
double add_rud P_((vec_p s, int len, vec_p scratch));
int add_rud_scratch P_((int len));
unsigned int add_rud_inplace P_((void));
int mul_ruz P_((vec_p s, int len, vec_p scratch));
int mul_ruz_scratch P_((int len));
unsigned int mul_ruz_inplace P_((void));
double mul_rud P_((vec_p s, int len, vec_p scratch));
int mul_rud_scratch P_((int len));
unsigned int mul_rud_inplace P_((void));
int min_ruz P_((vec_p s, int len, vec_p scratch));
int min_ruz_scratch P_((int len));
unsigned int min_ruz_inplace P_((void));
double min_rud P_((vec_p s, int len, vec_p scratch));
int min_rud_scratch P_((int len));
unsigned int min_rud_inplace P_((void));
int max_ruz P_((vec_p s, int len, vec_p scratch));
int max_ruz_scratch P_((int len));
unsigned int max_ruz_inplace P_((void));
double max_rud P_((vec_p s, int len, vec_p scratch));
int max_rud_scratch P_((int len));
unsigned int max_rud_inplace P_((void));
cvl_bool and_rub P_((vec_p s, int len, vec_p scratch));
int and_rub_scratch P_((int len));
unsigned int and_rub_inplace P_((void));
int and_ruz P_((vec_p s, int len, vec_p scratch));
int and_ruz_scratch P_((int len));
unsigned int and_ruz_inplace P_((void));
cvl_bool ior_rub P_((vec_p s, int len, vec_p scratch));
int ior_rub_scratch P_((int len));
unsigned int ior_rub_inplace P_((void));
int ior_ruz P_((vec_p s, int len, vec_p scratch));
int ior_ruz_scratch P_((int len));
unsigned int ior_ruz_inplace P_((void));
cvl_bool xor_rub P_((vec_p s, int len, vec_p scratch));
int xor_rub_scratch P_((int len));
unsigned int xor_rub_inplace P_((void));
int xor_ruz P_((vec_p s, int len, vec_p scratch));
int xor_ruz_scratch P_((int len));
unsigned int xor_ruz_inplace P_((void));
void add_rez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int add_rez_scratch P_((int vec_len, int seg_len));
unsigned int add_rez_inplace P_((void));
void add_red P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int add_red_scratch P_((int vec_len, int seg_len));
unsigned int add_red_inplace P_((void));
void mul_rez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int mul_rez_scratch P_((int vec_len, int seg_len));
unsigned int mul_rez_inplace P_((void));
void mul_red P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int mul_red_scratch P_((int vec_len, int seg_len));
unsigned int mul_red_inplace P_((void));
void min_rez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int min_rez_scratch P_((int vec_len, int seg_len));
unsigned int min_rez_inplace P_((void));
void min_red P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int min_red_scratch P_((int vec_len, int seg_len));
unsigned int min_red_inplace P_((void));
void max_rez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int max_rez_scratch P_((int vec_len, int seg_len));
unsigned int max_rez_inplace P_((void));
void max_red P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int max_red_scratch P_((int vec_len, int seg_len));
unsigned int max_red_inplace P_((void));
void and_reb P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int and_reb_scratch P_((int vec_len, int seg_len));
unsigned int and_reb_inplace P_((void));
void and_rez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int and_rez_scratch P_((int vec_len, int seg_len));
unsigned int and_rez_inplace P_((void));
void ior_reb P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int ior_reb_scratch P_((int vec_len, int seg_len));
unsigned int ior_reb_inplace P_((void));
void ior_rez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int ior_rez_scratch P_((int vec_len, int seg_len));
unsigned int ior_rez_inplace P_((void));
void xor_reb P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int xor_reb_scratch P_((int vec_len, int seg_len));
unsigned int xor_reb_inplace P_((void));
void xor_rez P_((vec_p d, vec_p s, vec_p sd, int n, int m, vec_p scratch));
int xor_rez_scratch P_((int vec_len, int seg_len));
unsigned int xor_rez_inplace P_((void));
int ext_vuz P_((vec_p V, int i, int len, vec_p scratch));
int ext_vuz_scratch P_((int len));
unsigned int ext_vuz_inplace P_((void));
cvl_bool ext_vub P_((vec_p V, int i, int len, vec_p scratch));
int ext_vub_scratch P_((int len));
unsigned int ext_vub_inplace P_((void));
double ext_vud P_((vec_p V, int i, int len, vec_p scratch));
int ext_vud_scratch P_((int len));
unsigned int ext_vud_inplace P_((void));
void ext_vez P_((vec_p d, vec_p s, vec_p i, vec_p sd, int n, int m, vec_p scratch));
int ext_vez_scratch P_((int vec_len, int seg_len));
unsigned int ext_vez_inplace P_((void));
void ext_veb P_((vec_p d, vec_p s, vec_p i, vec_p sd, int n, int m, vec_p scratch));
int ext_veb_scratch P_((int vec_len, int seg_len));
unsigned int ext_veb_inplace P_((void));
void ext_ved P_((vec_p d, vec_p s, vec_p i, vec_p sd, int n, int m, vec_p scratch));
int ext_ved_scratch P_((int vec_len, int seg_len));
unsigned int ext_ved_inplace P_((void));
void rep_vuz P_((vec_p V, int i, int val, int len, vec_p scratch));
int rep_vuz_scratch P_((int len));
unsigned int rep_vuz_inplace P_((void));
void rep_vub P_((vec_p V, int i, cvl_bool val, int len, vec_p scratch));
int rep_vub_scratch P_((int len));
unsigned int rep_vub_inplace P_((void));
void rep_vud P_((vec_p V, int i, double val, int len, vec_p scratch));
int rep_vud_scratch P_((int len));
unsigned int rep_vud_inplace P_((void));
void rep_vez P_((vec_p d, vec_p s, vec_p v, vec_p sd, int n, int m, vec_p scratch));
int rep_vez_scratch P_((int vec_len, int seg_len));
unsigned int rep_vez_inplace P_((void));
void rep_veb P_((vec_p d, vec_p s, vec_p v, vec_p sd, int n, int m, vec_p scratch));
int rep_veb_scratch P_((int vec_len, int seg_len));
unsigned int rep_veb_inplace P_((void));
void rep_ved P_((vec_p d, vec_p s, vec_p v, vec_p sd, int n, int m, vec_p scratch));
int rep_ved_scratch P_((int vec_len, int seg_len));
unsigned int rep_ved_inplace P_((void));
void dis_vuz P_((vec_p d, int v, int len, vec_p scratch));
int dis_vuz_scratch P_((int len));
unsigned int dis_vuz_inplace P_((void));
void dis_vub P_((vec_p d, cvl_bool v, int len, vec_p scratch));
int dis_vub_scratch P_((int len));
unsigned int dis_vub_inplace P_((void));
void dis_vud P_((vec_p d, double v, int len, vec_p scratch));
int dis_vud_scratch P_((int len));
unsigned int dis_vud_inplace P_((void));
void dis_vez P_((vec_p d, vec_p v, vec_p sd, int n, int m, vec_p scratch));
int dis_vez_scratch P_((int vec_len, int seg_len));
unsigned int dis_vez_inplace P_((void));
void dis_veb P_((vec_p d, vec_p v, vec_p sd, int n, int m, vec_p scratch));
int dis_veb_scratch P_((int vec_len, int seg_len));
unsigned int dis_veb_inplace P_((void));
void dis_ved P_((vec_p d, vec_p v, vec_p sd, int n, int m, vec_p scratch));
int dis_ved_scratch P_((int vec_len, int seg_len));
unsigned int dis_ved_inplace P_((void));
void smp_puz P_((vec_p d, vec_p s, vec_p i, int len, vec_p scratch));
int smp_puz_scratch P_((int len));
unsigned int smp_puz_inplace P_((void));
void smp_pub P_((vec_p d, vec_p s, vec_p i, int len, vec_p scratch));
int smp_pub_scratch P_((int len));
unsigned int smp_pub_inplace P_((void));
void smp_pud P_((vec_p d, vec_p s, vec_p i, int len, vec_p scratch));
int smp_pud_scratch P_((int len));
unsigned int smp_pud_inplace P_((void));
void smp_pez P_((vec_p d, vec_p s, vec_p i, vec_p sd, int n, int m, vec_p scratch));
int smp_pez_scratch P_((int vec_len, int seg_len));
unsigned int smp_pez_inplace P_((void));
void smp_peb P_((vec_p d, vec_p s, vec_p i, vec_p sd, int n, int m, vec_p scratch));
int smp_peb_scratch P_((int vec_len, int seg_len));
unsigned int smp_peb_inplace P_((void));
void smp_ped P_((vec_p d, vec_p s, vec_p i, vec_p sd, int n, int m, vec_p scratch));
int smp_ped_scratch P_((int vec_len, int seg_len));
unsigned int smp_ped_inplace P_((void));
void bck_puz P_((vec_p d, vec_p s, vec_p i, int s_len, int d_len, vec_p scratch));
int bck_puz_scratch P_((int s_len, int d_len));
unsigned int bck_puz_inplace P_((void));
void bck_pub P_((vec_p d, vec_p s, vec_p i, int s_len, int d_len, vec_p scratch));
int bck_pub_scratch P_((int s_len, int d_len));
unsigned int bck_pub_inplace P_((void));
void bck_pud P_((vec_p d, vec_p s, vec_p i, int s_len, int d_len, vec_p scratch));
int bck_pud_scratch P_((int s_len, int d_len));
unsigned int bck_pud_inplace P_((void));
void bck_pez P_((vec_p d, vec_p s, vec_p i, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
int bck_pez_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int bck_pez_inplace P_((void));
void bck_peb P_((vec_p d, vec_p s, vec_p i, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
int bck_peb_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int bck_peb_inplace P_((void));
void bck_ped P_((vec_p d, vec_p s, vec_p i, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
int bck_ped_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int bck_ped_inplace P_((void));
void fpm_puz P_((vec_p d, vec_p s, vec_p i, vec_p f, int len_src, int len_dest, vec_p scratch));
int fpm_puz_scratch P_((int s_len, int d_len));
unsigned int fpm_puz_inplace P_((void));
void fpm_pub P_((vec_p d, vec_p s, vec_p i, vec_p f, int len_src, int len_dest, vec_p scratch));
int fpm_pub_scratch P_((int s_len, int d_len));
unsigned int fpm_pub_inplace P_((void));
void fpm_pud P_((vec_p d, vec_p s, vec_p i, vec_p f, int len_src, int len_dest, vec_p scratch));
int fpm_pud_scratch P_((int s_len, int d_len));
unsigned int fpm_pud_inplace P_((void));
void fpm_pez P_((vec_p d, vec_p s, vec_p i, vec_p f, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
int fpm_pez_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int fpm_pez_inplace P_((void));
void fpm_peb P_((vec_p d, vec_p s, vec_p i, vec_p f, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
int fpm_peb_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int fpm_peb_inplace P_((void));
void fpm_ped P_((vec_p d, vec_p s, vec_p i, vec_p f, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
int fpm_ped_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int fpm_ped_inplace P_((void));
void bfp_puz P_((vec_p d, vec_p s, vec_p i, vec_p f, int len_src, int len_dest, vec_p scratch));
int bfp_puz_scratch P_((int s_len, int d_len));
unsigned int bfp_puz_inplace P_((void));
void bfp_pub P_((vec_p d, vec_p s, vec_p i, vec_p f, int len_src, int len_dest, vec_p scratch));
int bfp_pub_scratch P_((int s_len, int d_len));
unsigned int bfp_pub_inplace P_((void));
void bfp_pud P_((vec_p d, vec_p s, vec_p i, vec_p f, int len_src, int len_dest, vec_p scratch));
int bfp_pud_scratch P_((int s_len, int d_len));
unsigned int bfp_pud_inplace P_((void));
void bfp_pez P_((vec_p d, vec_p s, vec_p i, vec_p f, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
int bfp_pez_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int bfp_pez_inplace P_((void));
void bfp_peb P_((vec_p d, vec_p s, vec_p i, vec_p f, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
int bfp_peb_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int bfp_peb_inplace P_((void));
void bfp_ped P_((vec_p d, vec_p s, vec_p i, vec_p f, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
int bfp_ped_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int bfp_ped_inplace P_((void));
void dpe_puz P_((vec_p d, vec_p s, vec_p i, vec_p v, int len_src, int len_dest, vec_p scratch));
unsigned int dpe_puz_inplace P_((void));
int dpe_puz_scratch P_((int s_len, int d_len));
void dpe_pub P_((vec_p d, vec_p s, vec_p i, vec_p v, int len_src, int len_dest, vec_p scratch));
unsigned int dpe_pub_inplace P_((void));
int dpe_pub_scratch P_((int s_len, int d_len));
void dpe_pud P_((vec_p d, vec_p s, vec_p i, vec_p v, int len_src, int len_dest, vec_p scratch));
unsigned int dpe_pud_inplace P_((void));
int dpe_pud_scratch P_((int s_len, int d_len));
void dpe_pez P_((vec_p d, vec_p s, vec_p i, vec_p v, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
void dpe_peb P_((vec_p d, vec_p s, vec_p i, vec_p v, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
void dpe_ped P_((vec_p d, vec_p s, vec_p i, vec_p v, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
unsigned int dpe_pez_inplace P_((void));
int dpe_pez_scratch P_((int n_in, int m_in, int n_out, int m_out));
unsigned int dpe_peb_inplace P_((void));
int dpe_peb_scratch P_((int n_in, int m_in, int n_out, int m_out));
unsigned int dpe_ped_inplace P_((void));
int dpe_ped_scratch P_((int n_in, int m_in, int n_out, int m_out));
void dfp_puz P_((vec_p d, vec_p s, vec_p i, vec_p f, vec_p v, int len_src, int len_dest, vec_p scratch));
void dfp_pub P_((vec_p d, vec_p s, vec_p i, vec_p f, vec_p v, int len_src, int len_dest, vec_p scratch));
void dfp_pud P_((vec_p d, vec_p s, vec_p i, vec_p f, vec_p v, int len_src, int len_dest, vec_p scratch));
unsigned int dfp_puz_inplace P_((void));
int dfp_puz_scratch P_((int s_len, int d_len));
unsigned int dfp_pub_inplace P_((void));
int dfp_pub_scratch P_((int s_len, int d_len));
unsigned int dfp_pud_inplace P_((void));
int dfp_pud_scratch P_((int s_len, int d_len));
void dfp_pez P_((vec_p d, vec_p s, vec_p i, vec_p f, vec_p v, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
void dfp_peb P_((vec_p d, vec_p s, vec_p i, vec_p f, vec_p v, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
void dfp_ped P_((vec_p d, vec_p s, vec_p i, vec_p f, vec_p v, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
unsigned int dfp_pez_inplace P_((void));
int dfp_pez_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int dfp_peb_inplace P_((void));
int dfp_peb_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int dfp_ped_inplace P_((void));
int dfp_ped_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
void tgt_fos P_((cvl_timer_t *cvl_timer_p));
double tdf_fos P_((cvl_timer_t *t1, cvl_timer_t *t2));
int siz_fob P_((int length));
int siz_foz P_((int length));
int siz_fod P_((int length));
int siz_fos P_((int n, int m));
void mke_fov P_((vec_p sd, vec_p l, int n, int m, vec_p scratch));
unsigned int mke_fov_inplace P_((void));
int mke_fov_scratch P_((int n, int m));
void len_fos P_((vec_p l, vec_p sd, int n, int m, vec_p scratch));
int len_fos_scratch P_((int n, int m));
unsigned int len_fos_inplace P_((void));
vec_p alo_foz P_((int size));
void fre_fov P_((vec_p pointer));
void mov_fov P_((vec_p d, vec_p s, int size, vec_p scratch));
int mov_fov_scratch P_((int size));
vec_p add_fov P_((vec_p v, int a));
int sub_fov P_((vec_p v1, vec_p v2));
cvl_bool eql_fov P_((vec_p v1, vec_p v2));
int cmp_fov P_((vec_p v1, vec_p v2));
void v2c_fuz P_((int *d, vec_p s, int len, vec_p scratch));
int v2c_fuz_scratch P_((int len));
unsigned int v2c_fuz_inplace P_((void));
void v2c_fub P_((cvl_bool *d, vec_p s, int len, vec_p scratch));
int v2c_fub_scratch P_((int len));
unsigned int v2c_fub_inplace P_((void));
void v2c_fud P_((double *d, vec_p s, int len, vec_p scratch));
int v2c_fud_scratch P_((int len));
unsigned int v2c_fud_inplace P_((void));
void c2v_fuz P_((vec_p d, int *s, int len, vec_p scratch));
int c2v_fuz_scratch P_((int len));
unsigned int c2v_fuz_inplace P_((void));
void c2v_fub P_((vec_p d, cvl_bool *s, int len, vec_p scratch));
int c2v_fub_scratch P_((int len));
unsigned int c2v_fub_inplace P_((void));
void c2v_fud P_((vec_p d, double *s, int len, vec_p scratch));
int c2v_fud_scratch P_((int len));
unsigned int c2v_fud_inplace P_((void));
void ind_luz P_((vec_p d, int init, int stride, int count, vec_p scratch));
int ind_luz_scratch P_((int len));
unsigned int ind_luz_inplace P_((void));
void ind_lez P_((vec_p d, vec_p init, vec_p stride, vec_p dest_segd, int vec_len, int seg_count, vec_p scratch));
int ind_lez_scratch P_((int vec_len, int seg_len));
unsigned int ind_lez_inplace P_((void));
int pk1_luv P_((vec_p f, int len, vec_p scratch));
int pk1_luv_scratch P_((int len));
unsigned int pk1_luv_inplace P_((void));
void pk2_luz P_((vec_p d, vec_p s, vec_p f, int src_len, int dest_len, vec_p scratch));
int pk2_luz_scratch P_((int len));
unsigned int pk2_luz_inplace P_((void));
void pk2_lub P_((vec_p d, vec_p s, vec_p f, int src_len, int dest_len, vec_p scratch));
int pk2_lub_scratch P_((int len));
unsigned int pk2_lub_inplace P_((void));
void pk2_lud P_((vec_p d, vec_p s, vec_p f, int src_len, int dest_len, vec_p scratch));
int pk2_lud_scratch P_((int len));
unsigned int pk2_lud_inplace P_((void));
void pk1_lev P_((vec_p ds, vec_p f, vec_p sd, int n, int m, vec_p scratch));
int pk1_lev_scratch P_((int vec_len, int seg_len));
unsigned int pk1_lev_inplace P_((void));
void pk2_lez P_((vec_p d, vec_p s, vec_p f, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
int pk2_lez_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int pk2_lez_inplace P_((void));
void pk2_leb P_((vec_p d, vec_p s, vec_p f, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
int pk2_leb_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int pk2_leb_inplace P_((void));
void pk2_led P_((vec_p d, vec_p s, vec_p f, vec_p sd_s, int n_s, int m_s, vec_p sd_d, int n_d, int m_d, vec_p scratch));
int pk2_led_scratch P_((int s_vec_len, int s_seg_len, int d_vec_len, int d_seg_len));
unsigned int pk2_led_inplace P_((void));
void rku_luz P_((vec_p d, vec_p s, int len, vec_p scratch));
int rku_luz_scratch P_((int len));
unsigned int rku_luz_inplace P_((void));
void rkd_luz P_((vec_p d, vec_p s, int len, vec_p scratch));
int rkd_luz_scratch P_((int len));
unsigned int rkd_luz_inplace P_((void));
void rku_lez P_((vec_p d, vec_p s, vec_p segd, int vec_len, int seg_count, vec_p scratch));
int rku_lez_scratch P_((int vec_len, int seg_count));
unsigned int rku_lez_inplace P_((void));
void rkd_lez P_((vec_p d, vec_p s, vec_p segd, int vec_len, int seg_count, vec_p scratch));
int rkd_lez_scratch P_((int vec_len, int seg_count));
unsigned int rkd_lez_inplace P_((void));
void rku_lud P_((vec_p d, vec_p s, int len, vec_p scratch));
int rku_lud_scratch P_((int len));
unsigned int rku_lud_inplace P_((void));
void rkd_lud P_((vec_p d, vec_p s, int len, vec_p scratch));
int rkd_lud_scratch P_((int len));
unsigned int rkd_lud_inplace P_((void));
void rku_led P_((vec_p d, vec_p s, vec_p segd, int vec_len, int seg_count, vec_p scratch));
int rku_led_scratch P_((int vec_len, int seg_count));
unsigned int rku_led_inplace P_((void));
void rkd_led P_((vec_p d, vec_p s, vec_p segd, int vec_len, int seg_count, vec_p scratch));
int rkd_led_scratch P_((int vec_len, int seg_count));
unsigned int rkd_led_inplace P_((void));
void rnd_foz P_((int seed));
#if __cplusplus | c_plusplus
}
#endif

#undef P_
#endif /* cvl_h */
