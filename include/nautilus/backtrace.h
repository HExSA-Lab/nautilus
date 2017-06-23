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

// backtrace facility using compiler builtins
#define IS_CANONICAL(x) ((((uint64_t)(x))<0x0000800000000000ULL) || (((uint64_t)(x))>=0xffff800000000000ULL))
#define IN_PHYS_MEM(x) (((uint64_t)(x)) < nk_get_nautilus_info()->sys.mem.phys_mem_avail)
#define IS_VALID(x)   ((x) && IS_CANONICAL(x))
#define BACKTRACE(print_func,limit)			\
do {							\
    void *_cur = __builtin_return_address(0);		\
    if (IS_VALID(_cur) && limit==0) {			\
        print_func("%p\n",_cur);                        \
    }                                                   \
    if (IS_VALID(_cur) && limit>0) {			\
	print_func("%p\n",_cur);			\
	_cur = __builtin_return_address(1);		\
    }							\
    if (IS_VALID(_cur) && limit>1) {			\
	print_func("%p\n",_cur);			\
	_cur = __builtin_return_address(2);		\
    }							\
    if (IS_VALID(_cur) && limit>2) {			\
	print_func("%p\n",_cur);			\
	_cur = __builtin_return_address(3);		\
    }							\
    if (IS_VALID(_cur) && limit>3) {			\
	print_func("%p\n",_cur);			\
	_cur = __builtin_return_address(5);		\
    }							\
    if (IS_VALID(_cur) && limit>4) {			\
	print_func("%p\n",_cur);			\
	_cur = __builtin_return_address(6);		\
    }							\
    if (IS_VALID(_cur) && limit>5) {			\
	print_func("%p\n",_cur);			\
	_cur = __builtin_return_address(7);		\
    }							\
    if (IS_VALID(_cur) && limit>6) {			\
	print_func("%p\n",_cur);			\
	_cur = __builtin_return_address(8);		\
    }							\
    if (IS_VALID(_cur) && limit>7) {			\
	print_func("%p\n",_cur);			\
	_cur = __builtin_return_address(9);		\
    }							\
    if (IS_VALID(_cur) && limit>8) {			\
	print_func("%p [additional frames ignored]\n",_cur);	\
    }							\
 } while (0)						


#ifdef __cplusplus
}
#endif


#endif
