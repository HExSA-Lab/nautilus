/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

/*
 * Definitions of vcode primitives and such like.
 */

#ifndef	_VCODE
#define	_VCODE 1

typedef enum type 
    {Illegal=0, Pair, None, Bool, Int, Float, Segdes, String, Agree1, Agree2, Agree3, Agree4, Given}
    TYPE;

#define	MAX_ARGS	6
#define MAX_OUT		3

#define FN_NAME_MAX	50      /* maximum length of an identifier */

/* These are used in stack.c and refer to file_list */
#define NULL_STREAM_FD -1
#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2

/* global variables, mostly flags */
extern int lex_trace;
extern int stack_trace;
extern int runtime_trace;
extern int program_dump;
extern int link_trace;
extern int garbage_notify;
extern int check_args;

/* Argument constraints codes. */
#define	NONE		0x0000	/* no constraints */
#define AGREE1		0x0001	/* length of this arg == length of arg 1 */
#define AGREE2		0x0002	/* length of this arg == length of arg 2 */
#define AGREE3		0x0004	/* length of this arg == length of arg 3 */
#define AGREE4		0x0008	/* length of this arg == length of arg 4 */
#define AGREE5		0x0010	/* length of this arg == length of arg 5 */
#define	COMPAT1		0x0020	/* this arg is valid segdes for arg 1 */
#define	COMPAT2		0x0040	/* this arg is valid segdes for arg 2 */
#define	COMPAT3		0x0080	/* this arg is valid segdes for arg 3 */
#define	COMPAT4		0x0100	/* this arg is valid segdes for arg 4 */
#define	COMPAT5		0x0200	/* this arg is valid segdes for arg 5 */
#define IS_SCALAR	0x0400	/* this argument is a scalar */

#define AGREE_field(code)  ((code) & 0x001F)
#define COMPAT_field(code) ((code) & 0x03E0)

/* 
 * Vopcodes (none may be <= 0).  These are defined by yacc in y.tab.h
 */
typedef int VOPCODE;
typedef enum CVL_DESC_T {
	Elwise1, Elwise2, Elwise3, Scan, Reduce,  Special, SegOp,
	Permute, Dpermute, Fpermute, Bpermute, Bfpermute, Dfpermute,
	Extract, Replace, Dist, Index, Pack, NotSupported 
} cvl_desc_t;

typedef struct {
  VOPCODE	vop;			/* opcode */
  char	       *vopname;		/* string giving its name */
  int		arg_num;		/* number of arguments */
  int 		result_num;		/* number of results */
  TYPE		arg_type[MAX_ARGS];	/* input argument types */
  unsigned	arg_len[MAX_ARGS];	/* bit mask for constraint checking */
  TYPE		result_type[MAX_OUT];	/* in terms of input types */
  unsigned	result_len[MAX_OUT];	/* in terms of input sizes */
  int		arg_for_scratch[MAX_ARGS];/* arg to use to check scratch */
  cvl_desc_t	cvl_desc;		/* info about cvl call */
} vopdes_t;

extern vopdes_t vopdes_table[];
extern int ok_vcode_table PROTO_((void));

typedef struct {
    void (*function)();
    int (*scratch_function)();
    unsigned (*inplace_function)();
} cvl_funct_t;

typedef struct {
    cvl_funct_t cvl_int;
    cvl_funct_t cvl_float;
    cvl_funct_t cvl_boolean;
} cvl_triple_t;

extern cvl_triple_t cvl_fun_list[];

/* this should be the token for PLUS */
#define vop_min PLUS
#define vop_table_look(vop) (vopdes_table + ( (vop) - vop_min))

#define cvl_funct(vop, type)					\
    (  type == Int   ? cvl_fun_list[vop- vop_min].cvl_int.function \
     : type == Float ? cvl_fun_list[vop- vop_min].cvl_float.function \
     :                 cvl_fun_list[vop- vop_min].cvl_boolean.function )
		  
#define scratch_cvl_funct(vop, type)				\
    (  type == Int   ? cvl_fun_list[vop- vop_min].cvl_int.scratch_function \
     : type == Float ? cvl_fun_list[vop- vop_min].cvl_float.scratch_function \
     :                 cvl_fun_list[vop- vop_min].cvl_boolean.scratch_function )

#define inplace_cvl_funct(vop, type)				\
    (  type == Int   ? cvl_fun_list[vop- vop_min].cvl_int.inplace_function \
     : type == Float ? cvl_fun_list[vop- vop_min].cvl_float.inplace_function \
     :                 cvl_fun_list[vop- vop_min].cvl_boolean.inplace_function )

/* assert macros stolen from assert.h.  They did it wrong (not hygenic). */
# ifdef ASSERTS
# define _assert(ex)	{if (!(ex)){_fprintf(stderr,"vinterp: assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);vinterp_exit(1);}}
# define assert(ex)	do { _assert(ex) } while(0)
# else
# define _assert(ex)
# define assert(ex)
# endif

extern void vinterp_exit PROTO_((int));

#endif
