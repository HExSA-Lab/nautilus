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
 * Copyright (c) 2019, Hongyi Chen
 * Copyright (c) 2019, Peter Dinda
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Hongyi Chen
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
xo */

#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/paging.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>

#include <nautilus/aspace.h>

#ifndef NAUT_CONFIG_DEBUG_ASPACE_PAGING
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("aspace-paging: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("aspace-paging: " fmt, ##args)
#define INFO(fmt, args...)   INFO_PRINT("aspace-paging: " fmt, ##args)


static int get_characteristics(nk_aspace_characteristics_t *c)
{
    return -1;
}

static struct nk_aspace *create(char *name, nk_aspace_characteristics_t *c)
{
    return 0;
}


static nk_aspace_impl_t paging = {
				.impl_name = "paging",
				.get_characteristics = get_characteristics,
				.create = create,
};

nk_aspace_register_impl(paging);


