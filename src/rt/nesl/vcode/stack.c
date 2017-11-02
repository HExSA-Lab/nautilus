/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

/* Module for dealing with stack:
 * 1. the stack represents the actual runtime behavior of the VCODE program.
 * 2. pointers from stack entries indicate the entry in the vblock list 
 *    that is responsible for this vector.  vectors are NOT duplicated 
 *    by copy and push operations, only new pointers are created. see
 *    vstack.c for more details.
 *
 * POP and COPY operations (implicit or explicit) are carried on in the stack
 * and in the vstack.  It is the responsibility of the vstack to manage
 * vector memory.
 */

#include "config.h"
#include "vcode.h"
#include "vstack.h"
#include "program.h"
#include "constant.h"
#include "stack.h"
#include "io.h"
#include "y.tab.h"

/* The stack is an array of pointers to vb's. 
 * A stack_entry_t is just an int: an index into the stack array.
 * stack[stack_index] is the element on the top of the stack.
 */
vb_t **stack;
int stack_index;

/* ------------------------ stack_entry allocators -----------------*/

#define STACK_MAX_INITIAL 5	/* initial size of stack */
int stack_max;			/* number of stack_entry's allocated */

/* If there is room on stack, return pointer to next element,
 * otherwise allocate more space.
 * Define this as a macro to optimize the standard case.
 */
#define new_se()					\
    (++stack_index < stack_max ? stack_index : new_se_real())

static stack_entry_t new_se_real PROTO_((void))
{
    stack_max *= 2;			/* double size */
    stack = (vb_t **) 
	    realloc(stack, stack_max * sizeof(vb_t*));
    if (stack == NULL) {
	_fprintf(stderr, "vinterp: error allocating internal structures (stack_entry).\n");
	vinterp_exit (1);
    }
    return stack_index;
}
    
/* Decriment stack_size */
#define free_se(_se) 					\
    do {						\
	stack_index--;					\
    } while (0)

/* Initialize the stack */
static void se_alloc_init PROTO_((void))
{
    stack = (vb_t **)
	    malloc (STACK_MAX_INITIAL * sizeof (vb_t*));
    if (stack == NULL) {
        _fprintf(stderr, "vinterp: error allocating stack (se_alloc_init).\n");
        vinterp_exit (1);
    }
    stack_index = -1;
    stack_max = STACK_MAX_INITIAL;
}

/* ---------------------------stack manipulation code-------------------*/
void stack_init()
{
    se_alloc_init();

}
    
/* Pop an item from the stack: first, pop out vstack entry, then 
 * remove se from stack.
 */
void stack_pop(se)
stack_entry_t se;
{
    stack_entry_t to_se;		/* we push up elements under pop */

    assert ((se <= stack_index) && (se >= 0));

    vstack_pop(stack[se]);
    /* copy each element into the previous one */
#if cray
#pragma ivdep
#endif
    for (to_se = se; to_se < stack_index; to_se ++)
	stack[to_se] = stack[to_se + 1];
    
    assert(to_se == stack_index);		/* copy up to TOS */

    free_se(to_se);				/* free item at end */
}

/* Push a new vector on the stack, given length and type, and seglen */
void stack_push(len, type, seglen)
int len;
TYPE type;
int seglen;
{
    stack_entry_t se = new_se();
    vb_t *new_vb;

    new_vb = new_vector(len, type, seglen);
    stack[se] = new_vb;
}

/* Handle VCODE conditionals: grab bool scalar from top of stack and return
 * its truth value */
int do_cond(pe)
prog_entry_t *pe;
{
    vb_t *flag_vb = se_vb(TOS);
    int result;

    /* check type and length */
    if (check_args && (flag_vb->type != Bool || flag_vb->len != 1)) {
	_fprintf(stderr, "vinterp: line %d: IF: top of stack not bool scalar.\n", pe->lineno);
	vinterp_exit (1);
    }

    /* get value from vector */
    assert_mem_size(ext_vub_scratch(1));
    result = (int) ext_vub(flag_vb->vector, 0, 1, SCRATCH);

    /* remove flag from stack */
    stack_pop(TOS);
    return result;
}

/* scalar constants get put on the stack using replace.
 * vector constants are already in vector mem, so just create stack entry
 *   for it.
 */
void do_const(pe)
prog_entry_t *pe;
{
    vec_p dest;

    if (pe->is_in_const_pool) {		/* vector constant is in pool */
	stack_entry_t se = new_se();
	int index = pe->misc.const_pool_index;
	stack[se] = const_vb(index);
    } else {				/* scalar constant must be inserted */
	stack_push(1, pe->type, NOTSEGD);	/* push a 1 elt vector */
	dest = se_vb(TOS)->vector;

	switch (pe->type) {		/* put in the value */
	    case Int: 
		assert_mem_size(rep_vuz_scratch(1));
		rep_vuz(dest, 0, pe->misc.int_const, 1, SCRATCH);
		break;
	    case Float: 
		assert_mem_size(rep_vud_scratch(1));
		rep_vud(dest, 0, pe->misc.float_const, 1, SCRATCH);
		break;
	    case Bool: 
		assert_mem_size(rep_vub_scratch(1));
		rep_vub(dest, 0, pe->misc.bool_const, 1, SCRATCH);
		break;
	    default:
		_fprintf(stderr, "vinterp: illegal type for CONST.\n");
		vinterp_exit (1);
		break;
	}
    }
}

/* Copy count elements of the stack, starting from start, moving up:
 * COPY 2 3 when stack looks like (0 1 2 3 4 5) results in (0 1 2 3 4 5 1 2)
 */
void do_copy(pe)
prog_entry_t *pe;
{
    int count = pe->misc.stack_args.arg1;
    int start = pe->misc.stack_args.arg2;	/* 0 is top of stack */
    int from, last;
    stack_entry_t se;

    assert(pe->vop == COPY);

    if (check_args && (start + count > stack_size)) {
	_fprintf(stderr, "vinterp: line %d: can't do a COPY %d %d; only %d items on the stack.\n",
		pe->lineno, count, start, stack_size);
	vinterp_exit (1);
    }
	
    /* copy each se until done */
    from = stack_index - start - count + 1;
    last = stack_index - start;

#ifdef SPEEDHACKS
    /* if no storage problems, can do fast loop */
    if (count < stack_max - stack_index) {
	/* NB: This loop doesn't vectorize because of loop carried
	 * dependency on the count++
	 */
	for (; from <= last; from ++) {
	    stack[++stack_index] = stack[from];
	    stack[stack_index]->count++;
	}
    } else {
#endif /* SPEEDHACKS */
	for (; from <= last; from ++) {
	    se = new_se();
	    stack[se] = stack[from];
	    vstack_copy(se_vb(se));
	}
#ifdef SPEEDHACKS
    }
#endif /* SPEEDHACKS */
}

/* copy a single se onto the top of stack */
void do_quick_copy(se)
stack_entry_t se;
{
    stack_entry_t newse = new_se();
    stack[newse] = stack[se];
    vstack_copy(se_vb(se));
}

/* Pop out values from middle of stack:
 * POP 2 3 when stack is (0 1 2 3 4 5) results in (0 3 4 5).
 */
void do_pop(pe)
prog_entry_t *pe;
{
    int count = pe->misc.stack_args.arg1;
    int start = pe->misc.stack_args.arg2;	/* 0 is top of stack */
    stack_entry_t bad, num, from;

    assert(pe->vop == POP);

    /* error checking */
    if (check_args) {
	if (start + count > stack_size) {
	    _fprintf(stderr, "vinterp: line %d: can't do a POP %d %d; only %d items on the stack.\n",
		    pe->lineno, count, start, stack_size);
	    vinterp_exit (1);
	} else if (count < 0 || start < 0) {
	    _fprintf(stderr, "vinterp: line %d: negative argument: POP %d %d\n", pe->lineno, count, start);
	    vinterp_exit (1); 
	}
    }
	
    /* vstack_pop the bad stack elements */
    for (bad = stack_index - start, num = 0; num < count; num++, bad --) {
	vstack_pop(se_vb(bad));
    }

    /* copy the remaining elements up */
#if cray
#pragma ivdep
#endif
    for (from = stack_index - start + 1; from <= stack_index; from ++) {
	stack[from-count] = stack[from];	/* move things count back */
    }

    stack_index -= count;
}

/* COPY and POP: this is a move instruction on the stack */
void do_cpop(pe)
prog_entry_t *pe;
{
    vb_t *se_array[100];			/* hack alert.... */
    int count = pe->misc.stack_args.arg1;
    int start = pe->misc.stack_args.arg2;	/* 0 is top of stack */
    int from, last, to, se_array_index;

    assert(pe->vop == CPOP);

    if (check_args && (start + count > stack_size)) {
	_fprintf(stderr, "vinterp: line %d: can't do a CPOP %d %d; only %d items on the stack.\n",
		pe->lineno, count, start, stack_size);
	vinterp_exit (1);
    }
	
    to = from = stack_index - start - count + 1;
    last = stack_index - start;

    /* move the popped items onto se_array */
    se_array_index = 0;
    while (from <= last) {
	se_array[se_array_index++] = stack[from++];
    }

    /* While copying, no need to do bounds check on the stack
     * because size is the same before and after operation.
     */

    /* from now points to first thing kept on stack */
    assert(from == last + 1);
    while (from <= stack_index) {
	stack[to++] = stack[from++];
    }

    last = se_array_index;
    from = 0;
    while (from < last) {
	stack[to++] = se_array[from++];
    }

    assert(to == TOS + 1);
}

/* PAIR: take the top two elements from the stack and group them together.
 * new_pair() does all the ref count work,
 */
void do_pair(pe)
prog_entry_t *pe;
{
    vb_t *vb_first, *vb_second, *vb_pair;
    assert(stack_index > 1);		/* at least two things on stack */

    vb_second = stack[stack_index];
    vb_first = stack[stack_index - 1];
    vb_pair = new_pair(vb_first, vb_second);

    stack_index--;
    stack[stack_index] = vb_pair;
}

void do_unpair(pe)
prog_entry_t *pe;
{
    vb_t *vb_pair, *vb_first, *vb_second;
    stack_entry_t se;

    vb_pair = stack[stack_index];

    if (check_args && vb_pair->type != Pair) {
	_fprintf(stderr, "vinterp: line %d: attempting to UNPAIR an non-pair (%s)\n",
		pe->lineno, type_string(vb_pair->type));
	vinterp_exit(1);
    }

    vb_unpair(vb_pair, &vb_first, &vb_second);

    stack[stack_index] = vb_first;
    se = new_se();
    stack[stack_index] = vb_second;
}

/* get_args: return pointers to top elements on the stack */
void get_args(pe, arg_array)
prog_entry_t *pe;
stack_entry_t arg_array[];			/* array to put args */
{
    int numargs = pe->vopdes->arg_num;		/* # args needed */
    int i;

    if (check_args && numargs > stack_size) {
	_fprintf(stderr, "vinterp: line %d: Only %d values on stack; %s reqires %d.\n",
			pe->lineno, stack_size, pe->vopdes->vopname, numargs);
	vinterp_exit (1);
    }

#if cray
#pragma ivdep
#endif
    /* grab top numarg elts off the stack */
    for (i = 0; i < numargs; i++) {
	arg_array[numargs - i - 1] = stack_index - i;
    }
}

/* show the current state of the stack.  For debugging purposes */
void show_stack()
{
    stack_entry_t se;

    for (se = 0; se < stack_size; se++) {
	if (se_vb(se)->type == Pair) {
	    _fprintf(stderr, "pair(%s,%s) ", 
			    type_string(se_vb(se)->first->type),
			    type_string(se_vb(se)->second->type));
	} else {
	    _fprintf(stderr, "%d(%s) ", se_vb(se)->len, 
		            type_string(se_vb(se)->type));
	}
    }
    _fprintf(stderr, "\n");
}

void stack_debug()
{
    show_stack_values(stderr);
}
void show_stack_values(stream)
FILE *stream;
{
    stack_entry_t se;

    for (se = 0; se < stack_size; se++) {
	if (se_vb(se)->type == Pair) {
	    _fprintf(stream, "pair(%s,%s) \n", 
			    type_string(se_vb(se)->first->type),
			    type_string(se_vb(se)->second->type));
	} else {
    	    write_value(se_vb(se), stream);
	}

    }
}

