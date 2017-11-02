/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

#include "vcode.h"

typedef struct _vcode_com_t {
    char *funct_string;
    VOPCODE vop;
} vcode_com_t;

extern VOPCODE lookup_id PROTO_((char *));
