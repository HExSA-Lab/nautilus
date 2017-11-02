/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

/* These functions are the actions for the YACC parser. */

#include "config.h"
#include "vcode.h"
#include "y.tab.h"
#include "symbol_table.h"
#include "link_list.h"
#include "program.h"
#include "constant.h"
#include "parse.h"

/* --------------------- program array ------------------------------- */

prog_entry_t	*program;
#define PROG_MAX_INITIAL	5000	/* initial size of program array */
static int prog_max;			/* size of allocated array */
int next_prog_num;			/* index of newest token */

void prog_init()
{
    program = (prog_entry_t *)malloc(PROG_MAX_INITIAL * sizeof (prog_entry_t));
    if (program == NULL) {
	_fprintf(stderr, "vinterp: error allocating program array.\n");
	vinterp_exit(1);
    }
    prog_max = PROG_MAX_INITIAL;
    next_prog_num = 0;
    yylineno = 0;
}

void prog_deinit()
{
    free(program);
    program=0;
}

/* inc_prog_num: increment next_prog_num counter and make sure array
 * is large enough
 */
static void inc_prog_count PROTO_((void))
{
    ++next_prog_num;
    if (next_prog_num >= prog_max) {
	prog_max *= 2;		/* double array size */
	program = (prog_entry_t *)
		  realloc(program, prog_max * sizeof(prog_entry_t));
	if (program == NULL) {
	    _fprintf(stderr, "vinterp: error reallocating program array.\n");
	    vinterp_exit(1);
	}
    }
}

/* ------------------------ utilities ------------------------ */

/* map a lex/yacc token to a TYPE */
TYPE type(n)
int     n;
{
    switch(n) {
        case INT:       return(Int);
        case BOOL:      return(Bool);
        case FLOAT:     return(Float);
	case SEGDES:	return(Segdes);
	case CHAR:	return(String);
        default:        _fprintf(stderr, "vinterp: line %d: parser: bad type %d\n", yylineno, n);
                        return(Illegal);
    }
}

/* -------------------------- basic actions ------------------------------*/

/* parse_stmt: add a new statement (elt-op, vec-op, seg-op) to the program.
 * The grammar assures us that the type arg for the operation is correct,
 * so don't need to error check this.
 */
void parse_stmt(op, type)
VOPCODE op;
TYPE type;
{
    prog_entry_t *prog = program + next_prog_num;
    
    prog->vop = op;
    prog->vopdes = vop_table_look(op);
    prog->lineno = yylineno;
    prog->type = type;
    inc_prog_count();
}

/* Found a new label:  FUNC IDENTIFIER
 * Must enter it into the symbol table, along with its location 
 * in the program so that we can branch to it.
 * FUNC is a nop, so don't generate any code for it.
 */
void parse_label(label)
char *label;
{
    if (strlen(label) >= FN_NAME_MAX) {
	_fprintf(stderr, "vinterp: line %d: warning: label %s greater than %d characters, truncating.\n",
			yylineno, label, FN_NAME_MAX);
	label[FN_NAME_MAX - 1] = '\0';
    }
    if (hash_table_lookup(label) != NULL) {
	/* print warning and continue */
	_fprintf(stderr, "vinterp: line %d: warning: label %s being redefined.\n", yylineno, label);
    }
    hash_table_enter(label, next_prog_num);
}

/* Function Call: enter location of call into link_list for linking later.
 * Generate a CALL op in program with aux field to be filled in at link
 * time.
 */
void parse_call(label)
char *label;
{
    prog_entry_t *prog;

    link_list_enter(label, next_prog_num);

    prog = program + next_prog_num;
    prog->vop = CALL;
    prog->vopdes = vop_table_look(CALL);
    prog->lineno = yylineno;
    prog->misc.branch = UNKNOWN_BRANCH;

    inc_prog_count();
}

/* generate a COPY or POP op code. */
void parse_stack(op, arg1, arg2)
VOPCODE op;
int arg1, arg2;
{
    prog_entry_t *prog = program + next_prog_num;

    if (op != COPY && op != POP && op != CPOP) {
	_fprintf(stderr, "vinterp: line %d: internal error: bad token value in parse_stack: %d\n", yylineno, op);
	parse_error = 1;
	return;
    }
    
    prog->vop = op;
    prog->vopdes = vop_table_look(op);
    prog->lineno = yylineno;
    prog->misc.stack_args.arg1 = arg1;
    prog->misc.stack_args.arg2 = arg2;

    inc_prog_count();
}

/* ----------------------------- constants --------------------------- */
/* There are two types of VCODE constants: vectors and scalars:
 * A scalar syntactically corresponds to 
 *		CONST INT 5
 * For this case, the parser calls parse_const(), which puts the
 * constant directly into the corresponding prog_entry.
 * type of constant is second argument.  Switch on this
 * value and grab actual constant from constant global.
 */
void parse_const(op, type)
VOPCODE op;
TYPE type;
{
    prog_entry_t *prog = program + next_prog_num;

    if (op != CONST) {
	_fprintf(stderr, "vinterp: line %d: bad token value in parse_const: %d\n", yylineno, op);
	parse_error = 1;
    }

    if (type == Segdes) {
	/* segdes's must get turned in segment descriptor immmediately,
	 * so let the vector constant functions deal with it 
	 */
	parse_begin_vec(Segdes);
	parse_val_vec(Segdes);
	parse_end_vec();
	return;
    }
	
    prog->vop = op;
    prog->vopdes = vop_table_look(op);
    prog->lineno = yylineno;
    prog->type = type;
    prog->is_in_const_pool = 0;

    switch (type) {
	case Int: case Segdes:
	    prog->misc.int_const = constant.ival;
	    break;
	case Float:
	    prog->misc.float_const = constant.dval;
	    break;
	case Bool:
	    prog->misc.bool_const = constant.bval;
	    break;
	default:
	    _fprintf(stderr, "vinterp: line %d: internal error: bad type value in parse_const: %d\n", yylineno, type);
	    parse_error = 1;
    }

    inc_prog_count();
}

/* A vector constant syntactically corresponds to 
 *              CONST INT (1 2 3 4 5)
 * or 		CONST CHAR "constant"
 * In this case, we parse in three parts: 
 *     parse_begin_vec() allocates storage and the prog_entry;
 *     parse_val_vec() adds the next element to the constant vector; 
 *     parse_end_vec() adds the vector to the constant pool.
 */

static TYPE current_type = Illegal;		/* type of current vector */

void parse_begin_vec(type)
TYPE type;
{
    assert(type == Int || type == Float || type == Bool || type == Segdes);

    current_type = type;

    if (! const_begin())
	parse_error = 1;

    return;
}

/* get a new element of a constant vector */
void parse_val_vec(type)
TYPE type;
{
    assert(type == Int || type == Float || type == Bool || type == Segdes);

    if (current_type != type) {
	_fprintf(stderr, "vinterp: line %d: parse error: illegal type mixing in constant\n", yylineno);
	parse_error = 1;
	return;
    }

    /* constant is a global holding the value of constant */
    const_new_val(type, constant);
}

void parse_end_vec()
{
    prog_entry_t *prog = program + next_prog_num;
    int index;

    prog->vop = CONST;
    prog->vopdes = vop_table_look(CONST);
    prog->lineno = yylineno;
    prog->type = current_type;
    prog->is_in_const_pool = 1;

    index = const_end(current_type);
    if (index == -1) {
	parse_error = 1;
    }
    prog->misc.const_pool_index = index;
    inc_prog_count();
}

/* strings get parsed separately */
void parse_string(string, len)
YYTEXT_T string;
int len;
{
    current_type = String;
    const_string(string, len);

    parse_end_vec();
}


/*------------------ conditionals --------------------------*/
/*
 * Maintain our own stack of pending IFs in order to resolve the branches.
 * So: 
 *     parse_if: put an IF entry in the program, and push the program number
 *            onto the if_stack.
 *     parse_else: put an ELSE node in the program.  This is just a branch to 
 *            the corresponding ENDIF.  pop off the IF entry from the
 *	      if stack and put in the branch to here.  push an ELSE node
 *            onto the stack.
 *     parse_endif: pop an entry (either IF or ELSE) from the if_stack.
 *            Put the current program number into branch entry of node.
 *	      No need to make new program entry for the ENDIF.
 */

/* For now, make the if_stack an array of ints.  this should eventually be
 * changed to a real stack that can grow dynamically.
 */
#define IF_STACK_MAX	50
int if_stack[IF_STACK_MAX];
int if_stack_num = 0;

static void if_push PROTO_((int));
static void if_push(node)
int node;
{
    if (if_stack_num >= IF_STACK_MAX) {
	_fprintf(stderr, "vinterp: line %d: IF nesting too deep (> %d).\n", yylineno, IF_STACK_MAX);
	vinterp_exit(1);
    } else {
	if_stack[if_stack_num++] = node;
    }
    return;
}

static int if_pop PROTO_((void))
{
    if (if_stack_num <= 0) { 
	_fprintf(stderr, "vinterp: line %d: improper IF nesting.\n", yylineno);
	vinterp_exit(1);
    }
    return if_stack[--if_stack_num];
}

/* indicate if stack is currently empty */
static int if_is_empty PROTO_((void))
{
    return (if_stack_num == 0);
}

static void if_clear PROTO_((void))
{
    if_stack_num = 0;
}
	

void parse_if()
{
    prog_entry_t *prog = program + next_prog_num;
    
    prog->vop = IF;
    prog->vopdes = vop_table_look(IF);
    prog->lineno = yylineno;

    prog->misc.branch = UNKNOWN_BRANCH;

    if_push(next_prog_num);
    inc_prog_count();
}

void parse_else()
{
    int if_prog_num = if_pop();
    prog_entry_t *prog = program + next_prog_num;
    prog_entry_t *if_prog = program + if_prog_num;
    
    /* fix up if node */
    if_prog->misc.branch = next_prog_num + 1; /* branch to stmt after ELSE */

    prog->vop = ELSE;
    prog->vopdes = vop_table_look(ELSE);
    prog->lineno = yylineno;

    prog->misc.branch = UNKNOWN_BRANCH;

    if_push(next_prog_num);
    inc_prog_count();
}

/* NB: there is no node for an ENDIF */
void parse_endif()
{
    int if_prog_num = if_pop();
    prog_entry_t *if_prog = program + if_prog_num;

    if_prog->misc.branch = next_prog_num;
}

/* ---------------------- return ------------------------------------- */

/* returns:  current language grammar only allows one return per
 *           function.  We'll enforce this here by making sure
 *           the if_stack is empty at this point.  Grammar
 *           should enforce everything else.
 */
void parse_return()
{
    prog_entry_t *prog = program + next_prog_num;
    
    prog->vop = RET;
    prog->vopdes = vop_table_look(RET);
    prog->lineno = yylineno;

    if (!if_is_empty()) {
	_fprintf(stderr, "vinterp: line %d: RETURN found in middle of IF...ENDIF.  Only one return per procedure.\n", yylineno);
	if_clear();
	parse_error = 1;
    }

    inc_prog_count();
}
