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
#include <nautilus/nautilus.h>
#include <nautilus/doprnt.h>
#include <nautilus/printk.h>
#include <nautilus/fs.h>
#include <nautilus/fprintk.h>


static void
do_fputchar (nk_fs_fd_t fd, uint8_t c)
{
    char b = c;
    nk_fs_write(fd, &b, 1);
}


static int
do_fputs (nk_fs_fd_t fd, char * s)
{
    return nk_fs_write(fd, s, strnlen(s, FS_WRITE_MAX));
}


static void 
flush (struct fprintk_state * state, nk_fs_fd_t fd)
{
    int i;
    for (i = 0; i < state->idx; i++) {
        do_fputchar(fd, state->buf[i]);
    }

    state->idx = 0;
}


static void
fprintk_char (char * arg, int c)
{
    struct fprintk_state * state = (struct fprintk_state*)arg;

    if (c == '\n') {
        state->buf[state->idx] = 0;
        do_fputs(state->fd, state->buf);
        state->idx = 0;
    } else if ((c == 0) || (state->idx >= FPRINTK_BUFMAX)) {
        flush(state, state->fd);
        do_fputchar(state->fd, c);
    } else {
        state->buf[state->idx] = c;
        state->idx++;
    }
}


int 
vfprintk (nk_fs_fd_t fd, const char * fmt, va_list args)
{
    struct fprintk_state state;

    state.idx = 0;
    state.fd  = fd;

    _doprnt(fmt, args, 0, fprintk_char, (char*)&state);

    if (state.idx != 0)
        flush(&state, state.fd);

    return 0;
}


int 
fprintk (nk_fs_fd_t fd, const char * fmt, ...)
{
    va_list args;
    int err = 0;

    va_start(args, fmt);
    err = vfprintk(fd, fmt, args);
    va_end(args);

    return err;
}
