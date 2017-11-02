/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

#ifndef _CONSTANT_H
#define _CONSTANT_H 1

#include "vcode.h"
#include "vstack.h"
#include "parse.h"

extern int const_begin PROTO_((void));
extern void const_new_val PROTO_((TYPE, const_union));
extern void const_string PROTO_((YYTEXT_T, int));
extern int const_end PROTO_((TYPE));
extern void const_install PROTO_((void));
extern void const_show_vec PROTO_((int));
extern vb_t *const_vb PROTO_((int));

#endif /* _CONSTANT_H */
