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
#include <nautilus/nautilus.h>

#ifndef NAUT_CONFIG_DEBUG_CXX
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define CXX_DEBUG(fmt, args...) DEBUG_PRINT("C++ INIT: " fmt, ##args)
#define CXX_PRINT(fmt, args...) printk("C++ INIT:" fmt, ##args)

/* for global constructors */
extern void (*_init_array_start []) (void) __attribute__((weak));
extern void (*_init_array_end []) (void) __attribute__((weak));

static void 
__do_ctors_init (void) 
{
    void (**p)(void) = NULL;

    for (p = _init_array_start; p < _init_array_end; p++) {
        if (*p) {
            CXX_DEBUG("Calling static constructor (%p)\n", (void*)(*p));
            (*p)();
        }
    }
}


void 
nk_cxx_init (void)
{
    __do_ctors_init();
}
