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
#ifndef __BACKTRACE_H__
#define __BACKTRACE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define _TRACE_TAG printk("[----------------- Call Trace ------------------]\n")

#define backtrace_here() _TRACE_TAG; \
    __do_backtrace(__builtin_frame_address(0), 0)

#define backtrace(x) _TRACE_TAG; \
    __do_backtrace((void*)(x), 0)

    


struct nk_regs;
void __do_backtrace(void **, unsigned);
void nk_dump_mem(void *, ulong_t);
void nk_stack_dump(ulong_t);
void nk_print_regs(struct nk_regs * r);


#ifdef __cplusplus
}
#endif


#endif
