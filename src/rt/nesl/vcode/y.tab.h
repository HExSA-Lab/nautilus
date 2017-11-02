/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_Y_TAB_H_INCLUDED
# define YY_YY_Y_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    PLUS = 258,
    MINUS = 259,
    TIMES = 260,
    DIV = 261,
    MOD = 262,
    LT = 263,
    LEQ = 264,
    GT = 265,
    GEQ = 266,
    EQ = 267,
    NEQ = 268,
    LSHIFT = 269,
    RSHIFT = 270,
    NOT = 271,
    AND = 272,
    OR = 273,
    XOR = 274,
    SELECT = 275,
    RAND = 276,
    FLOOR = 277,
    CEIL = 278,
    TRUNC = 279,
    ROUND = 280,
    LOG = 281,
    SQRT = 282,
    EXP = 283,
    SIN = 284,
    COS = 285,
    TAN = 286,
    ASIN = 287,
    ACOS = 288,
    ATAN = 289,
    SINH = 290,
    COSH = 291,
    TANH = 292,
    I_TO_F = 293,
    I_TO_B = 294,
    B_TO_I = 295,
    PLUS_SCAN = 296,
    MULT_SCAN = 297,
    MAX_SCAN = 298,
    MIN_SCAN = 299,
    AND_SCAN = 300,
    OR_SCAN = 301,
    XOR_SCAN = 302,
    PLUS_REDUCE = 303,
    MULT_REDUCE = 304,
    MAX_REDUCE = 305,
    MIN_REDUCE = 306,
    AND_REDUCE = 307,
    OR_REDUCE = 308,
    XOR_REDUCE = 309,
    PERMUTE = 310,
    DPERMUTE = 311,
    FPERMUTE = 312,
    BPERMUTE = 313,
    BFPERMUTE = 314,
    DFPERMUTE = 315,
    EXTRACT = 316,
    REPLACE = 317,
    DIST = 318,
    INDEX = 319,
    RANK_UP = 320,
    RANK_DOWN = 321,
    PACK = 322,
    LENGTH = 323,
    MAKE_SEGDES = 324,
    LENGTHS = 325,
    COPY = 326,
    POP = 327,
    CPOP = 328,
    PAIR = 329,
    UNPAIR = 330,
    CALL = 331,
    RET = 332,
    FUNC = 333,
    IF = 334,
    ELSE = 335,
    ENDIF = 336,
    CONST = 337,
    EXIT = 338,
    START_TIMER = 339,
    STOP_TIMER = 340,
    READ = 341,
    WRITE = 342,
    FOPEN = 343,
    FCLOSE = 344,
    FREAD = 345,
    FREAD_CHAR = 346,
    FWRITE = 347,
    SPAWN = 348,
    SRAND = 349,
    INT = 350,
    BOOL = 351,
    FLOAT = 352,
    SEGDES = 353,
    CHAR = 354,
    V_TRUE = 355,
    V_FALSE = 356,
    NULL_STREAM = 357,
    STDIN = 358,
    STDOUT = 359,
    STDERR = 360,
    MAIN = 361,
    BEGIN_VECTOR = 362,
    END_VECTOR = 363,
    INTEGER = 364,
    REAL = 365,
    STRING = 366,
    IDENTIFIER = 367,
    INPUT_INFO = 368,
    OUTPUT_INFO = 369
  };
#endif
/* Tokens.  */
#define PLUS 258
#define MINUS 259
#define TIMES 260
#define DIV 261
#define MOD 262
#define LT 263
#define LEQ 264
#define GT 265
#define GEQ 266
#define EQ 267
#define NEQ 268
#define LSHIFT 269
#define RSHIFT 270
#define NOT 271
#define AND 272
#define OR 273
#define XOR 274
#define SELECT 275
#define RAND 276
#define FLOOR 277
#define CEIL 278
#define TRUNC 279
#define ROUND 280
#define LOG 281
#define SQRT 282
#define EXP 283
#define SIN 284
#define COS 285
#define TAN 286
#define ASIN 287
#define ACOS 288
#define ATAN 289
#define SINH 290
#define COSH 291
#define TANH 292
#define I_TO_F 293
#define I_TO_B 294
#define B_TO_I 295
#define PLUS_SCAN 296
#define MULT_SCAN 297
#define MAX_SCAN 298
#define MIN_SCAN 299
#define AND_SCAN 300
#define OR_SCAN 301
#define XOR_SCAN 302
#define PLUS_REDUCE 303
#define MULT_REDUCE 304
#define MAX_REDUCE 305
#define MIN_REDUCE 306
#define AND_REDUCE 307
#define OR_REDUCE 308
#define XOR_REDUCE 309
#define PERMUTE 310
#define DPERMUTE 311
#define FPERMUTE 312
#define BPERMUTE 313
#define BFPERMUTE 314
#define DFPERMUTE 315
#define EXTRACT 316
#define REPLACE 317
#define DIST 318
#define INDEX 319
#define RANK_UP 320
#define RANK_DOWN 321
#define PACK 322
#define LENGTH 323
#define MAKE_SEGDES 324
#define LENGTHS 325
#define COPY 326
#define POP 327
#define CPOP 328
#define PAIR 329
#define UNPAIR 330
#define CALL 331
#define RET 332
#define FUNC 333
#define IF 334
#define ELSE 335
#define ENDIF 336
#define CONST 337
#define EXIT 338
#define START_TIMER 339
#define STOP_TIMER 340
#define READ 341
#define WRITE 342
#define FOPEN 343
#define FCLOSE 344
#define FREAD 345
#define FREAD_CHAR 346
#define FWRITE 347
#define SPAWN 348
#define SRAND 349
#define INT 350
#define BOOL 351
#define FLOAT 352
#define SEGDES 353
#define CHAR 354
#define V_TRUE 355
#define V_FALSE 356
#define NULL_STREAM 357
#define STDIN 358
#define STDOUT 359
#define STDERR 360
#define MAIN 361
#define BEGIN_VECTOR 362
#define END_VECTOR 363
#define INTEGER 364
#define REAL 365
#define STRING 366
#define IDENTIFIER 367
#define INPUT_INFO 368
#define OUTPUT_INFO 369

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 34 "grammar.yy" /* yacc.c:1909  */

    int ival;
    double dval;
    char *cval;
    char *id;
    int vop;

#line 290 "y.tab.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_Y_TAB_H_INCLUDED  */
