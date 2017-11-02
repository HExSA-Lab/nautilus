/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

/* These functions access the internal tokenized format of a VCODE program. */

#include "config.h"
#include "vcode.h"
#include "y.tab.h"
#include "symbol_table.h"
#include "vstack.h"
#include "program.h"
#include "constant.h"

/* print out a TYPE */
char *type_string(type)
TYPE type;
{
    switch (type) {
	case Int:       return("Int");
	case Bool:      return("Bool");
	case Float:     return("Float");
	case None: 	return("None");
	case Segdes: 	return("Segdes");
	case String:	return("String");
	case Pair:	return("Pair");
	case Illegal:	return("Illegal");
	default:        _fprintf(stderr, "bad type: %d", type);
			return(NULL);
    }
}

/* print out a constant value */
static void show_const PROTO_((prog_entry_t *));
static void show_const(prog_entry)
prog_entry_t *prog_entry;
{
    if (prog_entry->type == Float)
	_fprintf(stderr, "type = Float; val = %f", prog_entry->misc.float_const);
    else if (prog_entry->type == Bool)
	_fprintf(stderr, "type = Bool; val = %s", 
			prog_entry->misc.bool_const ? "T" : "F");
    else if (prog_entry->type == Int || prog_entry->type == Segdes)
	_fprintf(stderr, "type = Int; val = %d", prog_entry->misc.int_const);
    else _fprintf(stderr, "illegal CONSTANT type");
    return;
}

/* print out a prog_entry structure in a meaningful way */
void show_prog_entry(prog_entry, line)
prog_entry_t *prog_entry;
int line;
{
    _fprintf(stderr, "(%d) vop = %s; ", line,
		  prog_entry->vopdes->vopname);
    if (prog_entry->vop <= LENGTHS)
	_fprintf(stderr, "type = %s", type_string(prog_entry->type));
    else {
	switch (prog_entry->vop) {
	    case READ: case WRITE: case FREAD: case FWRITE:
		_fprintf(stderr, "type = %s", type_string(prog_entry->type));
		break;
	    case CONST: 
		if (!prog_entry->is_in_const_pool)
		    show_const(prog_entry);
		else {
		    const_show_vec(prog_entry->misc.const_pool_index);
		}
		break;
	    case POP:
	    case COPY:
		_fprintf(stderr, "args = %d, %d",
				prog_entry->misc.stack_args.arg1,
				prog_entry->misc.stack_args.arg2);
		break;
	    case IF:
	    case ELSE:
	    case CALL:
		_fprintf(stderr, "branch to %d", prog_entry->misc.branch);
		break;
	    default:
		break;
	}
    }
    _fprintf(stderr, "; line = %d\n", prog_entry->lineno);
}


/* show_prog: print out a prettied version of the program. */
void show_program()
{
    int i;
    for (i = 0; i < next_prog_num; i++) {
	show_prog_entry(program + i, (program+i)->lineno);
    }
}
