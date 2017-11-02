/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

/* Module for dealing with runtime stack */

/* The runtime stack is an array of integers representing 
 * the call stack of the pc as the interpreter executes the program.
 */

#include "config.h"
#include "vcode.h"
#include "rtstack.h"

static int rt_stack_max = 10;
static int rt_stack_count = 0;	/* points to place for new item */

int *rt_stack;

void rtstack_init()
{
    if ((rt_stack = (int *) malloc(rt_stack_max * sizeof(int))) == NULL) {
	_fprintf(stderr, "vinterp: Couldn't allocate rt_stack.\n");
	vinterp_exit (1);
    }
    return;
}
    
/* push a new value onto the stack, increasing size of array, if necessary. */
void rtstack_push(val)
int val;
{
    /* double size of stack if run out of room */
    if (rt_stack_count == rt_stack_max) {
	rt_stack_max *= 2;
	rt_stack = (int *) realloc(rt_stack, rt_stack_max * sizeof(int));
	if (rt_stack == NULL) {
	    _fprintf(stderr, "vinterp: Couldn't reallocate rt_stack to %d\n", rt_stack_max);
	    vinterp_exit (1);
        }
    }

    if (runtime_trace)
        _fprintf(stderr, "push: \t stack_count = %d, saved_pc = %d\n", rt_stack_count, val);

    rt_stack[rt_stack_count++] = val;
    return;
}

/* pop the PC */
int rtstack_pop()
{
    if (runtime_trace)
        _fprintf(stderr, "pop: \t stack_count = %d, new_pc = %d\n", rt_stack_count - 1, rt_stack[rt_stack_count - 1]);

    return rt_stack[--rt_stack_count];
}
