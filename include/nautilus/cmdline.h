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
 * Copyright (c) 2018, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __CMDLINE_H__
#define __CMDLINE_H__

struct nk_cmdline_impl {
    char * name;
    int (*handler)(char * args);
}; 

struct cmdline_state {
    struct nk_hashtable * htable;
};

/* 
 * we can have multiple invocations of the same flag, so
 * we maintain a list of them, e.g.
 * -test t1,args="arg1 arg2 arg3"
 *
 */
struct registered_flag {
    struct nk_cmdline_impl * impl;
    struct cmdline_state * state;
    struct list_head invoke_list;
    size_t num_inst;
};

struct flag_invocation {
    struct registered_flag * flag;
    struct list_head node;
    char * args;
};


int nk_cmdline_dispatch(struct naut_info * naut);
int nk_cmdline_init(struct naut_info * naut);

#define nk_register_cmdline_flag(f) \
    static struct  nk_cmdline_impl * _nk_cmdline_##f \
    __attribute__((used)) \
    __attribute__((unused, __section__(".cmdline_flags"), \
        aligned(sizeof(void*)))) \
    = &f;

#endif
