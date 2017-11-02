/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

#ifndef _PROGRAM_H
#define _PROGRAM_H 1

#include "vcode.h"

/* data structures for representation of vcode program */
typedef struct prog_entry {
    VOPCODE	vop;		/* opcode of command */
    vopdes_t	*vopdes;	/* descriptor for command (vcode_table.c) */
    int 	lineno;		/* line number in text */
    TYPE	type;		/* type of vop argument */
    int	is_in_const_pool;       /* is the value in the pool */
    union {			/* misc stuff */
	int 	branch;		/* prog_entry number to branch to */
	int	int_const;	/* value of constants */
	double	float_const;
	int	bool_const;	
	int	const_pool_index; /* location of vector constant in pool */
	struct {		/* args to copy and pop */
	    int arg1, arg2;    
	} stack_args;		
    } misc;
} prog_entry_t;

extern prog_entry_t  *program;	/* the VCODE program */
extern int next_prog_num;	/* place for next prog_entry */

extern void show_program PROTO_((void));
extern void show_prog_entry PROTO_((prog_entry_t*, int));
extern char *type_string PROTO_((TYPE));

#define UNKNOWN_BRANCH -1	/* place holder before linking */

#endif /* _PROGRAM_H */
