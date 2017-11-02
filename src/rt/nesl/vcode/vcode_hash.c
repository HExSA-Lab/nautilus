/* C code produced by gperf version 2.5 (GNU C++ version) */
/* Command-line: gperf -ptT -K funct_string -D -N vcode_lookup  */
/*
* Copyright (c) 1992, 1993, 1994, 1995
* Carnegie Mellon University SCAL project:
* Guy Blelloch, Jonathan Hardwick, Jay Sipelstein, Marco Zagha
*
* All Rights Reserved.
*
* Permission to use, copy, modify and distribute this software and its
* documentation is hereby granted, provided that both the copyright
* notice and this permission notice appear in all copies of the
* software, derivative works or modified versions, and any portions
* thereof.
*
* CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
* CONDITION.  CARNEGIE MELLON AND THE SCAL PROJECT DISCLAIMS ANY 
* LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM 
* THE USE OF THIS SOFTWARE.
*
* The SCAL project requests users of this software to return to 
*
*  Guy Blelloch				nesl-contribute@cs.cmu.edu
*  School of Computer Science
*  Carnegie Mellon University
*  5000 Forbes Ave.
*  Pittsburgh PA 15213-3890
*
* any improvements or extensions that they make and grant Carnegie Mellon
* the rights to redistribute these changes.
*/

/* This file contains a list of all the VCODE tokens.  The lexer uses the
 * hash function whenever it encouters an identifer and returns the token
 * of the identifier found.
 */

#include "config.h"
#include "vcode.h"
#include "y.tab.h"
#include "parse.h"
#include "vcode_hash.h"

#define TOTAL_KEYWORDS 104
#define MIN_WORD_LENGTH 1
#define MAX_WORD_LENGTH 11
#define MIN_HASH_VALUE 1
#define MAX_HASH_VALUE 251
/* maximum key range = 251, duplicates = 12 */

static unsigned int
hash (str, len)
     register char *str;
     register int unsigned len;
{
  static unsigned char asso_values[] =
    {
     252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
     252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
     252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
     252, 252, 252,  15, 252, 252, 252,  40, 252, 252,
     252, 252,  70,  60, 252,   5, 252,  15, 252, 252,
     252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
      35,  10,  65, 252, 252, 102,  55,   5,  97,   0,
       0,  20,  30,  30, 252,  15,  65,  30,  10, 110,
      50, 252,  80,  20, 125,  10, 252,   5,  90,  20,
     252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
     252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
     252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
     252, 252, 252, 252, 252, 252, 252, 252,
    };
  return len + asso_values[(int)(str[len - 1])] + asso_values[(int)(str[0])];
}

vcode_com_t *
vcode_lookup (str, len)
     register char *str;
     register unsigned int len;
{
  static vcode_com_t wordlist[] =
    {
      {"",}, 
      {"F",  V_FALSE},
      {"ELSE",  ELSE},
      {"ENDIF",  ENDIF},
      {"FCLOSE",  FCLOSE},
      {"FWRITE",  FWRITE},
      {"FPERMUTE",  FPERMUTE},
      {"FUNC",  FUNC},
      {"WRITE",  WRITE},
      {"-",  MINUS},
      {"FOPEN",  FOPEN},
      {"=",  EQ},
      {"!=",  NEQ},
      {"COS",  COS},
      {"COPY",  COPY},
      {"/",  DIV},
      {"IF",  IF},
      {"SIN",  SIN},
      {"SPAWN",  SPAWN},
      {"STDIN",  STDIN},
      {"I_TO_F",  I_TO_F},
      {"COSH",  COSH},
      {"MAX_REDUCE",  MAX_REDUCE},
      {"MIN_REDUCE",  MIN_REDUCE},
      {"MAIN",  MAIN},
      {"SEGDES",  SEGDES},
      {"<=",  LEQ},
      {"MAX_SCAN",  MAX_SCAN},
      {"MIN_SCAN",  MIN_SCAN},
      {"NULL_STREAM",  NULL_STREAM},
      {"EXP",  EXP},
      {"SINH",  SINH},
      {"PERMUTE",  PERMUTE},
      {"CPOP",  CPOP},
      {"MAKE_SEGDES",  MAKE_SEGDES},
      {"BPERMUTE",  BPERMUTE},
      {"BFPERMUTE",  BFPERMUTE},
      {"+_REDUCE",  PLUS_REDUCE},
      {"PACK",  PACK},
      {"<",  LT},
      {"CEIL",  CEIL},
      {"CALL",  CALL},
      {"+_SCAN",  PLUS_SCAN},
      {">=",  GEQ},
      {"*_REDUCE",  MULT_REDUCE},
      {"%",  MOD},
      {"FLOOR",  FLOOR},
      {"*_SCAN",  MULT_SCAN},
      {"REPLACE",  REPLACE},
      {"LOG",  LOG},
      {"CHAR",  CHAR},
      {"FREAD_CHAR",  FREAD_CHAR},
      {"I_TO_B",  I_TO_B},
      {"B_TO_I",  B_TO_I},
      {"LENGTHS",  LENGTHS},
      {"UNPAIR",  UNPAIR},
      {"RANK_DOWN",  RANK_DOWN},
      {"XOR_REDUCE",  XOR_REDUCE},
      {"LENGTH",  LENGTH},
      {"FREAD",  FREAD},
      {"POP",  POP},
      {"DPERMUTE",  DPERMUTE},
      {"DFPERMUTE",  DFPERMUTE},
      {"STDERR",  STDERR},
      {"XOR_SCAN",  XOR_SCAN},
      {"STOP_TIMER",  STOP_TIMER},
      {"START_TIMER",  START_TIMER},
      {"AND_REDUCE",  AND_REDUCE},
      {"ASIN",  ASIN},
      {"ATAN",  ATAN},
      {"OR_REDUCE",  OR_REDUCE},
      {"AND_SCAN",  AND_SCAN},
      {"+",  PLUS},
      {"SRAND",  SRAND},
      {"BOOL",  BOOL},
      {"INDEX",  INDEX},
      {"ACOS",  ACOS},
      {"OR_SCAN",  OR_SCAN},
      {"EXIT",  EXIT},
      {"FLOAT",  FLOAT},
      {">",  GT},
      {"EXTRACT",  EXTRACT},
      {"PAIR",  PAIR},
      {"TRUNC",  TRUNC},
      {"CONST",  CONST},
      {"RANK_UP",  RANK_UP},
      {"NOT",  NOT},
      {"TAN",  TAN},
      {"*",  TIMES},
      {"SQRT",  SQRT},
      {"SELECT",  SELECT},
      {"STDOUT",  STDOUT},
      {"INT",  INT},
      {"TANH",  TANH},
      {"XOR",  XOR},
      {"RAND",  RAND},
      {"READ",  READ},
      {"ROUND",  ROUND},
      {"OR",  OR},
      {"LSHIFT",  LSHIFT},
      {"AND",  AND},
      {"RET",  RET},
      {"RSHIFT",  RSHIFT},
      {"DIST",  DIST},
      {"T",  V_TRUE},
    };

  static short lookup[] =
    {
        -1,   1,  -1,  -1,   2,   3, 257,  -1,   6,   7,   8,   9,  -4,  -2,
        -1,  10,  -1,  -1,  -1,  -1,  -1,  11,  -1,  -1,  -1,  -1,  -1,  12,
        13,  14,  -1,  15,  16,  17,  -1, 253,  20, -18,  -2,  21, 252, -22,
        -2,  -1,  24,  -1,  25,  26, 252, -27,  -2,  29,  -1,  30,  31,  -1,
        -1,  32,  -1,  33,  -1,  34,  -1,  35,  36,  -1,  -1,  -1,  37,  38,
        -1,  39,  -1,  -1, 256,  -1,  42,  43,  44, -40,  -2,  45,  -1,  -1,
        -1,  46,  47,  48,  49,  50,  51, 253,  54, -52,  -2,  -1,  55,  -1,
        -1,  56,  57,  58,  59,  60,  -1,  61, 258,  -1,  64,  -1,  65,  66,
        67, -62,  -2,  -1, 252, -68,  -2,  70,  71,  72,  73,  -1,  74,  75,
        76,  77,  -1,  78,  79,  80,  81,  -1,  82, 258,  -1,  85, 252, -86,
        -2,  88, -83,  -2,  -1,  -1,  -1,  -1,  -1,  89,  -1, 252, -90,  -2,
        -1,  -1,  -1,  -1,  92,  93,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        -1,  -1,  -1,  -1,  -1,  94,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 253,
        97, -95,  -2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  98,  -1,  -1,  -1,
        99,  -1,  -1,  -1,  -1,  -1, 100,  -1,  -1,  -1,  -1,  -1, 101,  -1,
        -1, 102,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        -1,  -1, 103,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 104,
      
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register int index = lookup[key];

          if (index >= 0 && index < MAX_HASH_VALUE)
            {
              register char *s = wordlist[index].funct_string;

              if (*s == *str && !strcmp (str + 1, s + 1))
                return &wordlist[index];
            }
          else if (index < 0 && index >= -MAX_HASH_VALUE)
            return 0;
          else
            {
              register int offset = key + index + (index > 0 ? -MAX_HASH_VALUE : MAX_HASH_VALUE);
              register vcode_com_t *base = &wordlist[-lookup[offset]];
              register vcode_com_t *ptr = base + -lookup[offset + 1];

              while (--ptr >= base)
                if (*str == *ptr->funct_string && !strcmp (str + 1, ptr->funct_string + 1))
                  return ptr;
            }
        }
    }
  return 0;
}

VOPCODE lookup_id(id)
char *id;
{
    vcode_com_t *result;

    result = vcode_lookup(id , strlen(id));
    if (result == NULL)
	return IDENTIFIER;
    else return result->vop;
}
