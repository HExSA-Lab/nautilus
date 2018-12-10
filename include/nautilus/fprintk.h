#ifndef __FPRINTK_H__
#define __FPRINTK_H__
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
 * Copyright (c) 2016, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/fs.h>

#define FPRINTK_BUFMAX  128
#define FS_WRITE_MAX 1024

struct fprintk_state {
    char buf[FPRINTK_BUFMAX];
    unsigned idx;
    nk_fs_fd_t fd;
};


int vfprintk(nk_fs_fd_t fd, const char * fmt, va_list args);
int fprintk(nk_fs_fd_t fd, const char * fmt, ...);

#endif
