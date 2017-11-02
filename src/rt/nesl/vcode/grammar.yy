%{
/*
* Copyright (c) 1992, 1993, 1994 Carnegie Mellon University 
*/
/*
 * grammar.yy: Yacc source file for analyzing vcode source
 */
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "vcode.h"
#include "parse.h"

#ifndef	_GRAMMAR
#define	_GRAMMAR 1
#endif	/* _GRAMMAR */

int	parse_error = 0;
int 	main_defined = 0;
int	first_function = 1;
char 	main_fn[FN_NAME_MAX];
const_union constant;

void yyerror(s)
char *s;
{
    fprintf(stderr, "line %d: %s: \"%s\"\n", yylineno, s, yytext);
}

%}
%start program

%union {
    int ival;
    double dval;
    char *cval;
    char *id;
    int vop;
}

/* Elementwise operations */
%token <vop> PLUS MINUS TIMES DIV MOD
%token <vop> LT LEQ GT GEQ EQ NEQ LSHIFT RSHIFT
%token <vop> NOT AND OR XOR
%token <vop> SELECT RAND
%token <vop> FLOOR CEIL TRUNC ROUND 
%token <vop> LOG SQRT EXP
%token <vop> SIN COS TAN ASIN ACOS ATAN SINH COSH TANH
%token <vop> I_TO_F I_TO_B B_TO_I

/* Vector operations */
%token <vop> PLUS_SCAN MULT_SCAN MAX_SCAN MIN_SCAN AND_SCAN OR_SCAN XOR_SCAN
%token <vop> PLUS_REDUCE MULT_REDUCE MAX_REDUCE MIN_REDUCE 
%token <vop> AND_REDUCE OR_REDUCE XOR_REDUCE
%token <vop> PERMUTE DPERMUTE FPERMUTE BPERMUTE BFPERMUTE DFPERMUTE
%token <vop> EXTRACT REPLACE DIST INDEX
%token <vop> RANK_UP RANK_DOWN PACK

/* Segment descriptor operations */
%token <vop> LENGTH MAKE_SEGDES LENGTHS

/* Control operations */
%token <vop> COPY POP CPOP PAIR UNPAIR CALL RET FUNC IF ELSE ENDIF 
%token <vop> CONST
%token <vop> EXIT

/* timing ops */
%token <vop> START_TIMER STOP_TIMER

/* I/O operations */
%token <vop> READ WRITE FOPEN FCLOSE FREAD FREAD_CHAR FWRITE SPAWN

/* Seed random number generator */
%token <vop> SRAND

/* Types */
%token <vop> INT BOOL FLOAT SEGDES CHAR

/* Constants */
%token <ival> V_TRUE V_FALSE
%token <ival> NULL_STREAM STDIN STDOUT STDERR

/* Main */
%token <vop> MAIN

/* Miscellaneous */
%token <vop> BEGIN_VECTOR END_VECTOR
%token <ival> INTEGER 
%token <dval> REAL
%token <cval> STRING
%token <id> IDENTIFIER
%token <vop> INPUT_INFO OUTPUT_INFO

%type <vop> types_1 types_2 types_3 types_5 type_const
%type <ival> int_con bool_con file_con int_val bool_val 
%type <dval> float_val

%%				/* beginning of rules section */

program	:	fn_def
	|	program fn_def
	;

fn_def	:	fn_decl stmts fn_end
	;

fn_decl	:	fn_name fn_types
	;

fn_name	:	FUNC IDENTIFIER
			{ if (first_function) {/* first function */
			    strcpy(main_fn, $2);
			    first_function = 0;
			  }
			  parse_label($2);
			}
	|	FUNC MAIN
			{ if (main_defined) {
			    fprintf(stderr, "attempt to redefine main\n");
			    parse_error = 1;
			  } else {
			    strcpy(main_fn, "MAIN");
			    main_defined = 1;
			    first_function = 0;
			  }
			  parse_label("MAIN");
			}
	;

fn_types :	/* empty */
	|	INPUT_INFO type_decls
		OUTPUT_INFO type_decls
	;

type_decls :	/* empty */
	| 	type_decls type_decl
	;

type_decl :     types_5
        ;

fn_end 	:	RET
			{parse_return();}
	;

stmts   :	/* empty */
	|	stmts stmt
	;

stmt	:	elt_op
	|	vec_op
	|	seg_op
	|	ctl_op
	|	io_op
	;

elt_op	:	PLUS types_1
			{parse_stmt($1, type($2));}
	|	MINUS types_1
			{parse_stmt($1, type($2));}
	|	TIMES types_1
			{parse_stmt($1, type($2));}
	|	DIV types_1
			{parse_stmt($1, type($2));}
	|	MOD
			{parse_stmt($1, Int);}
	|	LT types_1
			{parse_stmt($1, type($2));}
	|	LEQ types_1
			{parse_stmt($1, type($2));}
	|	GT types_1
			{parse_stmt($1, type($2));}
	|	GEQ types_1
			{parse_stmt($1, type($2));}
	|	EQ types_1
			{parse_stmt($1, type($2));}
	|	NEQ types_1
			{parse_stmt($1, type($2));}
	|	LSHIFT
			{parse_stmt($1, Int);}
	|	RSHIFT
			{parse_stmt($1, Int);}
	|	NOT types_2
			{parse_stmt($1, type($2));}
	|	AND types_2
			{parse_stmt($1, type($2));}
	|	OR types_2
			{parse_stmt($1, type($2));}
	|	XOR types_2
			{parse_stmt($1, type($2));}
	|	SELECT types_3
			{parse_stmt($1, type($2));}
	|	RAND
			{parse_stmt($1, Int);}
	|	FLOOR
			{parse_stmt($1, Float);}
	|	CEIL
			{parse_stmt($1, Float);}
	|	TRUNC
			{parse_stmt($1, Float);}
	|	ROUND
			{parse_stmt($1, Float);}
	|	LOG
			{parse_stmt($1, Float);}
	|	SQRT
			{parse_stmt($1, Float);}
	|	EXP
			{parse_stmt($1, Float);}
	|	SIN
			{parse_stmt($1, Float);}
	|	COS
			{parse_stmt($1, Float);}
	|	TAN
			{parse_stmt($1, Float);}
	|	ASIN
			{parse_stmt($1, Float);}
	|	ACOS
			{parse_stmt($1, Float);}
	|	ATAN
			{parse_stmt($1, Float);}
	|	SINH
			{parse_stmt($1, Float);}
	|	COSH
			{parse_stmt($1, Float);}
	|	TANH
			{parse_stmt($1, Float);}
	|	I_TO_F
			{parse_stmt($1, Int);}
	|	I_TO_B
			{parse_stmt($1, Int);}
	|	B_TO_I
			{parse_stmt($1, Bool);}
	;

vec_op	:	PLUS_SCAN types_1
			{parse_stmt($1, type($2));}
	|	MULT_SCAN types_1
			{parse_stmt($1, type($2));}
	|	MAX_SCAN types_1
			{parse_stmt($1, type($2));}
	|	MIN_SCAN types_1
			{parse_stmt($1, type($2));}
	|	AND_SCAN types_2
			{parse_stmt($1, type($2));}
	|	OR_SCAN types_2
			{parse_stmt($1, type($2));}
	|	XOR_SCAN types_2
			{parse_stmt($1, type($2));}
	|	PLUS_REDUCE types_1
			{parse_stmt($1, type($2));}
	|	MULT_REDUCE types_1
			{parse_stmt($1, type($2));}
	|	MAX_REDUCE types_1
			{parse_stmt($1, type($2));}
	|	MIN_REDUCE types_1
			{parse_stmt($1, type($2));}
	|	AND_REDUCE types_2
			{parse_stmt($1, type($2));}
	|	OR_REDUCE types_2
			{parse_stmt($1, type($2));}
	|	XOR_REDUCE types_2
			{parse_stmt($1, type($2));}
	|	PERMUTE types_3
			{parse_stmt($1, type($2));}
	|	DPERMUTE types_3
			{parse_stmt($1, type($2));}
	|	FPERMUTE types_3
			{parse_stmt($1, type($2));}
	|	BPERMUTE types_3
			{parse_stmt($1, type($2));}
	|	BFPERMUTE types_3
			{parse_stmt($1, type($2));}
	|	DFPERMUTE types_3
			{parse_stmt($1, type($2));}
	|	DIST types_3
			{parse_stmt($1, type($2));}
	|	INDEX
			{parse_stmt($1, Int);}
	|	LENGTH types_3
			{parse_stmt($1, type($2));}
	|	EXTRACT types_3
			{parse_stmt($1, type($2));}
	|	REPLACE types_3
			{parse_stmt($1, type($2));}
	|	RANK_UP types_1
			{parse_stmt($1, type($2));}
	|	RANK_DOWN types_1
			{parse_stmt($1, type($2));}
	|	PACK types_3
			{parse_stmt($1, type($2));}
	;

seg_op	:	MAKE_SEGDES
			{parse_stmt($1, Segdes);}
	|	LENGTHS
			{parse_stmt($1, Int);}
	;

ctl_op	:	COPY int_con int_con
			{parse_stack($1, $2, $3);}
	|	POP int_con int_con
			{parse_stack($1, $2, $3);}
	|	CPOP int_con int_con
			{parse_stack($1, $2, $3);}
	|	PAIR
			{parse_stmt($1, type(INT));}
	|	UNPAIR 
			{parse_stmt($1, type(INT));}
	|	CALL IDENTIFIER
			{parse_call($2);}
	|	CALL MAIN
			{parse_call("MAIN"); /* MAIN is a token, use string */}
	|	IF
			{parse_if();}
	|	ELSE
			{parse_else();}
	|	ENDIF
			{parse_endif();}
	|	EXIT
			{parse_stmt($1, type(INT));}
	| 	CONST INT INTEGER
			{
			constant.ival = $3;
			parse_const($1, type($2));
			}
	|	CONST INT file_con
			{
			constant.ival = $3;
			parse_const($1, type($2));
			}
	| 	CONST FLOAT REAL
			{
			constant.dval =  $3;
			parse_const($1, type($2));
			}
	| 	CONST BOOL bool_con
			{
			constant.bval = $3;
			parse_const($1, type($2));
			}
	|	CONST SEGDES INTEGER
			{
			constant.ival = $3;
			parse_const($1, type($2));
			}
	|	CONST type_and_vector
			{
			}
	;

type_and_vector: type_const BEGIN_VECTOR vals END_VECTOR
			{parse_end_vec();}
	|	CHAR STRING
			{
			parse_string($2, yyleng);
			}

type_const : 	INT
			{parse_begin_vec(type($1));}
	|	BOOL
			{parse_begin_vec(type($1));}
	|	FLOAT
			{parse_begin_vec(type($1));}
	|	SEGDES
			{parse_begin_vec(type($1));}
	;

vals	:	/* empty */
	|	vals val
	;

val	: 	int_val
	|	float_val
	|	bool_val
	;

int_val	:	INTEGER
			{
			constant.ival = $$;
			parse_val_vec(Int);
			}
	;

float_val:	REAL
			{
			constant.dval =  $$;
			parse_val_vec(Float);
			}
	;

bool_val:	bool_con
			{
			constant.bval = $$;
			parse_val_vec(Bool);
			}
	;

io_op	:	START_TIMER
			{parse_stmt($1, type(INT));}
	|	STOP_TIMER
			{parse_stmt($1, type(INT));}
	|	READ types_5
			{parse_stmt($1, type($2));}
	|	WRITE types_5
			{parse_stmt($1, type($2));}
	|	FOPEN 
			{parse_stmt($1, type(INT));}
	|	FCLOSE
			{parse_stmt($1, type(INT));}
	|	FREAD types_5
			{parse_stmt($1, type($2));}
	|	FREAD_CHAR
			{parse_stmt($1, type(INT));}
	|	FWRITE types_5
			{parse_stmt($1, type($2));}
	|	SPAWN
			{parse_stmt($1, type(INT));}
	|	SRAND INT
			{parse_stmt($1, type(INT));}
	;

bool_con:	V_TRUE
			{$$ = 1;}
	|	V_FALSE
			{$$ = 0;}
	;

int_con :	INTEGER
			{$$ = $1;}
	;

file_con:	STDIN
			{$$ = STDIN_FD;}
	|	STDOUT
			{$$ = STDOUT_FD;}
	|	STDERR
			{$$ = STDERR_FD;}
	|	NULL_STREAM
			{$$ = NULL_STREAM_FD;}
	;
			
types_1	:	INT
	|	FLOAT
	;

types_2	:	INT
	|	BOOL
	;

types_3	:	INT
	|	BOOL
	|	FLOAT
	;

types_5	:	INT
	|	BOOL
	|	FLOAT
	|	SEGDES
	|	CHAR
	;
