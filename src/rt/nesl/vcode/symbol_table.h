/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

/*
 *
 * Header file for symbol_table.c.
 */

#ifndef	_SYMTAB
#define	_SYMTAB 1

#include "vcode.h"

typedef struct hash_entry {
    char                 fn_name[FN_NAME_MAX];  /* name of function */
    int                  prog_num;      /* line in prog where defined */
    struct hash_entry   *next;          /* pointer for hash collisions */
} hash_entry_data_t, *hash_entry_t;

extern void		hash_table_init PROTO_((void));
extern void		hash_table_deinit PROTO_((void));
extern void		hash_table_enter PROTO_((char*, int));
extern hash_entry_t	hash_table_lookup PROTO_((char*));

#endif /* _SYMTAB */
