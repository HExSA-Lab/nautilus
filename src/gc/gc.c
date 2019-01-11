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
 * Copyright (c) 2018, Kyle Hale <khale@cs.iit.edu>
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle Hale <khale@cs.iit.edu>
 *         
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/shell.h>

#ifdef NAUT_CONFIG_ENABLE_BDWGC
#include <gc/bdwgc/bdwgc.h>
#endif

#ifdef NAUT_CONFIG_ENABLE_PDSGC
#include <gc/pdsgc/pdsgc.h>
#endif

static int 
handle_collect (char * buf, void * priv)
{
#ifdef NAUT_CONFIG_ENABLE_BDWGC
    nk_vc_printf("Doing BDWGC global garbage collection\n");
    int rc = nk_gc_bdwgc_collect();
    nk_vc_printf("BDWGC global garbage collection done result: %d\n",rc);
    return 0;
#else
#ifdef NAUT_CONFIG_ENABLE_PDSGC
    nk_vc_printf("Doing PDSGC global garbage collection\n");
    struct nk_gc_pdsgc_stats s;
    int rc = nk_gc_pdsgc_collect(&s);
    nk_vc_printf("PDSGC global garbage collection done result: %d\n",rc);
    nk_vc_printf("%lu blocks / %lu bytes freed\n",
		 s.num_blocks, s.total_bytes);
    nk_vc_printf("smallest freed block: %lu bytes, largest freed block: %lu bytes\n",
		 s.min_block, s.max_block);
    return 0;
#else 
    nk_vc_printf("No garbage collector is enabled...\n");
    return 0;
#endif
#endif
}


static int 
handle_leaks (char * buf, void * priv)
{
#ifdef NAUT_CONFIG_ENABLE_BDWGC
    nk_vc_printf("Leak detection not available for BDWGC\n");
    return 0;
#else
#ifdef NAUT_CONFIG_ENABLE_PDSGC
    nk_vc_printf("Doing PDSGC global leak detection\n");
    struct nk_gc_pdsgc_stats s;
    int rc = nk_gc_pdsgc_leak_detect(&s);
    nk_vc_printf("PDSGC global leak detection done result: %d\n",rc);
    nk_vc_printf("%lu blocks / %lu bytes have leaked\n",
		 s.num_blocks, s.total_bytes);
    nk_vc_printf("smallest leaked block: %lu bytes, largest leaked block: %lu bytes\n",
		 s.min_block, s.max_block);
    return 0;
#else 
    nk_vc_printf("No garbage collector is enabled...\n");
    return 0;
#endif
#endif
}

static int
handle_bdwgc (char * buf, void * priv)
{
#ifdef NAUT_CONFIG_TEST_BDWGC
    nk_vc_printf("Testing BDWGC garbage collector\n");
    return nk_gc_bdwgc_test();
#else
    return 0;
#endif
}

static int
handle_pdsgc (char * buf, void * priv)
{
#ifdef NAUT_CONFIG_TEST_PDSGC
    nk_vc_printf("Testing PDSGC garbage collector\n");
    return nk_gc_pdsgc_test();
#else
    return 0;
#endif
}

static struct shell_cmd_impl collect_impl = {
    .cmd      = "collect",
    .help_str = "collect",
    .handler  = handle_collect,
};
nk_register_shell_cmd(collect_impl);

static struct shell_cmd_impl leaks_impl = {
    .cmd      = "leaks",
    .help_str = "leaks",
    .handler  = handle_leaks,
};
nk_register_shell_cmd(leaks_impl);

static struct shell_cmd_impl bdwgc_impl = {
    .cmd      = "bdwgc",
    .help_str = "bdwgc",
    .handler  = handle_bdwgc,
};
nk_register_shell_cmd(bdwgc_impl);

static struct shell_cmd_impl pdsgc_impl = {
    .cmd      = "pdsgc",
    .help_str = "pdsgc",
    .handler  = handle_pdsgc,
};
nk_register_shell_cmd(pdsgc_impl);
