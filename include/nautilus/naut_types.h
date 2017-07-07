/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __NAUT_TYPES_H__
#define __NAUT_TYPES_H__

#ifdef __cplusplus 
extern "C" {
#endif

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
typedef unsigned long off_t;

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

typedef unsigned long  uintptr_t;
typedef long           intptr_t;

//#define NULL ((void *)0)
#ifndef NULL
#define NULL 0
#endif
#define FALSE 0
#define TRUE 1

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#ifdef __cplusplus
}
#endif

#endif
