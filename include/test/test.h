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
#ifndef __NK_TEST_H__
#define __NK_TEST_H__


struct nk_test_impl {
    char * name;
    int (*handler)(int argc, char * argv[]);
    char * default_args;
}; 

int nk_run_tests(struct naut_info * naut);
int nk_test_init(struct naut_info * naut);

#define nk_register_test(t) \
    static struct nk_test_impl * _nk_test_##t \
    __attribute__((used)) \
    __attribute__((unused, __section__(".tests"), \
        aligned(sizeof(void*)))) \
    = &t;

#endif
