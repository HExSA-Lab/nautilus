/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

#ifndef _CHECK_ARGS_H
#define _CHECK_ARGS_H 1

extern int args_ok PROTO_((prog_entry_t *, stack_entry_t []));
extern void allocate_result PROTO_((prog_entry_t*, stack_entry_t[], vb_t* []));
extern void assert_scratch PROTO_((prog_entry_t*, stack_entry_t[]));
extern void pop_args PROTO_((stack_entry_t[], int));

#endif /* _CHECK_ARGS_H */
