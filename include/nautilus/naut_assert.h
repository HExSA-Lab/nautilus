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
 * http://xstack.sandia.gov/hobbes
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
#ifndef __NAUT_ASSERT_H__
#define __NAUT_ASSERT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/nautilus.h>

extern void serial_print_redirect(const char * format, ...);


#ifdef NAUT_CONFIG_ENABLE_ASSERTS
#define ASSERT(cond)                   \
do {                            \
    if (!(cond)) {                  \
    panic("Failed assertion in %s: %s at %s, line %d, RA=%lx\n",\
        __func__, #cond, __FILE__, __LINE__,    \
        (ulong_t) __builtin_return_address(0));  \
    while (1)                   \
       ;                        \
    }                           \
} while (0)
#else 
#define ASSERT(cond)
#endif

#ifdef __cplusplus
}
#endif

#endif
