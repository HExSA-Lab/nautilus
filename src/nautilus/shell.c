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
 * Copyright (c) 2016, Peter Dinda <pdinda@northwestern.edu>
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
#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>

#ifndef NAUT_CONFIG_DEBUG_SHELL
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define INFO(fmt, args...)  INFO_PRINT("SHELL: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("SHELL: " fmt, ##args)
#define WARN(fmt, args...)  WARN_PRINT("SHELL: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("SHELL: " fmt, ##args)

#define CHAR_TO_IDX(k) (tolower(*(k)) - 'a')
#define BAD_KEY(k) (*(k) == ' ')

struct shell_op {
  char name[SHELL_OP_NAME_LEN];
  char **script;
  uint32_t flags;
};

struct shell_rtree_node {
    struct shell_rtree_node * ents[RTREE_NUM_ENTRIES];
    void * data;
};

struct shell_cmd {
    struct shell_cmd_impl * impl;
    unsigned ref_cnt;
    void * priv_data;
    struct list_head node;
    struct shell_cmd_state * shell_state;
};

struct shell_cmd_state {
    struct shell_rtree_node * root;
    struct list_head cmd_list;
};

struct iter_stack_entry { 
    int idx;
    struct shell_rtree_node * node;
};

struct shell_rtree_iter {
    struct shell_rtree_node * tree;
    char * substr;

    int idx;
    int node_idx;

    struct iter_stack_entry * stack[SHELL_MAX_CMD];
    int sp;
};

static inline void
iter_push_entry (struct shell_rtree_iter * iter,
                 struct shell_rtree_node * node, 
                 int idx)
{
    struct iter_stack_entry * ent = NULL;

    if (iter->sp >= (SHELL_MAX_CMD - 1)) {
        return;
    }

    ent = malloc(sizeof(*ent));
    if (!ent) {
        ERROR("Could not allocate iter stack entry\n");
        return;
    }

    ent->node = node;
    ent->idx  = idx;

    iter->stack[iter->sp++] = ent;
}

static inline struct shell_rtree_node *
iter_pop_entry (struct shell_rtree_iter * iter, int * idx)
{
    struct iter_stack_entry * ent  = NULL;
    struct shell_rtree_node * node = NULL;

    if (iter->sp <= 0) {
        return NULL;
    }

    ent = iter->stack[--iter->sp];
    iter->stack[iter->sp] = NULL;

    *idx = ent->idx;
    node = ent->node;

    free(ent);

    return node;
}


static struct shell_rtree_node *
shell_rtree_init (void)
{
    struct shell_rtree_node * root = NULL;

    root = malloc(sizeof(*root));
    if (!root) {
        ERROR("Could not alloc shell radix tree\n");
        return NULL;
    }
    memset(root, 0, sizeof(*root));

    return root;
}


static void
shell_rtree_deinit (struct shell_rtree_node * root)
{
    int i; 
    for (i = 0; i < RTREE_NUM_ENTRIES; i++) {
        if (root->ents[i]) {
            shell_rtree_deinit(root->ents[i]);
            free(root->ents[i]);
        }
    }
}


static void *
shell_rtree_lookup (struct shell_rtree_node * node, char * key)
{
    uchar_t c = CHAR_TO_IDX(key);

    if (BAD_KEY(key)) {
        WARN("bad key (%s)\n", key);
        return NULL;
    }

    if (!*key) {
        return node->data;
    }

    if (node->ents[c]) {
        return shell_rtree_lookup(node->ents[c], key + 1);
    }

    return NULL;
}


static int
shell_rtree_insert (struct shell_rtree_node * node,
                    char * key, 
                    void * data)
{
    uchar_t c = CHAR_TO_IDX(key);

    if (BAD_KEY(key)) {
        WARN("Bad key (%s)\n", key);
        return 0;
    }

    if (!*key) {
        if (!node->data) {
            node->data = data;
            return 0;
        } else {
            WARN("Key already exists in cmd tree\n");
            return 0;
        }
    } 

    if (!node->ents[c]) {
        node->ents[c] = malloc(sizeof(*node));
        if (!node->ents[c]) {
            ERROR("Could not alloc cmd node\n");
            return -1;
        }
        memset(node->ents[c], 0, sizeof(*node));
    } 

    return shell_rtree_insert(node->ents[c], key + 1, data);
}


static void *
shell_rtree_delete (char * key)
{
    ERROR("UNIMPLEMENTED (%s)\n", __func__);
    return NULL;
}


static struct shell_rtree_iter *
shell_rtree_iter_create (struct shell_rtree_node * node, char * cmd)
{
    struct shell_rtree_iter * iter = malloc(sizeof(*iter));

    if (!iter) {
        ERROR("Couldn't allocate rtree iterator\n");
        return NULL;
    }

    memset(iter, 0, sizeof(*iter));

    iter->substr   = cmd;
    iter->tree     = node;
    iter->sp       = 0;
    iter->idx      = 0;
    iter->node_idx = -1;

    return iter;
}


static void *
shell_rtree_iter_get_next (struct shell_rtree_iter * iter)
{
    int i;
    void * data = NULL;

    while (iter->node_idx < RTREE_NUM_ENTRIES && !data) {

        if (iter->node_idx < 0 && iter->tree->data) {
            data = iter->tree->data;
        } else if (iter->tree->ents[iter->node_idx]) {
            iter_push_entry(iter, iter->tree, iter->node_idx + 1);
            iter->tree = iter->tree->ents[iter->node_idx];
            iter->node_idx = -1;
            continue;
        }

        iter->node_idx++;

        // we reached the end of our subtrees
        if (iter->node_idx == RTREE_NUM_ENTRIES) {
            int new_nidx;
            struct shell_rtree_node * node = iter_pop_entry(iter, &new_nidx);
            if (node) {
                // go back to parent
                iter->tree = node;
                iter->node_idx = new_nidx;
            }
        }

    }

    iter->idx++;

    return data;
}


static void
shell_rtree_iter_destroy (struct shell_rtree_iter * iter)
{
    int i;

    for (i = 0; i < SHELL_MAX_CMD; i++) {
        if (iter->stack[i]) {
            free(iter->stack[i]);
        }
    }

    free(iter);
}


static struct shell_rtree_node *
__match (struct shell_rtree_node * node, 
         char * orig, 
         char * subcmd, 
         struct shell_rtree_iter ** iter)
{
    uchar_t c = CHAR_TO_IDX(subcmd);

    if (BAD_KEY(subcmd)) {
        WARN("bad key (%s)\n", subcmd);
        return NULL;
    }

    if (!*subcmd) {
        if (iter) {
            struct shell_rtree_iter * i = shell_rtree_iter_create(node, orig);
            *iter = i;
        }
        return node;
    } else {
        if (node->ents[c]) {
            return __match(node->ents[c], orig, subcmd + 1, iter);
        }
    }

    return NULL;
}


static inline struct shell_rtree_node *
shell_rtree_match (struct shell_rtree_node * root, char * subcmd)
{
    return __match(root, subcmd, subcmd, NULL);
}


static inline struct shell_rtree_iter *
shell_rtree_iter_from_match (struct shell_rtree_node * root, char * subcmd)
{
    struct shell_rtree_iter * iter = NULL;

    if (__match(root, subcmd, subcmd, &iter) != NULL) {
        return iter;
    }

    return NULL;
}


static int
shell_add_cmd (struct shell_cmd_state * state, struct shell_cmd * cmd)
{
    DEBUG("Adding cmd (%s) to shell command set\n", cmd->impl->cmd);
    list_add(&(cmd->node), &(state->cmd_list));
    return shell_rtree_insert(state->root, cmd->impl->cmd, (void*)cmd);
}


static int
shell_handle_cmd (struct shell_cmd_state * state, char * buf, int max)
{
    struct shell_cmd * cmd = NULL;
    int i = 0;
    int j = 0;
    char cmd_buf[SHELL_MAX_CMD];

    memset(cmd_buf, 0, SHELL_MAX_CMD);

    // skip whitespace at beginning of command
    while ((buf[i] == ' ' || !buf[i]) && (i < max)) {
        i++;
    }
    // only copy in non white-space
    while (buf[i] != 0 && buf[i] != ' ' && (i < max)) {
        cmd_buf[j++] = buf[i++];
    }

    cmd = shell_rtree_lookup(state->root, cmd_buf);

    if (cmd && cmd->impl && cmd->impl->handler) {
        return cmd->impl->handler(buf, cmd->priv_data);
    }
            
    nk_vc_printf("Don't understand \"%s\"\n", cmd_buf);

    return 0;
}


static struct shell_cmd_state * 
shell_cmd_init (void)
{
    extern struct shell_cmd_impl * __start_shell_cmds[];
    extern struct shell_cmd_impl * __stop_shell_cmds[];
    struct shell_cmd_impl ** tmp_cmd = __start_shell_cmds;
    int i = 0;

    struct shell_cmd_state * state = malloc(sizeof(*state));

    if (!state) {
        ERROR("Could not initialize shell cmd state\n");
        return NULL;
    }

    state->root = shell_rtree_init();

    INIT_LIST_HEAD(&state->cmd_list);

    if (!state->root) {
        ERROR("Could not initialize shell cmd tree\n");
        return NULL;
    }

    while (tmp_cmd != __stop_shell_cmds) {

        if (!(*tmp_cmd)) {
            ERROR("Impossible shell cmd\n");
            return NULL;
        }

        struct shell_cmd * c = malloc(sizeof(*c));
        if (!c) {
            ERROR("Could not allocate shell cmd\n");
            return NULL;
        }
        memset(c, 0, sizeof(*c));
        
        c->ref_cnt     = 0;
        c->impl        = *tmp_cmd;
        c->priv_data   = c;
        c->shell_state = state;

        if (shell_add_cmd(state, c) != 0) {
            ERROR("Could not register shell cmd (%s)\n", c->impl->cmd);
            return NULL;
        }

        tmp_cmd = &(__start_shell_cmds[++i]);
    }

    return state;
}


static void
shell_cmd_deinit (struct shell_cmd_state * state)
{
    shell_rtree_deinit(state->root);
    free(state->root);
    free(state);
}


static void 
shell (void * in, void ** out)
{
    struct shell_op * op           = (struct shell_op *)in;
    struct nk_virtual_console * vc = nk_create_vc(op->name, COOKED, 0x9f, 0, 0);
    struct shell_cmd_state * state = NULL;
    char buf[SHELL_MAX_CMD];
    char lastbuf[SHELL_MAX_CMD];
    int first = 1;

    state = shell_cmd_init();

    if (!state) {
        ERROR("Could not initialize shell commands\n");
        return;
    }

    if (!vc) { 
        ERROR("Cannot create virtual console for shell\n");
        return;
    }

    if (nk_thread_name(get_cur_thread(), op->name)) {   
        ERROR("Cannot name shell's thread\n");
        return;
    }

    if (nk_bind_vc(get_cur_thread(), vc)) { 
        ERROR("Cannot bind virtual console for shell\n");
        return;
    }

    nk_switch_to_vc(vc);
    nk_vc_clear(OUTPUT_CHAR);
    nk_vc_setattr(OUTPUT_CHAR);

    if (op->script) {
        int i;
        for (i = 0; *op->script[i]; i++) {
            nk_vc_printf("***exec: %s\n", op->script[i]);
            shell_handle_cmd(state, op->script[i], SHELL_MAX_CMD);
        }

        if (op->flags & NK_SHELL_SCRIPT_ONLY) {
            goto out;
        }
    }

    while (1) {  

        nk_vc_setattr(PROMPT_CHAR);
        nk_vc_printf("%s> ", (char*)in);
        nk_vc_setattr(INPUT_CHAR);
        nk_vc_gets(buf, SHELL_MAX_CMD, 1);
        nk_vc_setattr(OUTPUT_CHAR);

        if (!buf[0] && !first) { 
            // continue; // turn off autorepeat for now
            if (shell_handle_cmd(state, lastbuf, SHELL_MAX_CMD)) { 
                break;
            }
        } else {
            if (shell_handle_cmd(state, buf, SHELL_MAX_CMD)) {
                break;
            }
            memcpy(lastbuf, buf, SHELL_MAX_CMD);
            first = 0;
        }
    }

    nk_vc_printf("Exiting shell %s\n", (char*)in);

out:
    free(in);
    nk_release_vc(get_cur_thread());
}


nk_thread_id_t 
nk_launch_shell (char * name, 
                 int cpu, 
                 char ** script, 
                 uint32_t flags)
{
    nk_thread_id_t tid;

    struct shell_op * op = (struct shell_op *)malloc(sizeof(struct shell_op));

    if (!op) {
        WARN("No shell op provided, returning\n");
        return 0;
    }

    memset(op, 0, sizeof(*op));

    strncpy(op->name, name, SHELL_OP_NAME_LEN);

    op->name[SHELL_OP_NAME_LEN-1] = 0;
    op->script                    = script;
    op->flags                     = flags;

    if (nk_thread_start(shell, (void*)op, 0, 1, SHELL_STACK_SIZE, &tid, cpu)) { 
        free(op);
        return 0;
    } else {
        INFO_PRINT("Shell %s launched on cpu %d as %p\n",name,cpu,tid);
        return tid;
    }
}


static int
handle_shell (char * buf, void * priv)
{
    char name[SHELL_MAX_CMD];

    memset(name, 0, SHELL_MAX_CMD);

    if (sscanf(buf, "shell %s", name) == 1) { 
        nk_launch_shell(name, -1, 0, 0); // simple interactive shell
        return 0;
    }

    nk_vc_printf("invalid shell command\n");

    return 0;
}


static struct shell_cmd_impl shell_impl = {
    .cmd      = "shell",
    .help_str = "shell name",
    .handler  = handle_shell,
};
nk_register_shell_cmd(shell_impl);


static void
__dump_helps (struct shell_rtree_node * node)
{
    int i;
    struct shell_cmd * cmd;

    if (node->data) {
        cmd = node->data;
        nk_vc_printf("  %s\n", cmd->impl->help_str);
    }

    for (i = 0; i < RTREE_NUM_ENTRIES; i++) {
        if (node->ents[i]) {
            __dump_helps(node->ents[i]);
        }
    }
}


static inline void
dump_helps (struct shell_rtree_node * node)
{
    nk_vc_printf("\nAvailable commands:\n\n");
    __dump_helps(node);
    nk_vc_printf("\n");
}


static int
handle_help (char * buf, void * priv)
{
    struct shell_cmd * my_cmd = (struct shell_cmd *)priv;
    struct shell_cmd * cmd    = NULL;
    char sub[SHELL_MAX_CMD];

    memset(sub, 0, SHELL_MAX_CMD);

    if (sscanf(buf, "help %s", sub) == 1) {

        struct shell_rtree_iter * iter = shell_rtree_iter_from_match(my_cmd->shell_state->root, sub);

        if (!iter) {
            nk_vc_printf("No help strings match '%s'\n", sub);
            return 0;
        }

        struct shell_cmd * d;

        while ((d = (struct shell_cmd*)shell_rtree_iter_get_next(iter))) {
            nk_vc_printf("  %s\n", d->impl->help_str);
        }

        shell_rtree_iter_destroy(iter);

    } else if (strncmp(buf, "help", 4) == 0) {
        dump_helps(my_cmd->shell_state->root);
    }

    return 0;
}


static struct shell_cmd_impl help_impl = {
    .cmd      = "help",
    .help_str = "help [cmd (or substring)]",
    .handler  = handle_help,
};
nk_register_shell_cmd(help_impl);


static int
handle_exit (char * buf, void * priv)
{
    return 1;
}

static struct shell_cmd_impl exit_impl = {
    .cmd      = "exit",
    .help_str = "exit",
    .handler  = handle_exit,
};
nk_register_shell_cmd(exit_impl);

static int
handle_vcs (char * buf, void * priv)
{
    nk_switch_to_vc_list();
    return 0;
}

static struct shell_cmd_impl vcs_impl = {
    .cmd      = "vcs",
    .help_str = "vcs",
    .handler  = handle_vcs,
};
nk_register_shell_cmd(vcs_impl);
