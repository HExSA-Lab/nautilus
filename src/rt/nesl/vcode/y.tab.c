/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 1 "grammar.yy" /* yacc.c:339  */

/*
* Copyright (c) 1992, 1993, 1994 Carnegie Mellon University 
*/
/*
 * grammar.yy: Yacc source file for analyzing vcode source
 */

#include "config.h"
#include "vcode.h"
#include "parse.h"

//#define YYERROR_VERBOSE    1
#define YYSTACK_USE_ALLOCA 1


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


#line 98 "y.tab.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

#define YYDEBUG 1
/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
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
#line 34 "grammar.yy" /* yacc.c:355  */

    int ival;
    double dval;
    char *cval;
    char *id;
    int vop;

#line 374 "y.tab.c" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_Y_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 391 "y.tab.c" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  8
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   190

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  115
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  30
/* YYNRULES -- Number of rules.  */
#define YYNRULES  149
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  213

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   369

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   101,   101,   102,   105,   108,   111,   118,   131,   132,
     136,   137,   140,   143,   147,   148,   151,   152,   153,   154,
     155,   158,   160,   162,   164,   166,   168,   170,   172,   174,
     176,   178,   180,   182,   184,   186,   188,   190,   192,   194,
     196,   198,   200,   202,   204,   206,   208,   210,   212,   214,
     216,   218,   220,   222,   224,   226,   228,   230,   232,   236,
     238,   240,   242,   244,   246,   248,   250,   252,   254,   256,
     258,   260,   262,   264,   266,   268,   270,   272,   274,   276,
     278,   280,   282,   284,   286,   288,   290,   294,   296,   300,
     302,   304,   306,   308,   310,   312,   314,   316,   318,   320,
     322,   327,   332,   337,   342,   347,   352,   354,   359,   361,
     363,   365,   369,   370,   373,   374,   375,   378,   385,   392,
     399,   401,   403,   405,   407,   409,   411,   413,   415,   417,
     419,   423,   425,   429,   433,   435,   437,   439,   443,   444,
     447,   448,   451,   452,   453,   456,   457,   458,   459,   460
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "PLUS", "MINUS", "TIMES", "DIV", "MOD",
  "LT", "LEQ", "GT", "GEQ", "EQ", "NEQ", "LSHIFT", "RSHIFT", "NOT", "AND",
  "OR", "XOR", "SELECT", "RAND", "FLOOR", "CEIL", "TRUNC", "ROUND", "LOG",
  "SQRT", "EXP", "SIN", "COS", "TAN", "ASIN", "ACOS", "ATAN", "SINH",
  "COSH", "TANH", "I_TO_F", "I_TO_B", "B_TO_I", "PLUS_SCAN", "MULT_SCAN",
  "MAX_SCAN", "MIN_SCAN", "AND_SCAN", "OR_SCAN", "XOR_SCAN", "PLUS_REDUCE",
  "MULT_REDUCE", "MAX_REDUCE", "MIN_REDUCE", "AND_REDUCE", "OR_REDUCE",
  "XOR_REDUCE", "PERMUTE", "DPERMUTE", "FPERMUTE", "BPERMUTE", "BFPERMUTE",
  "DFPERMUTE", "EXTRACT", "REPLACE", "DIST", "INDEX", "RANK_UP",
  "RANK_DOWN", "PACK", "LENGTH", "MAKE_SEGDES", "LENGTHS", "COPY", "POP",
  "CPOP", "PAIR", "UNPAIR", "CALL", "RET", "FUNC", "IF", "ELSE", "ENDIF",
  "CONST", "EXIT", "START_TIMER", "STOP_TIMER", "READ", "WRITE", "FOPEN",
  "FCLOSE", "FREAD", "FREAD_CHAR", "FWRITE", "SPAWN", "SRAND", "INT",
  "BOOL", "FLOAT", "SEGDES", "CHAR", "V_TRUE", "V_FALSE", "NULL_STREAM",
  "STDIN", "STDOUT", "STDERR", "MAIN", "BEGIN_VECTOR", "END_VECTOR",
  "INTEGER", "REAL", "STRING", "IDENTIFIER", "INPUT_INFO", "OUTPUT_INFO",
  "$accept", "program", "fn_def", "fn_decl", "fn_name", "fn_types",
  "type_decls", "type_decl", "fn_end", "stmts", "stmt", "elt_op", "vec_op",
  "seg_op", "ctl_op", "type_and_vector", "type_const", "vals", "val",
  "int_val", "float_val", "bool_val", "io_op", "bool_con", "int_con",
  "file_con", "types_1", "types_2", "types_3", "types_5", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369
};
# endif

#define YYPACT_NINF -16

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-16)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int8 yypact[] =
{
      19,    23,    75,   -16,   -16,     0,   -16,   -16,   -16,   -16,
      -3,   -16,   -16,    93,    93,    93,    93,   -16,    93,    93,
      93,    93,    93,    93,   -16,   -16,    14,    14,    14,    14,
      71,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,
     -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,
     -16,    93,    93,    93,    93,    14,    14,    14,    93,    93,
      93,    93,    14,    14,    14,    71,    71,    71,    71,    71,
      71,    71,    71,    71,   -16,    93,    93,    71,    71,   -16,
     -16,    13,    13,    13,   -16,   -16,    65,   -16,   -16,   -16,
     -16,    83,   -16,   -16,   -16,    88,    88,   -16,   -16,    88,
     -16,    88,   -16,    31,   -16,   -16,   -16,   -16,   -16,   -16,
     -16,     9,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,
     -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,
     -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,
     -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,
     -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,
     -16,   -16,    13,    13,    13,   -16,   -16,    43,    17,    26,
      18,    64,   -16,    30,   -16,   -16,   -16,   -16,   -16,   -16,
     -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,
     -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,
     -16,   -16,   -16,    88,    11,   -16,   -16,   -16,   -16,   -16,
     -16,   -16,   -16
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     2,    14,     8,     7,     6,     1,     3,
       0,    10,     5,     0,     0,     0,     0,    25,     0,     0,
       0,     0,     0,     0,    32,    33,     0,     0,     0,     0,
       0,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    80,     0,     0,     0,     0,    87,
      88,     0,     0,     0,    92,    93,     0,    13,    96,    97,
      98,     0,    99,   120,   121,     0,     0,   124,   125,     0,
     127,     0,   129,     0,     4,    15,    16,    17,    18,    19,
      20,     0,   138,   139,    21,    22,    23,    24,    26,    27,
      28,    29,    30,    31,   140,   141,    34,    35,    36,    37,
     142,   143,   144,    38,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    82,    83,    79,    84,    85,    86,
      81,   133,     0,     0,     0,    95,    94,   108,   109,   110,
     111,     0,   105,     0,   145,   146,   147,   148,   149,   122,
     123,   126,   128,   130,    10,    11,    12,    89,    90,    91,
     137,   134,   135,   136,   100,   101,   131,   132,   103,   102,
     104,   107,   112,     9,     0,   106,   117,   118,   113,   114,
     115,   116,   119
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -16,   -16,   152,   -16,   -16,   -16,    -8,   -16,   -16,   -16,
     -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,   -16,
     -16,   -16,   -16,   -15,    10,   -16,    80,    87,    92,    29
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,     3,     4,     5,    12,   111,   185,   104,    10,
     105,   106,   107,   108,   109,   172,   173,   204,   208,   209,
     210,   211,   110,   198,   162,   195,   114,   126,   133,   186
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,     8,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   163,   164,   115,   116,   117,     1,   118,   119,
     120,   121,   122,   123,   174,   175,   176,   177,   178,   124,
     125,   196,   197,    11,   127,   128,   129,   196,   197,   205,
     206,   207,   161,   184,   179,   180,   183,   200,   181,     6,
     182,   134,   135,   136,   137,     7,   199,   202,   141,   142,
     143,   144,   138,   139,   140,   190,   191,   192,   193,   145,
     146,   147,   194,     1,     9,   157,   158,   148,   149,   150,
     151,   152,   153,   154,   155,   156,   130,   131,   132,   159,
     160,   165,   187,   188,   189,   201,   203,   166,   167,   168,
     169,   170,   171,   174,   175,   176,   177,   178,   112,   212,
     113
};

static const yytype_uint8 yycheck[] =
{
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,     0,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    82,    83,    14,    15,    16,    78,    18,    19,
      20,    21,    22,    23,    95,    96,    97,    98,    99,    95,
      96,   100,   101,   113,    27,    28,    29,   100,   101,   108,
     109,   110,   109,   114,    95,    96,    95,   109,    99,   106,
     101,    51,    52,    53,    54,   112,   110,   107,    58,    59,
      60,    61,    55,    56,    57,   102,   103,   104,   105,    62,
      63,    64,   109,    78,     2,    75,    76,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    95,    96,    97,    77,
      78,   106,   162,   163,   164,   111,   184,   112,    95,    96,
      97,    98,    99,    95,    96,    97,    98,    99,    95,   204,
      97
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    78,   116,   117,   118,   119,   106,   112,     0,   117,
     124,   113,   120,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,   123,   125,   126,   127,   128,   129,
     137,   121,    95,    97,   141,   141,   141,   141,   141,   141,
     141,   141,   141,   141,    95,    96,   142,   142,   142,   142,
      95,    96,    97,   143,   141,   141,   141,   141,   142,   142,
     142,   141,   141,   141,   141,   142,   142,   142,   143,   143,
     143,   143,   143,   143,   143,   143,   143,   141,   141,   143,
     143,   109,   139,   139,   139,   106,   112,    95,    96,    97,
      98,    99,   130,   131,    95,    96,    97,    98,    99,   144,
     144,   144,   144,    95,   114,   122,   144,   139,   139,   139,
     102,   103,   104,   105,   109,   140,   100,   101,   138,   110,
     109,   111,   107,   121,   132,   108,   109,   110,   133,   134,
     135,   136,   138
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,   115,   116,   116,   117,   118,   119,   119,   120,   120,
     121,   121,   122,   123,   124,   124,   125,   125,   125,   125,
     125,   126,   126,   126,   126,   126,   126,   126,   126,   126,
     126,   126,   126,   126,   126,   126,   126,   126,   126,   126,
     126,   126,   126,   126,   126,   126,   126,   126,   126,   126,
     126,   126,   126,   126,   126,   126,   126,   126,   126,   127,
     127,   127,   127,   127,   127,   127,   127,   127,   127,   127,
     127,   127,   127,   127,   127,   127,   127,   127,   127,   127,
     127,   127,   127,   127,   127,   127,   127,   128,   128,   129,
     129,   129,   129,   129,   129,   129,   129,   129,   129,   129,
     129,   129,   129,   129,   129,   129,   130,   130,   131,   131,
     131,   131,   132,   132,   133,   133,   133,   134,   135,   136,
     137,   137,   137,   137,   137,   137,   137,   137,   137,   137,
     137,   138,   138,   139,   140,   140,   140,   140,   141,   141,
     142,   142,   143,   143,   143,   144,   144,   144,   144,   144
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     2,     3,     2,     2,     2,     0,     4,
       0,     2,     1,     1,     0,     2,     1,     1,     1,     1,
       1,     2,     2,     2,     2,     1,     2,     2,     2,     2,
       2,     2,     1,     1,     2,     2,     2,     2,     2,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       1,     2,     2,     2,     2,     2,     2,     1,     1,     3,
       3,     3,     1,     1,     2,     2,     1,     1,     1,     1,
       3,     3,     3,     3,     3,     2,     4,     2,     1,     1,
       1,     1,     0,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     2,     1,     1,     2,     1,     2,     1,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#ifndef __NAUTILUS__
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#endif
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

// yes, this is "bad", but we don't want this warning polluting
// more important things
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wstrict-overflow"
  if (yyss + yystacksize - 1 <= yyssp)
//#pragma GCC diagnostic pop
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 6:
#line 112 "grammar.yy" /* yacc.c:1646  */
    { if (first_function) {/* first function */
			    strcpy(main_fn, (yyvsp[0].id));
			    first_function = 0;
			  }
			  parse_label((yyvsp[0].id));
			}
#line 1649 "y.tab.c" /* yacc.c:1646  */
    break;

  case 7:
#line 119 "grammar.yy" /* yacc.c:1646  */
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
#line 1664 "y.tab.c" /* yacc.c:1646  */
    break;

  case 13:
#line 144 "grammar.yy" /* yacc.c:1646  */
    {parse_return();}
#line 1670 "y.tab.c" /* yacc.c:1646  */
    break;

  case 21:
#line 159 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1676 "y.tab.c" /* yacc.c:1646  */
    break;

  case 22:
#line 161 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1682 "y.tab.c" /* yacc.c:1646  */
    break;

  case 23:
#line 163 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1688 "y.tab.c" /* yacc.c:1646  */
    break;

  case 24:
#line 165 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1694 "y.tab.c" /* yacc.c:1646  */
    break;

  case 25:
#line 167 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Int);}
#line 1700 "y.tab.c" /* yacc.c:1646  */
    break;

  case 26:
#line 169 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1706 "y.tab.c" /* yacc.c:1646  */
    break;

  case 27:
#line 171 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1712 "y.tab.c" /* yacc.c:1646  */
    break;

  case 28:
#line 173 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1718 "y.tab.c" /* yacc.c:1646  */
    break;

  case 29:
#line 175 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1724 "y.tab.c" /* yacc.c:1646  */
    break;

  case 30:
#line 177 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1730 "y.tab.c" /* yacc.c:1646  */
    break;

  case 31:
#line 179 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1736 "y.tab.c" /* yacc.c:1646  */
    break;

  case 32:
#line 181 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Int);}
#line 1742 "y.tab.c" /* yacc.c:1646  */
    break;

  case 33:
#line 183 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Int);}
#line 1748 "y.tab.c" /* yacc.c:1646  */
    break;

  case 34:
#line 185 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1754 "y.tab.c" /* yacc.c:1646  */
    break;

  case 35:
#line 187 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1760 "y.tab.c" /* yacc.c:1646  */
    break;

  case 36:
#line 189 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1766 "y.tab.c" /* yacc.c:1646  */
    break;

  case 37:
#line 191 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1772 "y.tab.c" /* yacc.c:1646  */
    break;

  case 38:
#line 193 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1778 "y.tab.c" /* yacc.c:1646  */
    break;

  case 39:
#line 195 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Int);}
#line 1784 "y.tab.c" /* yacc.c:1646  */
    break;

  case 40:
#line 197 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1790 "y.tab.c" /* yacc.c:1646  */
    break;

  case 41:
#line 199 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1796 "y.tab.c" /* yacc.c:1646  */
    break;

  case 42:
#line 201 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1802 "y.tab.c" /* yacc.c:1646  */
    break;

  case 43:
#line 203 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1808 "y.tab.c" /* yacc.c:1646  */
    break;

  case 44:
#line 205 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1814 "y.tab.c" /* yacc.c:1646  */
    break;

  case 45:
#line 207 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1820 "y.tab.c" /* yacc.c:1646  */
    break;

  case 46:
#line 209 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1826 "y.tab.c" /* yacc.c:1646  */
    break;

  case 47:
#line 211 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1832 "y.tab.c" /* yacc.c:1646  */
    break;

  case 48:
#line 213 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1838 "y.tab.c" /* yacc.c:1646  */
    break;

  case 49:
#line 215 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1844 "y.tab.c" /* yacc.c:1646  */
    break;

  case 50:
#line 217 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1850 "y.tab.c" /* yacc.c:1646  */
    break;

  case 51:
#line 219 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1856 "y.tab.c" /* yacc.c:1646  */
    break;

  case 52:
#line 221 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1862 "y.tab.c" /* yacc.c:1646  */
    break;

  case 53:
#line 223 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1868 "y.tab.c" /* yacc.c:1646  */
    break;

  case 54:
#line 225 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1874 "y.tab.c" /* yacc.c:1646  */
    break;

  case 55:
#line 227 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Float);}
#line 1880 "y.tab.c" /* yacc.c:1646  */
    break;

  case 56:
#line 229 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Int);}
#line 1886 "y.tab.c" /* yacc.c:1646  */
    break;

  case 57:
#line 231 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Int);}
#line 1892 "y.tab.c" /* yacc.c:1646  */
    break;

  case 58:
#line 233 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Bool);}
#line 1898 "y.tab.c" /* yacc.c:1646  */
    break;

  case 59:
#line 237 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1904 "y.tab.c" /* yacc.c:1646  */
    break;

  case 60:
#line 239 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1910 "y.tab.c" /* yacc.c:1646  */
    break;

  case 61:
#line 241 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1916 "y.tab.c" /* yacc.c:1646  */
    break;

  case 62:
#line 243 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1922 "y.tab.c" /* yacc.c:1646  */
    break;

  case 63:
#line 245 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1928 "y.tab.c" /* yacc.c:1646  */
    break;

  case 64:
#line 247 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1934 "y.tab.c" /* yacc.c:1646  */
    break;

  case 65:
#line 249 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1940 "y.tab.c" /* yacc.c:1646  */
    break;

  case 66:
#line 251 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1946 "y.tab.c" /* yacc.c:1646  */
    break;

  case 67:
#line 253 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1952 "y.tab.c" /* yacc.c:1646  */
    break;

  case 68:
#line 255 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1958 "y.tab.c" /* yacc.c:1646  */
    break;

  case 69:
#line 257 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1964 "y.tab.c" /* yacc.c:1646  */
    break;

  case 70:
#line 259 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1970 "y.tab.c" /* yacc.c:1646  */
    break;

  case 71:
#line 261 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1976 "y.tab.c" /* yacc.c:1646  */
    break;

  case 72:
#line 263 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1982 "y.tab.c" /* yacc.c:1646  */
    break;

  case 73:
#line 265 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1988 "y.tab.c" /* yacc.c:1646  */
    break;

  case 74:
#line 267 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 1994 "y.tab.c" /* yacc.c:1646  */
    break;

  case 75:
#line 269 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2000 "y.tab.c" /* yacc.c:1646  */
    break;

  case 76:
#line 271 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2006 "y.tab.c" /* yacc.c:1646  */
    break;

  case 77:
#line 273 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2012 "y.tab.c" /* yacc.c:1646  */
    break;

  case 78:
#line 275 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2018 "y.tab.c" /* yacc.c:1646  */
    break;

  case 79:
#line 277 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2024 "y.tab.c" /* yacc.c:1646  */
    break;

  case 80:
#line 279 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Int);}
#line 2030 "y.tab.c" /* yacc.c:1646  */
    break;

  case 81:
#line 281 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2036 "y.tab.c" /* yacc.c:1646  */
    break;

  case 82:
#line 283 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2042 "y.tab.c" /* yacc.c:1646  */
    break;

  case 83:
#line 285 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2048 "y.tab.c" /* yacc.c:1646  */
    break;

  case 84:
#line 287 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2054 "y.tab.c" /* yacc.c:1646  */
    break;

  case 85:
#line 289 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2060 "y.tab.c" /* yacc.c:1646  */
    break;

  case 86:
#line 291 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2066 "y.tab.c" /* yacc.c:1646  */
    break;

  case 87:
#line 295 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Segdes);}
#line 2072 "y.tab.c" /* yacc.c:1646  */
    break;

  case 88:
#line 297 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), Int);}
#line 2078 "y.tab.c" /* yacc.c:1646  */
    break;

  case 89:
#line 301 "grammar.yy" /* yacc.c:1646  */
    {parse_stack((yyvsp[-2].vop), (yyvsp[-1].ival), (yyvsp[0].ival));}
#line 2084 "y.tab.c" /* yacc.c:1646  */
    break;

  case 90:
#line 303 "grammar.yy" /* yacc.c:1646  */
    {parse_stack((yyvsp[-2].vop), (yyvsp[-1].ival), (yyvsp[0].ival));}
#line 2090 "y.tab.c" /* yacc.c:1646  */
    break;

  case 91:
#line 305 "grammar.yy" /* yacc.c:1646  */
    {parse_stack((yyvsp[-2].vop), (yyvsp[-1].ival), (yyvsp[0].ival));}
#line 2096 "y.tab.c" /* yacc.c:1646  */
    break;

  case 92:
#line 307 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), type(INT));}
#line 2102 "y.tab.c" /* yacc.c:1646  */
    break;

  case 93:
#line 309 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), type(INT));}
#line 2108 "y.tab.c" /* yacc.c:1646  */
    break;

  case 94:
#line 311 "grammar.yy" /* yacc.c:1646  */
    {parse_call((yyvsp[0].id));}
#line 2114 "y.tab.c" /* yacc.c:1646  */
    break;

  case 95:
#line 313 "grammar.yy" /* yacc.c:1646  */
    {parse_call("MAIN"); /* MAIN is a token, use string */}
#line 2120 "y.tab.c" /* yacc.c:1646  */
    break;

  case 96:
#line 315 "grammar.yy" /* yacc.c:1646  */
    {parse_if();}
#line 2126 "y.tab.c" /* yacc.c:1646  */
    break;

  case 97:
#line 317 "grammar.yy" /* yacc.c:1646  */
    {parse_else();}
#line 2132 "y.tab.c" /* yacc.c:1646  */
    break;

  case 98:
#line 319 "grammar.yy" /* yacc.c:1646  */
    {parse_endif();}
#line 2138 "y.tab.c" /* yacc.c:1646  */
    break;

  case 99:
#line 321 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), type(INT));}
#line 2144 "y.tab.c" /* yacc.c:1646  */
    break;

  case 100:
#line 323 "grammar.yy" /* yacc.c:1646  */
    {
			constant.ival = (yyvsp[0].ival);
			parse_const((yyvsp[-2].vop), type((yyvsp[-1].vop)));
			}
#line 2153 "y.tab.c" /* yacc.c:1646  */
    break;

  case 101:
#line 328 "grammar.yy" /* yacc.c:1646  */
    {
			constant.ival = (yyvsp[0].ival);
			parse_const((yyvsp[-2].vop), type((yyvsp[-1].vop)));
			}
#line 2162 "y.tab.c" /* yacc.c:1646  */
    break;

  case 102:
#line 333 "grammar.yy" /* yacc.c:1646  */
    {
			constant.dval =  (yyvsp[0].dval);
			parse_const((yyvsp[-2].vop), type((yyvsp[-1].vop)));
			}
#line 2171 "y.tab.c" /* yacc.c:1646  */
    break;

  case 103:
#line 338 "grammar.yy" /* yacc.c:1646  */
    {
			constant.bval = (yyvsp[0].ival);
			parse_const((yyvsp[-2].vop), type((yyvsp[-1].vop)));
			}
#line 2180 "y.tab.c" /* yacc.c:1646  */
    break;

  case 104:
#line 343 "grammar.yy" /* yacc.c:1646  */
    {
			constant.ival = (yyvsp[0].ival);
			parse_const((yyvsp[-2].vop), type((yyvsp[-1].vop)));
			}
#line 2189 "y.tab.c" /* yacc.c:1646  */
    break;

  case 105:
#line 348 "grammar.yy" /* yacc.c:1646  */
    {
			}
#line 2196 "y.tab.c" /* yacc.c:1646  */
    break;

  case 106:
#line 353 "grammar.yy" /* yacc.c:1646  */
    {parse_end_vec();}
#line 2202 "y.tab.c" /* yacc.c:1646  */
    break;

  case 107:
#line 355 "grammar.yy" /* yacc.c:1646  */
    {
			parse_string((yyvsp[0].cval), yyleng);
			}
#line 2210 "y.tab.c" /* yacc.c:1646  */
    break;

  case 108:
#line 360 "grammar.yy" /* yacc.c:1646  */
    {parse_begin_vec(type((yyvsp[0].vop)));}
#line 2216 "y.tab.c" /* yacc.c:1646  */
    break;

  case 109:
#line 362 "grammar.yy" /* yacc.c:1646  */
    {parse_begin_vec(type((yyvsp[0].vop)));}
#line 2222 "y.tab.c" /* yacc.c:1646  */
    break;

  case 110:
#line 364 "grammar.yy" /* yacc.c:1646  */
    {parse_begin_vec(type((yyvsp[0].vop)));}
#line 2228 "y.tab.c" /* yacc.c:1646  */
    break;

  case 111:
#line 366 "grammar.yy" /* yacc.c:1646  */
    {parse_begin_vec(type((yyvsp[0].vop)));}
#line 2234 "y.tab.c" /* yacc.c:1646  */
    break;

  case 117:
#line 379 "grammar.yy" /* yacc.c:1646  */
    {
			constant.ival = (yyval.ival);
			parse_val_vec(Int);
			}
#line 2243 "y.tab.c" /* yacc.c:1646  */
    break;

  case 118:
#line 386 "grammar.yy" /* yacc.c:1646  */
    {
			constant.dval =  (yyval.dval);
			parse_val_vec(Float);
			}
#line 2252 "y.tab.c" /* yacc.c:1646  */
    break;

  case 119:
#line 393 "grammar.yy" /* yacc.c:1646  */
    {
			constant.bval = (yyval.ival);
			parse_val_vec(Bool);
			}
#line 2261 "y.tab.c" /* yacc.c:1646  */
    break;

  case 120:
#line 400 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), type(INT));}
#line 2267 "y.tab.c" /* yacc.c:1646  */
    break;

  case 121:
#line 402 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), type(INT));}
#line 2273 "y.tab.c" /* yacc.c:1646  */
    break;

  case 122:
#line 404 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2279 "y.tab.c" /* yacc.c:1646  */
    break;

  case 123:
#line 406 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2285 "y.tab.c" /* yacc.c:1646  */
    break;

  case 124:
#line 408 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), type(INT));}
#line 2291 "y.tab.c" /* yacc.c:1646  */
    break;

  case 125:
#line 410 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), type(INT));}
#line 2297 "y.tab.c" /* yacc.c:1646  */
    break;

  case 126:
#line 412 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2303 "y.tab.c" /* yacc.c:1646  */
    break;

  case 127:
#line 414 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), type(INT));}
#line 2309 "y.tab.c" /* yacc.c:1646  */
    break;

  case 128:
#line 416 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type((yyvsp[0].vop)));}
#line 2315 "y.tab.c" /* yacc.c:1646  */
    break;

  case 129:
#line 418 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[0].vop), type(INT));}
#line 2321 "y.tab.c" /* yacc.c:1646  */
    break;

  case 130:
#line 420 "grammar.yy" /* yacc.c:1646  */
    {parse_stmt((yyvsp[-1].vop), type(INT));}
#line 2327 "y.tab.c" /* yacc.c:1646  */
    break;

  case 131:
#line 424 "grammar.yy" /* yacc.c:1646  */
    {(yyval.ival) = 1;}
#line 2333 "y.tab.c" /* yacc.c:1646  */
    break;

  case 132:
#line 426 "grammar.yy" /* yacc.c:1646  */
    {(yyval.ival) = 0;}
#line 2339 "y.tab.c" /* yacc.c:1646  */
    break;

  case 133:
#line 430 "grammar.yy" /* yacc.c:1646  */
    {(yyval.ival) = (yyvsp[0].ival);}
#line 2345 "y.tab.c" /* yacc.c:1646  */
    break;

  case 134:
#line 434 "grammar.yy" /* yacc.c:1646  */
    {(yyval.ival) = STDIN_FD;}
#line 2351 "y.tab.c" /* yacc.c:1646  */
    break;

  case 135:
#line 436 "grammar.yy" /* yacc.c:1646  */
    {(yyval.ival) = STDOUT_FD;}
#line 2357 "y.tab.c" /* yacc.c:1646  */
    break;

  case 136:
#line 438 "grammar.yy" /* yacc.c:1646  */
    {(yyval.ival) = STDERR_FD;}
#line 2363 "y.tab.c" /* yacc.c:1646  */
    break;

  case 137:
#line 440 "grammar.yy" /* yacc.c:1646  */
    {(yyval.ival) = NULL_STREAM_FD;}
#line 2369 "y.tab.c" /* yacc.c:1646  */
    break;


#line 2373 "y.tab.c" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa) {
    YYSTACK_FREE (yyss);
  }
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf) {
    YYSTACK_FREE (yymsg);
  }
#endif
  return yyresult;
}
