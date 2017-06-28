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
 * Copyright (c) 2017, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/* Preliminary interface to Nautilus functions from "user" executables */

#ifndef __NAUTILUS_EXE
#define __NAUTILUS_EXE

// exposed functon list goes here

#define NK_VC_PRINTF 0


#ifdef NAUTILUS_EXE
// Being included from "user" space

// Add a macro version for eaxh exposed function here
#define nk_vc_printf(...) __nk_func_table[NK_VC_PRINTF](__VA_ARGS__)


// defined in the framework code
extern void * (**__nk_func_table)(); 

#else

// included from the kernel code

// no implementation here

#endif

#endif

