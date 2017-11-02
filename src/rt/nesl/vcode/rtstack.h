/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

#ifndef _RT_STACK_H
#define _RT_STACK_H 1

extern void rtstack_init PROTO_((void));
extern void rtstack_push PROTO_((int));
extern int rtstack_pop PROTO_((void));

#define TOP_LEVEL	-1	/* indicates we're finished */

#endif /* _RT_STACK_H */
