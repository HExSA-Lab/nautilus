/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

/*
 * symbol table for identifiers: hash table of linked lists.
 */


#include "config.h"
#include "vcode.h"
#include "symbol_table.h"

#define HASHSIZE        1021    /* want this to be prime */

hash_entry_t	node_table[HASHSIZE];

static int hash_fn PROTO_((char*));
static int hash_fn(s)
char * s;
/*
 * This is the hash function hashpjw on p.436 of the new Dragon book.
 */
{
    char	* p;
    unsigned 	h, g;

    h = 0;
    for (p = s; *p != '\0'; p++) {
	h = (h << 4) + (*p);
	if ((g = (h & 0xf0000000))) {
	    h = h ^ (g >> 24);
	    h = h ^ g;
	}
    }
    return(h%HASHSIZE);
}

static hash_entry_t new_hash_node PROTO_((void))
{
    hash_entry_t new_node = (hash_entry_t) malloc(sizeof(hash_entry_data_t));
    if (new_node == (hash_entry_t) NULL) {
	_fprintf(stderr, "vinterp: couldn't allocate symbol table entry.\n");
	vinterp_exit(1);
    }
    return new_node;
}
	
/* enter a new (id, location) pair into the symbol table */
void hash_table_enter(id, prog_num)
char *id;
int prog_num;
{
    int 		hash_val;
    hash_entry_t	new_hash_entry;
    
    new_hash_entry = new_hash_node();

    hash_val = hash_fn(id);

    new_hash_entry->next = node_table[hash_val];
    node_table[hash_val] = new_hash_entry;

    (void) strncpy(new_hash_entry->fn_name, id, FN_NAME_MAX); new_hash_entry->fn_name[FN_NAME_MAX-1] = 0;
    new_hash_entry->prog_num = prog_num;
}

/*
 * Looks up a node in the symbol table.  A prospective match is checked by
 * comparing node names.  NULL is returned in case of match failure.
 */
hash_entry_t hash_table_lookup(id)
char	* id;
{
    hash_entry_t	tmp;
    int			hash_val;

    hash_val = hash_fn(id);
    tmp = node_table[hash_val];

    for (;
	 ((tmp != NULL) && (strncmp(tmp->fn_name, id, FN_NAME_MAX) != 0));
    	 tmp = tmp->next)
      ;
    return(tmp);
}


void hash_table_init()
{
    int i;

    for (i = 0; i < HASHSIZE; i++)
	node_table[i] = NULL;
}

void hash_table_deinit()
{
    int i;
    hash_entry_t c;

    for (i = 0; i < HASHSIZE; i++) {
	while ((c=node_table[i])) {
	    node_table[i] = c->next;
	    free(c);
	}
	node_table[i] = NULL;
    }
}
