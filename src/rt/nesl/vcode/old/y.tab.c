#ifndef lint
static const char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif

#include <stdlib.h>
#include <string.h>

#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYPATCH 20070509

#define YYEMPTY (-1)
#define yyclearin    (yychar = YYEMPTY)
#define yyerrok      (yyerrflag = 0)
#define YYRECOVERING (yyerrflag != 0)

extern int yyparse(void);

static int yygrowstack(void);
#define YYPREFIX "yy"
#line 2 "grammar.yy"
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

#line 34 "grammar.yy"
typedef union {
    int ival;
    double dval;
    char *cval;
    char *id;
    int vop;
} YYSTYPE;
#line 61 "y.tab.c"
#define PLUS 257
#define MINUS 258
#define TIMES 259
#define DIV 260
#define MOD 261
#define LT 262
#define LEQ 263
#define GT 264
#define GEQ 265
#define EQ 266
#define NEQ 267
#define LSHIFT 268
#define RSHIFT 269
#define NOT 270
#define AND 271
#define OR 272
#define XOR 273
#define SELECT 274
#define RAND 275
#define FLOOR 276
#define CEIL 277
#define TRUNC 278
#define ROUND 279
#define LOG 280
#define SQRT 281
#define EXP 282
#define SIN 283
#define COS 284
#define TAN 285
#define ASIN 286
#define ACOS 287
#define ATAN 288
#define SINH 289
#define COSH 290
#define TANH 291
#define I_TO_F 292
#define I_TO_B 293
#define B_TO_I 294
#define PLUS_SCAN 295
#define MULT_SCAN 296
#define MAX_SCAN 297
#define MIN_SCAN 298
#define AND_SCAN 299
#define OR_SCAN 300
#define XOR_SCAN 301
#define PLUS_REDUCE 302
#define MULT_REDUCE 303
#define MAX_REDUCE 304
#define MIN_REDUCE 305
#define AND_REDUCE 306
#define OR_REDUCE 307
#define XOR_REDUCE 308
#define PERMUTE 309
#define DPERMUTE 310
#define FPERMUTE 311
#define BPERMUTE 312
#define BFPERMUTE 313
#define DFPERMUTE 314
#define EXTRACT 315
#define REPLACE 316
#define DIST 317
#define INDEX 318
#define RANK_UP 319
#define RANK_DOWN 320
#define PACK 321
#define LENGTH 322
#define MAKE_SEGDES 323
#define LENGTHS 324
#define COPY 325
#define POP 326
#define CPOP 327
#define PAIR 328
#define UNPAIR 329
#define CALL 330
#define RET 331
#define FUNC 332
#define IF 333
#define ELSE 334
#define ENDIF 335
#define CONST 336
#define EXIT 337
#define START_TIMER 338
#define STOP_TIMER 339
#define READ 340
#define WRITE 341
#define FOPEN 342
#define FCLOSE 343
#define FREAD 344
#define FREAD_CHAR 345
#define FWRITE 346
#define SPAWN 347
#define SRAND 348
#define INT 349
#define BOOL 350
#define FLOAT 351
#define SEGDES 352
#define CHAR 353
#define V_TRUE 354
#define V_FALSE 355
#define NULL_STREAM 356
#define STDIN 357
#define STDOUT 358
#define STDERR 359
#define MAIN 360
#define BEGIN_VECTOR 361
#define END_VECTOR 362
#define INTEGER 363
#define REAL 364
#define STRING 365
#define IDENTIFIER 366
#define INPUT_INFO 367
#define OUTPUT_INFO 368
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,    0,   12,   13,   16,   16,   17,   17,   18,   18,
   19,   15,   14,   14,   20,   20,   20,   20,   20,   21,
   21,   21,   21,   21,   21,   21,   21,   21,   21,   21,
   21,   21,   21,   21,   21,   21,   21,   21,   21,   21,
   21,   21,   21,   21,   21,   21,   21,   21,   21,   21,
   21,   21,   21,   21,   21,   21,   21,   22,   22,   22,
   22,   22,   22,   22,   22,   22,   22,   22,   22,   22,
   22,   22,   22,   22,   22,   22,   22,   22,   22,   22,
   22,   22,   22,   22,   22,   23,   23,   24,   24,   24,
   24,   24,   24,   24,   24,   24,   24,   24,   24,   24,
   24,   24,   24,   24,   26,   26,    5,    5,    5,    5,
   27,   27,   28,   28,   28,    9,   11,   10,   25,   25,
   25,   25,   25,   25,   25,   25,   25,   25,   25,    7,
    7,    6,    8,    8,    8,    8,    1,    1,    2,    2,
    3,    3,    3,    4,    4,    4,    4,    4,
};
short yylen[] = {                                         2,
    1,    2,    3,    2,    2,    2,    0,    4,    0,    2,
    1,    1,    0,    2,    1,    1,    1,    1,    1,    2,
    2,    2,    2,    1,    2,    2,    2,    2,    2,    2,
    1,    1,    2,    2,    2,    2,    2,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    1,    2,
    2,    2,    2,    2,    2,    1,    1,    3,    3,    3,
    1,    1,    2,    2,    1,    1,    1,    1,    3,    3,
    3,    3,    3,    2,    4,    2,    1,    1,    1,    1,
    0,    2,    1,    1,    1,    1,    1,    1,    1,    1,
    2,    2,    1,    1,    2,    1,    2,    1,    2,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,
};
short yydefred[] = {                                      0,
    0,    0,    1,   13,    0,    6,    5,    2,    0,    9,
    4,    0,    0,    0,    0,   24,    0,    0,    0,    0,
    0,    0,   31,   32,    0,    0,    0,    0,    0,   38,
   39,   40,   41,   42,   43,   44,   45,   46,   47,   48,
   49,   50,   51,   52,   53,   54,   55,   56,   57,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   79,    0,    0,    0,    0,   86,   87,    0,
    0,    0,   91,   92,    0,   12,   95,   96,   97,    0,
   98,  119,  120,    0,    0,  123,  124,    0,  126,    0,
  128,    0,    3,   14,   15,   16,   17,   18,   19,    0,
  137,  138,   20,   21,   22,   23,   25,   26,   27,   28,
   29,   30,  139,  140,   33,   34,   35,   36,  141,  142,
  143,   37,   58,   59,   60,   61,   62,   63,   64,   65,
   66,   67,   68,   69,   70,   71,   72,   73,   74,   75,
   76,   77,   81,   82,   78,   83,   84,   85,   80,  132,
    0,    0,    0,   94,   93,    0,    0,    0,    0,    0,
    0,  104,  144,  145,  146,  147,  148,  121,  122,  125,
  127,  129,    9,   11,   10,   88,   89,   90,  136,  133,
  134,  135,   99,  100,  130,  131,  102,  101,  103,  106,
  111,    0,    0,  105,  116,  117,  118,  113,  115,  114,
  112,
};
short yydgoto[] = {                                       2,
  113,  125,  132,  184,  171,  161,  197,  194,  208,  209,
  210,    3,    4,    9,  103,    5,   11,  110,  185,  104,
  105,  106,  107,  108,  109,  172,  203,  211,
};
short yysindex[] = {                                    -51,
  -52,  -51,    0,    0, -292,    0,    0,    0, -257,    0,
    0,  -39,  -39,  -39,  -39,    0,  -39,  -39,  -39,  -39,
  -39,  -39,    0,    0,  -53,  -53,  -53,  -53,    1,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  -39,
  -39,  -39,  -39,  -53,  -53,  -53,  -39,  -39,  -39,  -39,
  -53,  -53,  -53,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    0,  -39,  -39,    1,    1,    0,    0, -196,
 -196, -196,    0,    0,  -47,    0,    0,    0,    0,   10,
    0,    0,    0,   15,   15,    0,    0,   15,    0,   15,
    0,  -90,    0,    0,    0,    0,    0,    0,    0,  -61,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
 -196, -196, -196,    0,    0,  -27,  -34,  -71,  -62,  -28,
  -23,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   15,  -60,    0,    0,    0,    0,    0,    0,    0,
    0,
};
short yyrindex[] = {                                      0,
    0,    0,    0,    0, -165,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   -6,    8,    9,   11,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  -73,    0,    0,    0,    0,    0,    0,    0,    0,
    0,
};
short yygindex[] = {                                      0,
  265,  272,  277,  211,    0,  195,  168,    0,    0,    0,
    0,  371,    0,    0,    0,    0,    0,  191,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
};
#define YYTABLESIZE 374
short yytable[] = {                                      12,
   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,
   23,   24,   25,   26,   27,   28,   29,   30,   31,   32,
   33,   34,   35,   36,   37,   38,   39,   40,   41,   42,
   43,   44,   45,   46,   47,   48,   49,   50,   51,   52,
   53,   54,   55,   56,   57,   58,   59,   60,   61,   62,
   63,   64,   65,   66,   67,   68,   69,   70,   71,   72,
   73,   74,   75,   76,   77,   78,   79,   80,   81,   82,
   83,   84,   85,   86,   10,   87,   88,   89,   90,   91,
   92,   93,   94,   95,   96,   97,   98,   99,  100,  101,
  102,    7,    7,    7,    7,    7,    7,    7,    7,    7,
    7,    7,    7,    7,    7,    7,    7,    7,    7,    7,
    7,    7,    7,    7,    7,    7,    7,    7,    7,    7,
    7,    7,    7,    7,    7,    7,    7,    7,    7,    7,
    7,    7,    7,    7,    7,    7,    7,    7,    7,    7,
    7,    7,    7,    7,    7,    7,    7,    7,    7,    7,
    7,    7,    7,    7,    7,    7,    7,    7,    7,    7,
    7,    7,    7,    7,    7,    7,  160,    7,    7,    7,
    7,    7,    7,    7,    7,    7,    7,    7,    7,    7,
    7,    7,    7,    8,    8,    8,    8,    8,    8,    8,
    8,    8,    8,    8,    8,    8,    8,    8,    8,    8,
    8,    8,    8,    8,    8,    8,    8,    8,    8,    8,
    8,    8,    8,    8,    8,    8,    8,    8,    8,    8,
    8,    8,    8,    8,    8,    8,    8,    8,    8,    8,
    8,    8,    8,    8,    8,    8,    8,    8,    8,    8,
    8,    8,    8,    8,    8,    8,    8,    8,    8,    8,
    8,    8,    8,    8,    8,    8,    8,    8,  182,    8,
    8,    8,    8,    8,    8,    8,    8,    8,    8,    8,
    8,    8,    8,    8,    8,  162,  163,  114,  115,  116,
    1,  117,  118,  119,  120,  121,  122,  173,  174,  175,
  176,  177,  198,  195,  196,  123,  124,  126,  127,  128,
  199,  204,  205,  206,  178,  179,  183,    6,  180,  111,
  181,  112,  164,    7,  133,  134,  135,  136,  165,  195,
  196,  140,  141,  142,  143,  137,  138,  139,  189,  190,
  191,  192,  144,  145,  146,  193,  200,  201,  156,  157,
  147,  148,  149,  150,  151,  152,  153,  154,  155,  129,
  130,  131,  158,  159,  107,  186,  187,  188,  166,  167,
  168,  169,  170,  173,  174,  175,  176,  177,  108,  109,
  207,  110,    8,  202,
};
short yycheck[] = {                                     257,
  258,  259,  260,  261,  262,  263,  264,  265,  266,  267,
  268,  269,  270,  271,  272,  273,  274,  275,  276,  277,
  278,  279,  280,  281,  282,  283,  284,  285,  286,  287,
  288,  289,  290,  291,  292,  293,  294,  295,  296,  297,
  298,  299,  300,  301,  302,  303,  304,  305,  306,  307,
  308,  309,  310,  311,  312,  313,  314,  315,  316,  317,
  318,  319,  320,  321,  322,  323,  324,  325,  326,  327,
  328,  329,  330,  331,  367,  333,  334,  335,  336,  337,
  338,  339,  340,  341,  342,  343,  344,  345,  346,  347,
  348,  257,  258,  259,  260,  261,  262,  263,  264,  265,
  266,  267,  268,  269,  270,  271,  272,  273,  274,  275,
  276,  277,  278,  279,  280,  281,  282,  283,  284,  285,
  286,  287,  288,  289,  290,  291,  292,  293,  294,  295,
  296,  297,  298,  299,  300,  301,  302,  303,  304,  305,
  306,  307,  308,  309,  310,  311,  312,  313,  314,  315,
  316,  317,  318,  319,  320,  321,  322,  323,  324,  325,
  326,  327,  328,  329,  330,  331,  363,  333,  334,  335,
  336,  337,  338,  339,  340,  341,  342,  343,  344,  345,
  346,  347,  348,  257,  258,  259,  260,  261,  262,  263,
  264,  265,  266,  267,  268,  269,  270,  271,  272,  273,
  274,  275,  276,  277,  278,  279,  280,  281,  282,  283,
  284,  285,  286,  287,  288,  289,  290,  291,  292,  293,
  294,  295,  296,  297,  298,  299,  300,  301,  302,  303,
  304,  305,  306,  307,  308,  309,  310,  311,  312,  313,
  314,  315,  316,  317,  318,  319,  320,  321,  322,  323,
  324,  325,  326,  327,  328,  329,  330,  331,  349,  333,
  334,  335,  336,  337,  338,  339,  340,  341,  342,  343,
  344,  345,  346,  347,  348,   81,   82,   13,   14,   15,
  332,   17,   18,   19,   20,   21,   22,  349,  350,  351,
  352,  353,  364,  354,  355,  349,  350,   26,   27,   28,
  363,  362,  363,  364,   94,   95,  368,  360,   98,  349,
  100,  351,  360,  366,   50,   51,   52,   53,  366,  354,
  355,   57,   58,   59,   60,   54,   55,   56,  356,  357,
  358,  359,   61,   62,   63,  363,  365,  361,   74,   75,
   64,   65,   66,   67,   68,   69,   70,   71,   72,  349,
  350,  351,   76,   77,  361,  161,  162,  163,  349,  350,
  351,  352,  353,  349,  350,  351,  352,  353,  361,  361,
  203,  361,    2,  183,
};
#define YYFINAL 2
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 368
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"PLUS","MINUS","TIMES","DIV",
"MOD","LT","LEQ","GT","GEQ","EQ","NEQ","LSHIFT","RSHIFT","NOT","AND","OR","XOR",
"SELECT","RAND","FLOOR","CEIL","TRUNC","ROUND","LOG","SQRT","EXP","SIN","COS",
"TAN","ASIN","ACOS","ATAN","SINH","COSH","TANH","I_TO_F","I_TO_B","B_TO_I",
"PLUS_SCAN","MULT_SCAN","MAX_SCAN","MIN_SCAN","AND_SCAN","OR_SCAN","XOR_SCAN",
"PLUS_REDUCE","MULT_REDUCE","MAX_REDUCE","MIN_REDUCE","AND_REDUCE","OR_REDUCE",
"XOR_REDUCE","PERMUTE","DPERMUTE","FPERMUTE","BPERMUTE","BFPERMUTE","DFPERMUTE",
"EXTRACT","REPLACE","DIST","INDEX","RANK_UP","RANK_DOWN","PACK","LENGTH",
"MAKE_SEGDES","LENGTHS","COPY","POP","CPOP","PAIR","UNPAIR","CALL","RET","FUNC",
"IF","ELSE","ENDIF","CONST","EXIT","START_TIMER","STOP_TIMER","READ","WRITE",
"FOPEN","FCLOSE","FREAD","FREAD_CHAR","FWRITE","SPAWN","SRAND","INT","BOOL",
"FLOAT","SEGDES","CHAR","V_TRUE","V_FALSE","NULL_STREAM","STDIN","STDOUT",
"STDERR","MAIN","BEGIN_VECTOR","END_VECTOR","INTEGER","REAL","STRING",
"IDENTIFIER","INPUT_INFO","OUTPUT_INFO",
};
char *yyrule[] = {
"$accept : program",
"program : fn_def",
"program : program fn_def",
"fn_def : fn_decl stmts fn_end",
"fn_decl : fn_name fn_types",
"fn_name : FUNC IDENTIFIER",
"fn_name : FUNC MAIN",
"fn_types :",
"fn_types : INPUT_INFO type_decls OUTPUT_INFO type_decls",
"type_decls :",
"type_decls : type_decls type_decl",
"type_decl : types_5",
"fn_end : RET",
"stmts :",
"stmts : stmts stmt",
"stmt : elt_op",
"stmt : vec_op",
"stmt : seg_op",
"stmt : ctl_op",
"stmt : io_op",
"elt_op : PLUS types_1",
"elt_op : MINUS types_1",
"elt_op : TIMES types_1",
"elt_op : DIV types_1",
"elt_op : MOD",
"elt_op : LT types_1",
"elt_op : LEQ types_1",
"elt_op : GT types_1",
"elt_op : GEQ types_1",
"elt_op : EQ types_1",
"elt_op : NEQ types_1",
"elt_op : LSHIFT",
"elt_op : RSHIFT",
"elt_op : NOT types_2",
"elt_op : AND types_2",
"elt_op : OR types_2",
"elt_op : XOR types_2",
"elt_op : SELECT types_3",
"elt_op : RAND",
"elt_op : FLOOR",
"elt_op : CEIL",
"elt_op : TRUNC",
"elt_op : ROUND",
"elt_op : LOG",
"elt_op : SQRT",
"elt_op : EXP",
"elt_op : SIN",
"elt_op : COS",
"elt_op : TAN",
"elt_op : ASIN",
"elt_op : ACOS",
"elt_op : ATAN",
"elt_op : SINH",
"elt_op : COSH",
"elt_op : TANH",
"elt_op : I_TO_F",
"elt_op : I_TO_B",
"elt_op : B_TO_I",
"vec_op : PLUS_SCAN types_1",
"vec_op : MULT_SCAN types_1",
"vec_op : MAX_SCAN types_1",
"vec_op : MIN_SCAN types_1",
"vec_op : AND_SCAN types_2",
"vec_op : OR_SCAN types_2",
"vec_op : XOR_SCAN types_2",
"vec_op : PLUS_REDUCE types_1",
"vec_op : MULT_REDUCE types_1",
"vec_op : MAX_REDUCE types_1",
"vec_op : MIN_REDUCE types_1",
"vec_op : AND_REDUCE types_2",
"vec_op : OR_REDUCE types_2",
"vec_op : XOR_REDUCE types_2",
"vec_op : PERMUTE types_3",
"vec_op : DPERMUTE types_3",
"vec_op : FPERMUTE types_3",
"vec_op : BPERMUTE types_3",
"vec_op : BFPERMUTE types_3",
"vec_op : DFPERMUTE types_3",
"vec_op : DIST types_3",
"vec_op : INDEX",
"vec_op : LENGTH types_3",
"vec_op : EXTRACT types_3",
"vec_op : REPLACE types_3",
"vec_op : RANK_UP types_1",
"vec_op : RANK_DOWN types_1",
"vec_op : PACK types_3",
"seg_op : MAKE_SEGDES",
"seg_op : LENGTHS",
"ctl_op : COPY int_con int_con",
"ctl_op : POP int_con int_con",
"ctl_op : CPOP int_con int_con",
"ctl_op : PAIR",
"ctl_op : UNPAIR",
"ctl_op : CALL IDENTIFIER",
"ctl_op : CALL MAIN",
"ctl_op : IF",
"ctl_op : ELSE",
"ctl_op : ENDIF",
"ctl_op : EXIT",
"ctl_op : CONST INT INTEGER",
"ctl_op : CONST INT file_con",
"ctl_op : CONST FLOAT REAL",
"ctl_op : CONST BOOL bool_con",
"ctl_op : CONST SEGDES INTEGER",
"ctl_op : CONST type_and_vector",
"type_and_vector : type_const BEGIN_VECTOR vals END_VECTOR",
"type_and_vector : CHAR STRING",
"type_const : INT",
"type_const : BOOL",
"type_const : FLOAT",
"type_const : SEGDES",
"vals :",
"vals : vals val",
"val : int_val",
"val : float_val",
"val : bool_val",
"int_val : INTEGER",
"float_val : REAL",
"bool_val : bool_con",
"io_op : START_TIMER",
"io_op : STOP_TIMER",
"io_op : READ types_5",
"io_op : WRITE types_5",
"io_op : FOPEN",
"io_op : FCLOSE",
"io_op : FREAD types_5",
"io_op : FREAD_CHAR",
"io_op : FWRITE types_5",
"io_op : SPAWN",
"io_op : SRAND INT",
"bool_con : V_TRUE",
"bool_con : V_FALSE",
"int_con : INTEGER",
"file_con : STDIN",
"file_con : STDOUT",
"file_con : STDERR",
"file_con : NULL_STREAM",
"types_1 : INT",
"types_1 : FLOAT",
"types_2 : INT",
"types_2 : BOOL",
"types_3 : INT",
"types_3 : BOOL",
"types_3 : FLOAT",
"types_5 : INT",
"types_5 : BOOL",
"types_5 : FLOAT",
"types_5 : SEGDES",
"types_5 : CHAR",
};
#endif
#if YYDEBUG
#include <stdio.h>
#endif

/* define the initial stack-sizes */
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH  YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH  10000
#endif
#endif

#define YYINITSTACKSIZE 500

int      yydebug;
int      yynerrs;
int      yyerrflag;
int      yychar;
short   *yyssp;
YYSTYPE *yyvsp;
YYSTYPE  yyval;
YYSTYPE  yylval;

/* variables for the parser stack */
static short   *yyss;
static short   *yysslim;
static YYSTYPE *yyvs;
static int      yystacksize;
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(void)
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;

    i = yyssp - yyss;
    newss = (yyss != 0)
          ? (short *)realloc(yyss, newsize * sizeof(*newss))
          : (short *)malloc(newsize * sizeof(*newss));
    if (newss == 0)
        return -1;

    yyss  = newss;
    yyssp = newss + i;
    newvs = (yyvs != 0)
          ? (YYSTYPE *)realloc(yyvs, newsize * sizeof(*newvs))
          : (YYSTYPE *)malloc(newsize * sizeof(*newvs));
    if (newvs == 0)
        return -1;

    yyvs = newvs;
    yyvsp = newvs + i;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse(void)
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register const char *yys;

    if ((yys = getenv("YYDEBUG")) != 0)
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = YYEMPTY;

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = YYEMPTY;
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;

    yyerror("syntax error");

#ifdef lint
    goto yyerrlab;
#endif

yyerrlab:
    ++yynerrs;

yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = YYEMPTY;
        goto yyloop;
    }

yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    if (yym)
        yyval = yyvsp[1-yym];
    else
        memset(&yyval, 0, sizeof yyval);
    switch (yyn)
    {
case 5:
#line 112 "grammar.yy"
{ if (first_function) {/* first function */
			    strcpy(main_fn, yyvsp[0].id);
			    first_function = 0;
			  }
			  parse_label(yyvsp[0].id);
			}
break;
case 6:
#line 119 "grammar.yy"
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
break;
case 12:
#line 144 "grammar.yy"
{parse_return();}
break;
case 20:
#line 159 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 21:
#line 161 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 22:
#line 163 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 23:
#line 165 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 24:
#line 167 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Int);}
break;
case 25:
#line 169 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 26:
#line 171 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 27:
#line 173 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 28:
#line 175 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 29:
#line 177 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 30:
#line 179 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 31:
#line 181 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Int);}
break;
case 32:
#line 183 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Int);}
break;
case 33:
#line 185 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 34:
#line 187 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 35:
#line 189 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 36:
#line 191 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 37:
#line 193 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 38:
#line 195 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Int);}
break;
case 39:
#line 197 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 40:
#line 199 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 41:
#line 201 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 42:
#line 203 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 43:
#line 205 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 44:
#line 207 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 45:
#line 209 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 46:
#line 211 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 47:
#line 213 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 48:
#line 215 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 49:
#line 217 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 50:
#line 219 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 51:
#line 221 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 52:
#line 223 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 53:
#line 225 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 54:
#line 227 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Float);}
break;
case 55:
#line 229 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Int);}
break;
case 56:
#line 231 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Int);}
break;
case 57:
#line 233 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Bool);}
break;
case 58:
#line 237 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 59:
#line 239 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 60:
#line 241 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 61:
#line 243 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 62:
#line 245 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 63:
#line 247 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 64:
#line 249 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 65:
#line 251 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 66:
#line 253 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 67:
#line 255 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 68:
#line 257 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 69:
#line 259 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 70:
#line 261 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 71:
#line 263 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 72:
#line 265 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 73:
#line 267 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 74:
#line 269 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 75:
#line 271 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 76:
#line 273 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 77:
#line 275 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 78:
#line 277 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 79:
#line 279 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Int);}
break;
case 80:
#line 281 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 81:
#line 283 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 82:
#line 285 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 83:
#line 287 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 84:
#line 289 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 85:
#line 291 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 86:
#line 295 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Segdes);}
break;
case 87:
#line 297 "grammar.yy"
{parse_stmt(yyvsp[0].vop, Int);}
break;
case 88:
#line 301 "grammar.yy"
{parse_stack(yyvsp[-2].vop, yyvsp[-1].ival, yyvsp[0].ival);}
break;
case 89:
#line 303 "grammar.yy"
{parse_stack(yyvsp[-2].vop, yyvsp[-1].ival, yyvsp[0].ival);}
break;
case 90:
#line 305 "grammar.yy"
{parse_stack(yyvsp[-2].vop, yyvsp[-1].ival, yyvsp[0].ival);}
break;
case 91:
#line 307 "grammar.yy"
{parse_stmt(yyvsp[0].vop, type(INT));}
break;
case 92:
#line 309 "grammar.yy"
{parse_stmt(yyvsp[0].vop, type(INT));}
break;
case 93:
#line 311 "grammar.yy"
{parse_call(yyvsp[0].id);}
break;
case 94:
#line 313 "grammar.yy"
{parse_call("MAIN"); /* MAIN is a token, use string */}
break;
case 95:
#line 315 "grammar.yy"
{parse_if();}
break;
case 96:
#line 317 "grammar.yy"
{parse_else();}
break;
case 97:
#line 319 "grammar.yy"
{parse_endif();}
break;
case 98:
#line 321 "grammar.yy"
{parse_stmt(yyvsp[0].vop, type(INT));}
break;
case 99:
#line 323 "grammar.yy"
{
			constant.ival = yyvsp[0].ival;
			parse_const(yyvsp[-2].vop, type(yyvsp[-1].vop));
			}
break;
case 100:
#line 328 "grammar.yy"
{
			constant.ival = yyvsp[0].ival;
			parse_const(yyvsp[-2].vop, type(yyvsp[-1].vop));
			}
break;
case 101:
#line 333 "grammar.yy"
{
			constant.dval =  yyvsp[0].dval;
			parse_const(yyvsp[-2].vop, type(yyvsp[-1].vop));
			}
break;
case 102:
#line 338 "grammar.yy"
{
			constant.bval = yyvsp[0].ival;
			parse_const(yyvsp[-2].vop, type(yyvsp[-1].vop));
			}
break;
case 103:
#line 343 "grammar.yy"
{
			constant.ival = yyvsp[0].ival;
			parse_const(yyvsp[-2].vop, type(yyvsp[-1].vop));
			}
break;
case 104:
#line 348 "grammar.yy"
{
			}
break;
case 105:
#line 353 "grammar.yy"
{parse_end_vec();}
break;
case 106:
#line 355 "grammar.yy"
{
			parse_string(yyvsp[0].cval, yyleng);
			}
break;
case 107:
#line 360 "grammar.yy"
{parse_begin_vec(type(yyvsp[0].vop));}
break;
case 108:
#line 362 "grammar.yy"
{parse_begin_vec(type(yyvsp[0].vop));}
break;
case 109:
#line 364 "grammar.yy"
{parse_begin_vec(type(yyvsp[0].vop));}
break;
case 110:
#line 366 "grammar.yy"
{parse_begin_vec(type(yyvsp[0].vop));}
break;
case 116:
#line 379 "grammar.yy"
{
			constant.ival = yyval.ival;
			parse_val_vec(Int);
			}
break;
case 117:
#line 386 "grammar.yy"
{
			constant.dval =  yyval.dval;
			parse_val_vec(Float);
			}
break;
case 118:
#line 393 "grammar.yy"
{
			constant.bval = yyval.ival;
			parse_val_vec(Bool);
			}
break;
case 119:
#line 400 "grammar.yy"
{parse_stmt(yyvsp[0].vop, type(INT));}
break;
case 120:
#line 402 "grammar.yy"
{parse_stmt(yyvsp[0].vop, type(INT));}
break;
case 121:
#line 404 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 122:
#line 406 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 123:
#line 408 "grammar.yy"
{parse_stmt(yyvsp[0].vop, type(INT));}
break;
case 124:
#line 410 "grammar.yy"
{parse_stmt(yyvsp[0].vop, type(INT));}
break;
case 125:
#line 412 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 126:
#line 414 "grammar.yy"
{parse_stmt(yyvsp[0].vop, type(INT));}
break;
case 127:
#line 416 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(yyvsp[0].vop));}
break;
case 128:
#line 418 "grammar.yy"
{parse_stmt(yyvsp[0].vop, type(INT));}
break;
case 129:
#line 420 "grammar.yy"
{parse_stmt(yyvsp[-1].vop, type(INT));}
break;
case 130:
#line 424 "grammar.yy"
{yyval.ival = 1;}
break;
case 131:
#line 426 "grammar.yy"
{yyval.ival = 0;}
break;
case 132:
#line 430 "grammar.yy"
{yyval.ival = yyvsp[0].ival;}
break;
case 133:
#line 434 "grammar.yy"
{yyval.ival = STDIN_FD;}
break;
case 134:
#line 436 "grammar.yy"
{yyval.ival = STDOUT_FD;}
break;
case 135:
#line 438 "grammar.yy"
{yyval.ival = STDERR_FD;}
break;
case 136:
#line 440 "grammar.yy"
{yyval.ival = NULL_STREAM_FD;}
break;
#line 1266 "y.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;

yyoverflow:
    yyerror("yacc stack overflow");

yyabort:
    return (1);

yyaccept:
    return (0);
}
