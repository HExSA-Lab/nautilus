%{
/*
* Copyright (c) 1992, 1993, 1994 Carnegie Mellon University 
*/

/* 
 * Lex source file for analyzing VCODE source.  yyin is redefined in 
 * main.c to point to the input VCODE file.
 */

#include "config.h"
#include "vcode.h"
#include "y.tab.h"
#include "parse.h"
#include "vcode_hash.h"

//#include <libccompat.h>

#ifndef _TOKENS
#define _TOKENS 1
#endif	/* _TOKENS */

int errno;

#define YYLMAX 100  /* Who knows if this is right */

#define show_token() if (lex_trace) printf("%s\n", yytext)
//#define show_token() printf("%s\n", yytext)

#ifdef __NAUTILUS__

// Temporary hack to pass program to the implementation

extern char *nk_nesl_current_parse_ptr

#define YY_INPUT(buf,result,max_size) { result = (*nk_nesl_current_parse_ptr == 0) ? YY_NULL : (buf[0] = *nk_nesl_current_parse_ptr++, 1); } 

#endif


VOPCODE lookup;

%}
%option yylineno


digit		[0-9]
integer		[-+]?{digit}+
float		[-+]?{digit}+\.({digit}+)?([Ee][-+]?{digit}+)?
identifier	[-+*/%<>=!._A-Za-z0-9]+
comment		"{"[^}]*"}"
string		\"[^"]*\"
whitespace	[ \t\n]
begin_vector	"("
end_vector	")"
input_info	I
output_info	O

%%				/* rules section begins here */

{integer}	{
		 show_token(); 
		 sscanf(yytext, "%d", &yylval.ival);
		 return(INTEGER);
		}

{float}		{
		 show_token(); 
		 sscanf(yytext, "%lf", &yylval.dval);
		 return(REAL);
		} 

{input_info}    {
                 show_token();
                 return(INPUT_INFO);
                }

{output_info}   {
                 show_token();
                 return(OUTPUT_INFO);
                }


{identifier}	{
		 show_token(); 
		 lookup = lookup_id(yytext);
		 if (lookup == IDENTIFIER) 
		    yylval.id = yytext;
		 else 
		    yylval.vop = lookup;
		 return (lookup);
		}

{string}	{
		 show_token();
		 yylval.cval = yytext;
		 if (yyleng >= YYLMAX) {
		    fprintf (stderr, "vinterp: lexer: line %d: strings longer than %d characters may not be parsed correctly by lex\n", yylineno, YYLMAX);
		 }
		 return(STRING);
		}

{begin_vector}  {
		 show_token();
		 return(BEGIN_VECTOR);
		}
{end_vector}	{
		 show_token();
		 return(END_VECTOR);
		}
{comment}	{show_token();}

\n              {}

{whitespace}	; 
.		{
    		    fprintf(stderr, "vinterp: lexer: Unexpected character in input at ");
		    fprintf(stderr, "line %d\n", yylineno);
		}
%%
