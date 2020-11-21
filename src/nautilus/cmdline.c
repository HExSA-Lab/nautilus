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


typedef enum {
    FLAG_SCANNING,
    FLAG_NAME_CONSUME,
    FLAG_ARGS_FIND,
    FLAG_NO_ARGS,
    FLAG_ARGS_CONSUME,
    FLAG_SUBARGS_CONSUME,
    FLAG_ARGS_END,
} cmdline_state_t;


static char * 
strndup (char * str, size_t lim)
{
    char * ret = malloc(strnlen(str, lim) + 1);
    if (!ret) {
        return NULL;
    }
    strncpy(ret, str, strnlen(str, lim));
    ret[strnlen(str, lim)] = 0;
    return ret;
}

int
nk_cmdline_dispatch (struct naut_info * naut)
{
    struct cmdline_state * state = naut->cmdline;
    char * cline = naut->sys.mb_info->boot_cmd_line;
    char * flag_args = NULL;
    char * cursor    = NULL;
    char * flag      = NULL;
    char * nonwhite  = NULL;
    int len          = 0;
    int i            = 0;

    INFO("parsing kernel cmdline arguments %s\n", cline);

    cmdline_state_t state_id = FLAG_SCANNING;

    for (i = 0; cline[i]; i++) {

        switch (state_id) {
            case FLAG_SCANNING:
                if (cline[i] == '-') {
                    cursor   = &cline[i+1];
                    state_id = FLAG_NAME_CONSUME;
                    len      = 0;
                    DEBUG("Entering FLAG_NAME_CONSUME state\n");
                }
                break;
            case FLAG_NAME_CONSUME:
                if (cline[i] == ' ' || cline[i] == '\t') {
                    state_id = FLAG_ARGS_FIND;
                    flag     = strndup(cursor, len);
                    len      = 0;
                    DEBUG("Entering FLAG_ARGS_FIND state (flag=%s)\n", flag);
                } else {
                    len++;
                }
                break;
            case FLAG_ARGS_FIND:
                if (cline[i] == ' ' || cline[i] == '\t') {
                    break;
                } else if (cline[i] == '-') {
                    state_id = FLAG_NO_ARGS;
                    DEBUG("Entering FLAG_NO_ARGS state\n");
                    break;
                } else { // non-whitespace char (also not -)
                    DEBUG("Entering FLAG_ARGS_CONSUME state\n");
                    state_id = FLAG_ARGS_CONSUME;
                    cursor   = &cline[i];
                    nonwhite = cursor;
                    len      = 0;
                }
                break;
            case FLAG_NO_ARGS:
                if (dispatch_flag(state, flag, NULL)) {
                    ERROR("Could not dispatch cmdline flag (%s)\n", flag);
                    free(flag);
                    return -1;
                }
                state_id = FLAG_SCANNING;
                DEBUG("Entering FLAG_SCANNING state\n");
                i--;
                break;
            case FLAG_ARGS_CONSUME:
                if (strncmp(&cline[i], " -", 2) == 0) {
                    state_id = FLAG_ARGS_END;
                    len++;
                    DEBUG("Entering FLAG_ARGS_END state\n");
                    break;
                } else if (cline[i] == '\"') {
                    state_id = FLAG_SUBARGS_CONSUME;
                    DEBUG("Entering FLAG_SUBARGS_CONSUME state\n");
                }

                if (cline[i] != ' ' && cline[i] != '\t') {
                    nonwhite = &cline[i];
                }
                len++;
                break;
            case FLAG_SUBARGS_CONSUME:
                if (cline[i] == '\"') {
                    state_id = FLAG_ARGS_CONSUME;
                    DEBUG("Entering FLAG_ARGS_CONSUME state\n");
                } 

                if (cline[i] != ' ' && cline[i] != '\t') {
                    nonwhite = &cline[i];
                }
                len++;
                break;
            case FLAG_ARGS_END:
                state_id  = FLAG_SCANNING;
                flag_args = strndup(cursor, len);
                cursor    = NULL;
                nonwhite  = NULL;
                len       = 0;
                DEBUG("In FLAG_ARGS_END with flag=%s and args=%s\n", flag, flag_args);
                if (dispatch_flag(state, flag, flag_args)) {
                    ERROR("Could not dispatch cmdline flag (%s)\n", flag);
                    free(flag);
                    free(flag_args);
                    return -1;
                }
                flag      = NULL;
                flag_args = NULL;
                i--;
                DEBUG("Entering FLAG_SCANNING state\n");
                break;
            default:
                ERROR("Invalid command parser state: %d\n", state_id);
                return -1;
        }

    }

    DEBUG("Reached end of input\n");
    len++;

    /* we reached the end */
    switch (state_id) {
        case FLAG_SCANNING: /* we never found any flags */
            DEBUG("Never found any command flags\n");
            break;
        case FLAG_ARGS_FIND: /* we never found args for most recent flag */
            if (dispatch_flag(state, flag, NULL)) {
                ERROR("Could not dispatch cmdline flag (%s)\n", flag);
                free(flag);
                return -1;
            }
            break;
        case FLAG_ARGS_CONSUME: /* we never encountered another flag */
            flag_args = strndup(cursor, len);
            DEBUG("Consumed args: %s\n", flag_args);
            if (dispatch_flag(state, flag, flag_args)) {
                ERROR("Could not dispatch cmdline flag (%s)\n", flag);
                free(flag);
                free(flag_args);
                return -1;
            }
            break;
        default:
            ERROR("Invalid command parser state (%d)\n", state_id);
            return -1;
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
