/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

#ifndef _STACK_H
#define _STACK_H 1
#include "vstack.h"
#include "program.h"

/* a stack_entry is an index into the stack */
typedef int stack_entry_t;		

extern vb_t **stack;			/* the stack */
extern int stack_index;			/* index of TOS */

#define TOS 	(stack_index)		/* top of stack */

#define se_vb(_se) (stack[_se])

#define stack_size (stack_index + 1)	/* number of elements in stack */

extern void stack_init PROTO_((void));
extern void stack_pop PROTO_((stack_entry_t));
extern void stack_push PROTO_((int, TYPE, int));
extern void do_pair PROTO_((prog_entry_t *));
extern void do_unpair PROTO_((prog_entry_t *));
extern int  do_cond PROTO_((prog_entry_t *));
extern void do_const PROTO_((prog_entry_t *));
extern void do_copy PROTO_((prog_entry_t *));
extern void do_pop PROTO_((prog_entry_t *));
extern void do_cpop PROTO_((prog_entry_t *));
extern void get_args PROTO_((prog_entry_t *, stack_entry_t[]));
extern void show_stack PROTO_((void));
extern void show_stack_values PROTO_((FILE*));
extern void do_quick_copy PROTO_((stack_entry_t));

/* Use this as the 3rd argument to stack_push, if the vector is not
 * a segment descriptor
 */
#define NOTSEGD -1

#endif /* _STACK_H */
