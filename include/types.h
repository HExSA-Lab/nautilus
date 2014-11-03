#ifndef __TYPES_H__
#define __TYPES_H__

typedef signed char   schar_t;
typedef unsigned char uchar_t;

typedef signed short   sshort_t;
typedef unsigned short ushort_t;

typedef signed int   sint_t;
typedef unsigned int uint_t;

typedef signed long long   sllong_t;
typedef unsigned long long ullong_t;

typedef signed long   slong_t;
typedef unsigned long ulong_t;

typedef unsigned long size_t;
typedef long          ssize_t;

//typedef unsigned long long uint64_t;
typedef unsigned long uint64_t;
//typedef long long          sint64_t;
typedef long sint64_t;

typedef unsigned int uint32_t;
typedef int          sint32_t;


typedef unsigned short uint16_t;
typedef short          sint16_t;

typedef unsigned char uint8_t;
typedef char          sint8_t;

typedef ulong_t addr_t;
typedef uchar_t bool_t;

//#define NULL ((void *)0)
#ifndef NULL
#define NULL 0
#endif
#define FALSE 0
#define TRUE 1

#endif
