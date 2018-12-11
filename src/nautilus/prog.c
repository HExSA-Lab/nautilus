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
 *
 * Authors:  Conghao Liu <cliu115@hawk.iit.edu>
 *           Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/multiboot2.h>
#include <nautilus/mb_utils.h>
#include <nautilus/naut_string.h>
#include <nautilus/prog.h>
#include <nautilus/linker.h>
#include <nautilus/shell.h>

#ifndef NAUT_CONFIG_DEBUG_LINKER
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define MAX_ARGV_LEN 1024
#define MAX_ARG_LEN 64

static void
init_argv (struct nk_prog_info * pr, struct multiboot_mod * mod)
{
    char ** argv = NULL;
    char * tmp   = NULL;
    char tmp_cmdline[MAX_ARGV_LEN];
    unsigned len = strnlen(mod->cmdline, MAX_ARGV_LEN);
    int argc     = 0;
    int i = 0;

    memcpy(tmp_cmdline, mod->cmdline, len + 1);

    tmp = strtok(tmp_cmdline, " ");

    while (tmp) {
        argc++;
        tmp = strtok(NULL, " ");
    }

    argv = malloc(sizeof(char*)*(argc+1));
    if (!argv) {
        ERROR_PRINT("Could not allocate argv array\n");
        return;
    }
    memset(argv, 0, sizeof(char*)*(argc+1));

    memcpy(tmp_cmdline, mod->cmdline, len + 1);

    tmp = strtok(tmp_cmdline, " ");

    while (tmp) {
        int arglen = strnlen(tmp, MAX_ARG_LEN) + 1;
        char * arg = malloc(arglen);
        if (!arg) {
            ERROR_PRINT("Could not allocate arg %d\n", i);
        }

        memset(arg, 0, arglen);
        memcpy(arg, tmp, arglen);

        argv[i++] = arg;

        tmp = strtok(NULL, " ");
    }

    pr->name = argv[0];
}


int
nk_prog_init (struct naut_info * naut) 
{
    struct nk_prog_info * prog = NULL;
    struct list_head * tmp = NULL;
    struct multiboot_mod * mod = NULL;

    prog = malloc(sizeof(struct nk_prog_info));

    if (!prog) {
        ERROR_PRINT("Could not allocate program info\n");
        return -1;
    }
    memset(prog, 0, sizeof(struct nk_prog_info));

    list_for_each(tmp, &naut->sys.mb_info->mod_list) {
        mod = list_entry(tmp, struct multiboot_mod, elm);
        if (mod->type == MOD_PROGRAM) {
            prog->mod        = mod;
            prog->entry_addr = (void*) mod->start + *((uint64_t*) (mod->start + 0x18));
            init_argv(prog, mod);
            DEBUG_PRINT("Program entry addr: %p\n", (void*)prog->entry_addr);
        }
    }

    naut->sys.prog_info = prog;

    return 0;
}

static int
have_valid_prog (struct naut_info * naut)
{
    return naut->sys.prog_info->mod != NULL;
}


int
nk_prog_run (struct naut_info * naut)
{
    void (*func)(int argc, char ** argv);

    if (!have_valid_prog(naut)) {
        nk_vc_printf("No valid program found\n");
        return 0;
    }

    if (nk_link_prog(naut->sys.linker_info, naut->sys.prog_info) != 0) {
        ERROR_PRINT("Could not link program\n");
        return -1;
    }

    func = naut->sys.prog_info->entry_addr;

    DEBUG_PRINT("Running program (%s) at entry address: %p\n", naut->sys.prog_info->name, (void*)func);

    func(naut->sys.prog_info->argc, naut->sys.prog_info->argv);

    return 0;
}


static void
prog_info (struct naut_info * naut)
{
    struct nk_prog_info * pr = naut->sys.prog_info;

    if (have_valid_prog(naut)) {
        nk_vc_printf("Program name: %s\n", pr->name);
        nk_vc_printf("Module address: %p\n", (void*)pr->mod);
        nk_vc_printf("Program entry address: %p\n", pr->entry_addr);
    } else {
        nk_vc_printf("No valid program found\n");
    }
}


static int
handle_prog (char * buf, void * priv)
{
    buf += 4;

    while( *buf && *buf == ' ') {
        buf++;
    }
    
    if (!*buf) {
        return 0;
    }

    if ((strncmp(buf, "run", 3) == 0)) {
        nk_prog_run(nk_get_nautilus_info());
    } else if ((strncmp(buf, "info", 4) == 0)) {
        prog_info(nk_get_nautilus_info());
    }

    return 0;
}


static struct shell_cmd_impl prog_impl = {
    .cmd      = "prog",
    .help_str = "prog run | prog info",
    .handler  = handle_prog,
};
nk_register_shell_cmd(prog_impl);
