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
 * Copyright (c) 2016, Peter Dinda
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Peter Dinda <pdinda@northwestern.edu>
 *          Kyle Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __NK_SHELL
#define __NK_SHELL

#define NK_SHELL_SCRIPT_ONLY 0x1
#define SHELL_OP_NAME_LEN 32
#define SHELL_MAX_CMD 80
#define SHELL_STACK_SIZE (PAGE_SIZE_2MB) 

#define SHELL_HIST_BUF_SZ 128
#define SHELL_DEFAULT_HIST_DISPLAY 20

#define PROMPT_CHAR 0xcf
#define INPUT_CHAR  0x3f
#define OUTPUT_CHAR 0x9f

/* '0'-'9', 'a'-'z' , '-', '_', ' '  */
#define RTREE_NUM_ENTRIES 39

struct shell_cmd_impl {
    char * cmd;
    char * help_str;
    int (*handler)(char * buf, void * priv);
};

nk_thread_id_t nk_launch_shell(char *name, int cpu, char **script, uint32_t flags);

#define nk_register_shell_cmd(cmd) \
    static struct shell_cmd_impl * _nk_cmd_##cmd \
    __attribute__((used)) \
    __attribute__((unused, __section__(".shell_cmds"), \
        aligned(sizeof(void*)))) \
    = &cmd;

#endif
