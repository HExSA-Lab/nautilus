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
 * Copyright (c) 2018, Kyle C. Hale
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Conghao Liu <cliu115@hawk.iit.edu>
 *           Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __PROG_H__
#define __PROG_H__


struct nk_prog_info {
    char * name;
    struct multiboot_mod * mod;
    void * entry_addr;
    int argc;
    char ** argv;
};

int nk_prog_init (struct naut_info * naut);
int nk_prog_run (struct naut_info * naut);

#endif
