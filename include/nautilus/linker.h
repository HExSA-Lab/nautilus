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
 * Copyright (c) 2018, Conghao Liu
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 *
 * Authors:  Conghao Liu <cliu115@hawk.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __LINKER_H__
#define __LINKER_H__
#include <nautilus/naut_types.h>
#include <nautilus/multiboot2.h>
#include <nautilus/nautilus.h>

/* 
 * structs below are used by linker in nautilus to parse the
 * kernel bniary symbol table
 */
typedef struct symentry {
    uint64_t value;
    uint64_t offset;
} symentry_t;

struct symtab_info {
    uint32_t sym_count;
    symentry_t * entries;
    char * strtab;
};

struct nk_link_info {
    int ready;
    struct symtab_info symtab;
};


int nk_link_prog (struct nk_link_info * linfo, struct nk_prog_info * prog);
int nk_linker_init (struct naut_info * naut);


#endif
