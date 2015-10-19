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
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/cpu.h>
#include <nautilus/cpuid.h>
#include <nautilus/percpu.h>
#include <nautilus/naut_types.h>
#include <nautilus/naut_string.h>
#include <nautilus/irq.h>
#include <dev/i8254.h>


static int
get_vendor_string (char name[13])
{
    cpuid_ret_t ret;
    cpuid(0, &ret);
    ((uint32_t*)name)[0] = ret.b;
    ((uint32_t*)name)[1] = ret.d;
    ((uint32_t*)name)[2] = ret.c;
    name[12] = '\0';
    return ret.a;
}


uint8_t 
nk_is_amd (void) 
{
    char name[13];
    get_vendor_string(name);
    return !strncmp(name, "AuthenticAMD", 13);
}

uint8_t
nk_is_intel (void)
{
    char name[13];
    get_vendor_string(name);
    return !strncmp(name, "GenuineIntel", 13);
}

/*
 * nk_detect_cpu_freq
 *
 * detect this CPU's frequency in KHz
 *
 * returns freq on success, 0 on error
 *
 */
ulong_t
nk_detect_cpu_freq (uint32_t cpu) 
{
    uint8_t flags = irq_disable_save();
    ulong_t khz = i8254_calib_tsc();
    if (khz == ULONG_MAX) {
        ERROR_PRINT("Unable to detect CPU frequency\n");
        goto out_err;
    }

    irq_enable_restore(flags);
    return khz;

out_err:
    irq_enable_restore(flags);
    return -1;
}
