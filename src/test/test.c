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
 * Copyright (c) 2020, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/shutdown.h>
#include <nautilus/cmdline.h>
#include <nautilus/hashtable.h>
#include <nautilus/idle.h>
#include <test/test.h>

#ifndef NAUT_CONFIG_DEBUG_TESTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define DEBUG(fmt, args...) DEBUG_PRINT("TEST: " fmt, ##args)
#define INFO(fmt, args...)  INFO_PRINT("TEST: " fmt, ##args)
#define WARN(fmt, args...)  WARN_PRINT("TEST: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("TEST: " fmt, ##args)

struct nk_test_harness {
    struct nk_hashtable * htable;
    int test_cnt;
};

struct nk_test {
    int enabled;
    struct nk_test_impl * impl;
    struct list_head inst_list;
    int inst_cnt;
};

struct nk_test_instance {
    struct nk_test * test;
    struct list_head node;
    int argc;
    char ** argv;
};

static unsigned 
test_hash_fn(addr_t key) {
    char * name = (char *)key;
    return nk_hash_buffer((uint8_t *)name, strlen(name));
}


static int 
test_eq_fn (addr_t key1, addr_t key2) {
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;
    return (strcmp(name1, name2) == 0);
}


static void
shutdown_with_code (uint16_t code)
{
    qemu_shutdown_with_code(code);
    /* if we're not running in QEMU, just reboot */
    reboot();
}


static int
__run_tests (struct naut_info * naut, int shutdown)
{
    struct nk_hashtable_iter * iter = nk_create_htable_iter(naut->test_info->htable);

    INFO("Running test harness\n");

    if (!iter) {
        ERROR("Could not create test htable iter\n");
        return -1;
    }

    do {
        char * testname = (char*)nk_htable_get_iter_key(iter);
        struct nk_test * test = (struct nk_test*)nk_htable_get_iter_value(iter);
        struct nk_test_instance * inst = NULL;

        if (!test->enabled) continue;

        INFO("Testing all instances of test (%s)\n", test->impl->name);

        int i = 0;

        list_for_each_entry(inst, &test->inst_list, node) {
            INFO("  Instance %d (args: ", i);
                int j;
                for (j = 1; j < inst->argc; j++) {
                    printk("%s", inst->argv[j]);
                    if (j != inst->argc - 1) {
                        printk(" ");
                    }
                }
                printk(")\n");

            if (test->impl && test->impl->handler) {
                if (test->impl->handler(inst->argc, inst->argv) != 0) {
                    INFO("    [TEST FAILED]\n");
                    return -1;
                } else {
                    INFO("    [TEST PASSED]\n");
                }
            } else {
                ERROR("  No test implementation!\n");
                return -1;
            }

            i++;
        }

    } while (nk_htable_iter_advance(iter) != 0);

    INFO("All tests passed\n");

    if (shutdown) {
        INFO("Shutting down\n");
        shutdown_with_code(0x31);
    }

    return 0;
}


int
nk_run_tests (struct naut_info * naut)
{
    // if we're running tests at boot,
    // we need an idle thread on the BSP so tests
    // can block, otherwise the scheduler
    // will panic.
    nk_thread_start(idle,
                    NULL,
                    0,
                    0,
                    TSTACK_DEFAULT,
                    NULL,
                    0);
                
    int ret = __run_tests(naut, 1);
    if (ret != 0) {
        shutdown_with_code(0xff);
    }

    return ret;
}


static int
add_test (struct nk_test_harness * state,
          struct nk_test_impl * impl)
{
    struct nk_test * test = malloc(sizeof(*test));
    if (!test) {
        ERROR("Couldn't allocate test struct\n");
        return -1;
    }
    memset(test, 0, sizeof(*test));

    test->enabled  = 0;
    test->impl     = impl;
    test->inst_cnt = 0;

    INIT_LIST_HEAD(&test->inst_list);

    nk_htable_insert(state->htable, (addr_t)impl->name, (addr_t)test);

    return 0;
}



int
nk_test_init (struct naut_info *  naut)
{
    extern struct nk_test_impl * __start_tests[];
    extern struct nk_test_impl * __stop_tests[];
    struct nk_test_impl ** tmp_test = __start_tests;
    int i = 0;

    struct nk_test_harness * state = malloc(sizeof(*state));
    if (!state) {
        ERROR("Could not allocate test harness\n");
        return -1;
    }
    memset(state, 0, sizeof(*state));

    state->htable = nk_create_htable(0, test_hash_fn, test_eq_fn);

    if (!state->htable) {
        ERROR("Could not initialize test harness\n");
        goto out_err;
    }

    while (tmp_test != __stop_tests) {

        if (!(*tmp_test)) {
            ERROR("Impossible test\n");
            goto out_err1;
        }

        if (add_test(state, *tmp_test) != 0) {
            ERROR("Could not add test (%s)\n", (*tmp_test)->name);
            goto out_err1;
        }

        tmp_test = &(__start_tests[++i]);
    }

    naut->test_info = state;

    return 0;

out_err1:
    nk_free_htable(state->htable, 0, 0);
out_err:
    free(state);
    return -1;
}


#define ARGMAX 80
#define ARG_OPEN '"'
#define ARG_CLOSE '"'

/*
 * format is -test testname "arg1 arg2 -f arg3"
 *                 ^
 *                 we start here
 */
static int
parse_args (char * args, int * argc, char ** argv[])
{
    char * arg_vec[ARGMAX];
    char * arg_copy  = NULL;
    char  * testname = NULL;
    char * curs      = NULL;
    char * tmp       = NULL;
    int foundargs = 0;
    int len = 0;

    memset(arg_vec, 0, sizeof(char*)*ARGMAX);

    curs = args;

    // skip white space
    while (*curs && *curs == ' ') curs++;

    tmp = curs;

    // find the end of the test name
    while (*curs != ' ') {
        curs++; len++;
    }

    testname = malloc(len+1);
    strncpy(testname, tmp, len);
    testname[len] = '\0';

    DEBUG("testname: %s\n", testname);

    arg_vec[(*argc)++] = testname;

    while (*curs && *curs != ARG_OPEN) curs++;

    if (*curs != ARG_OPEN) {
        DEBUG("No args found, skipping\n");
        goto out;
    }

    tmp = ++curs;
    len = 0;

    while (*curs && *curs != ARG_CLOSE) {
        curs++; len++;
    }

    arg_copy = malloc(len+1);
    strncpy(arg_copy, tmp, len);
    arg_copy[len] = '\0';

    tmp = strtok(arg_copy, " ");

    DEBUG("First arg: %s\n", tmp);

    if (tmp) {
        arg_vec[(*argc)++] = tmp;
    }

    while ((tmp = strtok(NULL, " "))) {
        DEBUG("Arg found: %s\n", tmp);
        arg_vec[(*argc)++] = tmp;
    }

    DEBUG("Found %d args\n", *argc);

out:
    *argv = malloc(sizeof(char*)*(*argc));
    memcpy(*argv, arg_vec, sizeof(char*)*(*argc));

    return 0;
}


static int
handle_test_from_cmdline (char * args)
{
    struct nk_test_harness * state = nk_get_nautilus_info()->test_info;
    struct nk_test * test = NULL;
    struct nk_test_instance * inst = NULL;
    char ** argv;
    int argc;

    parse_args(args, &argc, &argv);

    DEBUG("looking up test '%s'\n", argv[0]);

    test = (struct nk_test*)nk_htable_search(state->htable, (addr_t)argv[0]);

    if (!test) {
        ERROR("No test '%s' found\n", argv[0]);
        return -1;
    }

    /* 
     * we have at least one instance, so 
     * we enable it
     */
    if (!test->enabled) {
        DEBUG("Enabling test '%s'\n", test->impl->name);
        test->enabled = 1;
    }

    inst = malloc(sizeof(*inst));
    if (!inst) {
        ERROR("Couldn't allocate test instance\n");
        return -1;
    }
    memset(inst, 0, sizeof(*inst));

    inst->argc = argc;
    inst->argv = argv;
    inst->test = test;

    DEBUG("Adding new test instance for '%s'\n", test->impl->name);

    list_add_tail(&inst->node, &test->inst_list);
    test->inst_cnt++;

    return 0;
}


static int
handle_all (char * args)
{
    struct nk_test_harness * state  = nk_get_nautilus_info()->test_info;
    struct nk_hashtable_iter * iter = nk_create_htable_iter(state->htable);

    INFO("Enabling all tests\n");

    if (!iter) {
        ERROR("Could not create test htable iter\n");
        return -1;
    }

    do {
        char * testname = (char*)nk_htable_get_iter_key(iter);
        struct nk_test * test = (struct nk_test*)nk_htable_get_iter_value(iter);
        char argbuf[ARGMAX];

        test->enabled = 1;

        snprintf(argbuf, ARGMAX, "%s \"%s\"", testname, test->impl->default_args);

        if (handle_test_from_cmdline(argbuf) != 0) {
            ERROR("Could not enable test '%s'\n", testname);
            return -1;
        }

    } while (nk_htable_iter_advance(iter) != 0);

    return 0;
}


/*
 * The following can be used like so in the grub.cfg:
 * multiboot2 /nautilus.bin -test sampletest "foo bar baz" -test sampletest "three more args"
 */

static struct nk_cmdline_impl test_cmdline_impl = {
    .name    = "test",
    .handler = handle_test_from_cmdline,
};
nk_register_cmdline_flag(test_cmdline_impl);

static struct nk_cmdline_impl all_cmdline_impl = {
    .name    = "test-all",
    .handler = handle_all,
};
nk_register_cmdline_flag(all_cmdline_impl);


static int
handle_sample_test (int argc, char ** argv)
{
    DEBUG("In sample test handler\n");
    return 0;
}


static struct nk_test_impl test_impl = {
    .name         = "sampletest",
    .handler      = handle_sample_test,
    .default_args = "foo bar baz",
};

nk_register_test(test_impl);
