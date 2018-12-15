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
#include <nautilus/nautilus.h>
#include <nautilus/hashtable.h>
#include <nautilus/mb_utils.h>
#include <nautilus/cmdline.h>

#ifndef NAUT_CONFIG_DEBUG_CMDLINE
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define DEBUG(fmt, args...) DEBUG_PRINT("CMDLINE: " fmt, ##args)
#define INFO(fmt, args...)  INFO_PRINT("CMDLINE: " fmt, ##args)
#define WARN(fmt, args...)  WARN_PRINT("CMDLINE: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("CMDLINE: " fmt, ##args)


static unsigned 
cmdline_hash_fn(addr_t key) {
    char * name = (char *)key;
    return nk_hash_buffer((uint8_t *)name, strlen(name));
}


static int 
cmdline_eq_fn (addr_t key1, addr_t key2) {
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;
    return (strcmp(name1, name2) == 0);
}


static int
dispatch_flag (struct cmdline_state * state, char * flag, char * args)
{
    struct registered_flag * inv      = NULL;
    struct flag_invocation * instance = NULL;

    inv = (struct registered_flag*)nk_htable_search(state->htable, (addr_t)flag);

    if (!inv) {
        ERROR("No cmdline flag '%s' found\n", flag);
        return -1;
    }

    instance = malloc(sizeof(*instance));

    if (!instance) {
        ERROR("Could not allocate flag instance\n");
        return -1;
    }

    memset(instance, 0, sizeof(*instance));

    instance->flag = inv;
    instance->args = args;

    list_add(&instance->node, &inv->invoke_list);
    inv->num_inst++;

    if (instance->flag->impl && instance->flag->impl->handler) {
        return instance->flag->impl->handler(instance->args);
    }

    return 0;
}


int
nk_cmdline_dispatch (struct naut_info * naut)
{
    struct cmdline_state * state = naut->cmdline;
    char * cline = naut->sys.mb_info->boot_cmd_line;

    while (*cline) {
        switch (*cline) {
            case '-': { // found a flag word 

                char * word_curs = ++cline;
                int len = 0;

                while (*cline != ' ' && *cline != '\t') {
                    len++; cline++;
                }

                char * new = malloc(len + 1);
                strncpy(new, word_curs, len);
                new[len] = 0;

                while (*cline == ' ' || *cline == '\t') cline++;

                char * arg_curs = cline;
                len = 0;

                while (*cline  && *cline != '-') {
                    len++; cline++;
                }

                char * args = malloc(len + 1);
                strncpy(args, arg_curs, len);
                args[len] = 0;

                DEBUG("Found flag (%s)\n", new);
                DEBUG("  args: %s\n", args);

                if (dispatch_flag(state, new, args)) {
                    ERROR("Could not dispatch cmdline flag (%s)\n", new);
                    free(new);
                    free(args);
                }

                break;
            }
            default:
                cline++;
                break;
        }

    }

    return 0;
}


static int
add_cmdline_flag (struct cmdline_state * state, struct nk_cmdline_impl * impl)
{
    struct registered_flag * flag = NULL;

    DEBUG("Adding cmdline flag (%s)\n", impl->name);

    flag = malloc(sizeof(*flag));
    if (!flag) {
        ERROR("Could not add cmdline flag (%s)\n", impl->name);
        return -1;
    }
    memset(flag, 0, sizeof(*flag));

    flag->impl     = impl;
    flag->state    = state;
    flag->num_inst = 0;

    INIT_LIST_HEAD(&flag->invoke_list);

    nk_htable_insert(state->htable, (addr_t)impl->name, (addr_t)flag);

    return 0;
}


int
nk_cmdline_init (struct naut_info * naut)
{
    extern struct nk_cmdline_impl * __start_flags[];
    extern struct nk_cmdline_impl * __stop_flags[];
    struct nk_cmdline_impl ** tmp_flag = __start_flags;
    int i = 0;

    struct cmdline_state * state = malloc(sizeof(*state));

    if (!state) {
        ERROR("Could not allocate cmdline state\n");
        return -1;
    }
    memset(state, 0, sizeof(*state));

    state->htable = nk_create_htable(0, cmdline_hash_fn, cmdline_eq_fn);

    if (!state->htable) {
        ERROR("Could not create cmdline flag htable\n");
        goto out_err;
    }

    while (tmp_flag != __stop_flags) {

        if (!(*tmp_flag)) {
            ERROR("Impossible cmdline flag\n");
            goto out_err1;
        }

        add_cmdline_flag(state, *tmp_flag);

        tmp_flag = &(__start_flags[++i]);
    }

    naut->cmdline = state;

    return 0;

out_err1:
    nk_free_htable(state->htable, 0, 0);
out_err:
    free(state);
    return -1;
}
