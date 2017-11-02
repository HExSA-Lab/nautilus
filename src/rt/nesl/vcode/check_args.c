/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

/* These functions deal with the arguments to VCODE routines: checking
 * for consistancy, memory usage and popping off arguments after a call.
 */

#include "config.h"
#include "vcode.h"
#include "y.tab.h"
#include "vstack.h"
#include "stack.h"
#include "program.h"
#include "check_args.h"

/* Use this to quickly map from the bitmap in AGREE_field to 
 * the index of the arg that must match.
 */
static int BOOL_VALUE[17] = {0, 1, 2, 0, 3, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 5};
#define AGREE_value(code) BOOL_VALUE[AGREE_field(code)];
#define COMPAT_value(code) BOOL_VALUE[(COMPAT_field(code))>>5]

/* arg_ok: 
 *     pe: the instruction being executed
 *     se_array: the arguments to the instruction
 * This function checks to see whether the arguments are of the 
 * correct length, type, and conformance for the instruction.  
 * Returns a boolean indicating success or failure.
 */
int args_ok(pe, se_array)
prog_entry_t *pe;
stack_entry_t se_array[];
{
    vopdes_t *vopdes = pe->vopdes;
    int i, j = 0;
    TYPE type = Illegal;
    int result = 1;

    /* compare arg constraints to contents of stack */
    for (i = 0; i < vopdes->arg_num; i++) {	/* loop over args */
	/* type agreement */
	switch (vopdes->arg_type[i]) {	
	    case Bool: 
	    case Int: 
	    case Float:
	    case Segdes:
		type = vopdes->arg_type[i];
		break;
	    case Given: 
		type = pe->type;
		break;
	    default:
		_fprintf(stderr, "vinterp: internal error: Illegal arg_type field for %s\n",
			vopdes->vopname);
		vinterp_exit (1);
	}
	if (se_vb(se_array[i])->type != type) {
	    _fprintf(stderr, "vinterp: line %d: type mismatch.  Stack[%d] is %s, function %s expects %s\n", 
			    pe->lineno,
			    i,
			    type_string(se_vb(se_array[i])->type),
			    vopdes->vopname,
			    type_string(type));
	    result = 0;
	}

	/* length agreement */
	switch (AGREE_field(vopdes->arg_len[i])) {
	    case NONE:   j = 0; break;		/* j is arg to check against */
	    case AGREE1: j = 1; break;
	    case AGREE2: j = 2; break;
	    case AGREE3: j = 3; break;
	    case AGREE4: j = 4; break;
	    case AGREE5: j = 5; break;
	    default: 
		_fprintf(stderr, "vinterp: internal error: illegal Agree field for %s\n",
			vopdes->vopname);
		vinterp_exit (1);
	}
	if (j != 0) {
	    if (se_vb(se_array[j-1])->len != se_vb(se_array[i])->len) {
		_fprintf(stderr, "vinterp: line %d: length mismatch between args %d(%d) and %d(%d) in %s.\n",
				pe->lineno, 
				j-1, se_vb(se_array[j-1])->len, 
				i, se_vb(se_array[i])->len, 
				vopdes->vopname);
		result = 0;
	    }
	}

	/* compatibility check: must refer to a segdes arg */
	switch (COMPAT_field(vopdes->arg_len[i])) {
	    case NONE: j = 0; break;
	    case COMPAT1: j = 1; break;
	    case COMPAT2: j = 2; break;
	    case COMPAT3: j = 3; break;
	    case COMPAT4: j = 4; break;
	    case COMPAT5: j = 5; break;
	    default:
		_fprintf(stderr, "vinterp: internal error: illegal Compat field for %s\n",
			vopdes->vopname);
	}
	if (j != 0) {
	    if (se_vb(se_array[i])->type == Segdes) {
		/* Check to see whether vector len is correct for the segdes */
		if (se_vb(se_array[j-1])->len != se_vb(se_array[i])->seg_len) {
		    _fprintf(stderr, "vinterp: line %d: Args %d and %d to %s are incompatible.\n", 
			    pe->lineno, j-1, i, vopdes->vopname);
		    result = 0;
		}
	    } else if (result != 0) {
		/* error could be a type error, and not internal */
		_fprintf(stderr, "vinterp: internal error: non-null compat field for non-segment descriptor:\n\t command: %s, arg: %d\n", vopdes->vopname, i);
		vinterp_exit (1);
	    }
	}
    }
    return result;
}

/* Allocate space for the result of instruction takes the instruction,
 * the character version of the instruction, and the arguments of the
 * instruction.  Returns a pointer to the allocated vector storage.
 *
 * Using the INPLACE CVL functions: find the result of the correct _inplace
 * CVL call.  For each destination, check to see if there is an argument
 * that can be overwritten (according to the inplace info), has no other
 * references (count == 1, pair_count == 0), and is of the correct length
 * and type.  If so, copy that arg onto the top of the stack, and make that
 * the destination.
 *
 * NB: This will not work if the result is a Segdes!!
 */
void allocate_result(pe, stack_args, dest_array)
prog_entry_t *pe;
stack_entry_t stack_args[];
vb_t *dest_array[];
{
    vopdes_t *vopdes = pe->vopdes;
    int num_dests = vopdes->result_num;
    int i;
    unsigned inplace_val;		/* result of cvl inplace funct */
    unsigned (*inplace_funct)();
    
    inplace_funct = inplace_cvl_funct(vopdes->vop, pe->type);
    if (inplace_funct == NULL) {
	_fprintf(stderr, "vinterp: internal error: inplace function for %s %s is NULL\n", vopdes->vopname, type_string(pe->type));
	vinterp_exit(1);
    }
    inplace_val = inplace_funct();

    for (i = 0; i < num_dests; i++) {	/* loop over results */
        int which_len;			/* which arg len agrees with */
        int res_len = 0;		/* length of result */
	unsigned res_field = vopdes->result_len[i];
        TYPE res_type = Illegal;	/* type of result */
	int j;

	/* Find type of result */
	switch (vopdes->result_type[i]) {
	    case Bool: 
	    case Int:
	    case Float:
		res_type = vopdes->result_type[i];
		break;
	    case Agree1: res_type = se_vb(stack_args[0])->type; break;
	    case Agree2: res_type = se_vb(stack_args[1])->type; break;
	    case Agree3: res_type = se_vb(stack_args[2])->type; break;
	    case Agree4: res_type = se_vb(stack_args[3])->type; break;
	    case Segdes:
		_fprintf(stderr, "vinterp: line %d: internal error: allocate_result can't allocate Segdes.\n instruction = %s\n", pe->lineno, vopdes->vopname);
		vinterp_exit(1);
		break;
	    default:
		_fprintf(stderr, "vinterp: line %d: internal error: Bad result_type value for %s\n",
				pe->lineno, vopdes->vopname);
		vinterp_exit (1);
		break;
	}

	/* Find len of result vector in terms of input */
	which_len = AGREE_value(res_field);
	if (which_len != 0) {
	    res_len = se_vb(stack_args[which_len - 1])->len;
	} else if (0 != (which_len = COMPAT_value(res_field))) {
	    res_len = se_vb(stack_args[which_len - 1])->seg_len;
	} else if (res_field == IS_SCALAR) {
	    res_len = 1;
	} else {
		_fprintf(stderr, "vinterp: internal error: bad result_size field for %s.\n",
				vopdes->vopname);
		vinterp_exit(1);
	}

#if 0
	/* The above code replaces this, which generated poor code
	 * on the Cray.  (Switch compiled to multiple ifs)
	 */
	switch (vopdes->result_len[i]) {
	    default:
		_fprintf(stderr, "vinterp: internal error: bad result_size field for %s.\n",
				vopdes->vopname);
		vinterp_exit(1);
		break;
	    case AGREE1: res_len = se_vb(stack_args[0])->len; break;
	    case AGREE2: res_len = se_vb(stack_args[1])->len; break;
	    case AGREE3: res_len = se_vb(stack_args[2])->len; break;
	    case AGREE4: res_len = se_vb(stack_args[3])->len; break;
	    case AGREE5: res_len = se_vb(stack_args[4])->len; break;
	    case COMPAT1: res_len = se_vb(stack_args[0])->seg_len; break;
	    case COMPAT2: res_len = se_vb(stack_args[1])->seg_len; break;
	    case COMPAT3: res_len = se_vb(stack_args[2])->seg_len; break;
	    case COMPAT4: res_len = se_vb(stack_args[3])->seg_len; break;
	    case COMPAT5: res_len = se_vb(stack_args[4])->seg_len; break;
	    case IS_SCALAR: res_len = 1; break;
	}
#endif

	assert(res_type != Segdes);

	j = 1;
	while (inplace_val != 0) {
	    assert (j <= vopdes->arg_num);
	    if (inplace_val & 1) {		/* arg can be overwritten */
		vb_t *arg_vb = se_vb(stack_args[j-1]);

		if ((arg_vb->count == 1) &&		/* no other refs */
		    (arg_vb->pair_count == 0) &&
		    (arg_vb->type == res_type) &&	/* right size */
		    (arg_vb->len == res_len)) {
#ifdef INPLACE_DEBUG
		    _fprintf(stderr, "overwriting arg %d\n", j);
#endif
		    do_quick_copy(stack_args[j-1]);
		    goto ALLOCATED;
		}
	    }
	    inplace_val >>=1;
	    j++;
	}

	assert (inplace_val == 0);

        /* allocate space on stack for result */
	stack_push(res_len, res_type, -1);
ALLOCATED:
	dest_array[i] = se_vb(TOS);		/* the new node is on TOS */
    }
}


/* This routine checks to see that there is enough scratch space in the
 * heap for the vector operation.  The scratch_cvl_funct returns a
 * pointer to the cvl scratch function for the vcode instruction.
 * That function is called, and the result is passed to assert_mem_size,
 * which will abort if enough memory cannot be scavenged.
 *
 * This routine assumes the cvl scratch functions take at most 2 argments.
 */
void assert_scratch(pe, stack_args)
prog_entry_t *pe;
stack_entry_t stack_args[];
{
    int scratch_needed;
    int (*scratch_fun)() = (int (*)())scratch_cvl_funct(pe->vop, pe->type);

    if (scratch_fun == NULL) {
	_fprintf(stderr, "vinterp: line %d: internal error: scratch function not in cvl_fun_list: %s, type %s .\n",
			pe->lineno, pe->vopdes->vopname, type_string(pe->type));
	vinterp_exit (1);
    } else {
	/* The arg_for_scratch field of the vopdes gives the list of arguments
	 * to be used for the scratch function.  0 indicates no more args.
	 * Values in arg_for_scratch are 1 based, so need to correct before
	 * we index into stack_args.
	 */
	int *arg_for_scratch = pe->vopdes->arg_for_scratch;
	vb_t *vb1, *vb2;

	assert(arg_for_scratch[0] != -1);	/* should never get here */
	assert(arg_for_scratch[0] != 0);	/* must have a scratch */
	assert(arg_for_scratch[2] == 0);	/* only 1 or 2, for now */

	vb1 = se_vb(stack_args[arg_for_scratch[0] - 1]);
	if (arg_for_scratch[1] == 0) {
	    /* just one */
	    if (vb1->type != Segdes) { 
		scratch_needed = (*scratch_fun)(vb1->len);
	    } else {
		assert(vb1->seg_len != -1);
		scratch_needed = (*scratch_fun)(vb1->seg_len, vb1->len);
	    }
	} else {
	    /* two */
	    vb2 = se_vb(stack_args[arg_for_scratch[1] - 1]);
	    if (vb1->type != Segdes) { 
		scratch_needed = (*scratch_fun)(vb1->len, vb2->len);
	    } else {
		assert(vb1->seg_len != -1);
		assert(vb2->seg_len != -1);
		scratch_needed = (*scratch_fun)(vb1->seg_len, vb1->len,
	    					vb2->seg_len, vb2->len);
	    }
	}
	assert_mem_size(scratch_needed);
    }
}

/* Pop off the args of the stack after function call */
void pop_args(args, numargs)
stack_entry_t args[];
int numargs;
{
    for (; numargs > 0; numargs--) {
	stack_pop(args[numargs - 1]);
    }
}
