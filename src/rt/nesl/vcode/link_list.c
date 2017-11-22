/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

/* Functions for doing link of user defined vcode procedures */

#include "config.h"
#include "vcode.h"
#include "vstack.h"
#include "program.h"
#include "symbol_table.h"

extern int yylineno;
typedef struct link_list_entry {
    int                     prog_num;
    struct link_list_entry *next;
    char                    fn_name[FN_NAME_MAX];
} link_list_entry_t;

link_list_entry_t    *link_list;

void link_list_init()
{
    link_list = (link_list_entry_t *)NULL;
}


void link_list_deinit()
{
    link_list_entry_t *c;
    while ((c=link_list)) {
	link_list=c->next;
	free(c);
    }
}

static link_list_entry_t *new_link PROTO_((void))
{
    link_list_entry_t *link = (link_list_entry_t*) malloc(sizeof(link_list_entry_t));
    if (link == NULL) {
        _fprintf(stderr, "vinterp: internal error: couldn't allocate link list entry.\n");
        vinterp_exit(1);
    } 
    return link;
}

void link_list_enter(id, prog_num)
char *id;
int prog_num;
{
    link_list_entry_t	*new_link_entry;
    
    if (link_trace)
	_fprintf(stderr, "vinterp: linker: adding link for linking %s\n", id);

    new_link_entry = new_link();
    new_link_entry->next = link_list;
    link_list = new_link_entry;

    (void) strncpy(new_link_entry->fn_name, id, FN_NAME_MAX); new_link_entry->fn_name[FN_NAME_MAX-1] = 0;
    new_link_entry->prog_num = prog_num;
}

/* link up program: traverse the linked list looking up each function in
 * the symbol table and fixing up program branch entries.
 * Returns 1 on error, 0 if everything links.
 */
int do_link()
{
    link_list_entry_t *link = link_list;
    int link_error = 0;

    for (link = link_list; link != NULL; link = link->next) {
	hash_entry_t hash_entry = hash_table_lookup(link->fn_name);

        if (link_trace)
	    _fprintf(stderr, "vinterp: linker: linking %s\n", link->fn_name);

	if (hash_entry == NULL) {
	    _fprintf(stderr, "vinterp: line %d: unresolved identifier %s\n", 
                            link->prog_num, link->fn_name);
	    link_error = 1;
	} else {
	    program[link->prog_num].misc.branch = hash_entry->prog_num;
	}
    }
    return link_error;
}
