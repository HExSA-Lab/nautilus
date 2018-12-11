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
#include <nautilus/msr.h>
#include <nautilus/shell.h>


inline void 
msr_write (uint32_t msr, uint64_t data)
{
    uint32_t lo = data;
    uint32_t hi = data >> 32;
    asm volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}


inline uint64_t 
msr_read (uint32_t msr) 
{
    uint32_t lo, hi;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}


static int
handle_wrmsr (char * buf, void * priv)
{
    uint32_t msr;
    uint64_t data;

    if (sscanf(buf, "wrmsr %x %lx",&msr,&data)==2) { 
        msr_write(msr,data);
        nk_vc_printf("MSR[0x%08x] = 0x%016lx\n",msr,data);
        return 0;
    }

    nk_vc_printf("unknown wrmsr request\n");

    return 0;
}


static int
handle_rdmsr (char * buf, void * priv)
{
    uint32_t msr;
    uint64_t size;
    uint64_t data;

    if ((sscanf(buf,"rdmsr %x %lu", &msr, &size)==2) ||
            (size=1, sscanf(buf,"rdmsr %x", &msr)==1)) {
        uint64_t i,k;
        for (i=0;i<size;i++) { 
            data = msr_read(msr+i);
            nk_vc_printf("MSR[0x%08x] = 0x%016lx ",(uint32_t)(msr+i),data);
            for (k=0;k<8;k++) { 
                nk_vc_printf("%02x",*(k + (uint8_t*)&data));
            }
            nk_vc_printf(" ");
            for (k=0;k<8;k++) { 
                nk_vc_printf("%c",isalnum(*(k + (uint8_t*)&data)) ?
                        (*(k + (uint8_t*)&data)) : '.');
            }
            nk_vc_printf("\n");
        }
        return 0;
    }

    nk_vc_printf("unknown rdmsr request\n");

    return 0;
}

static struct shell_cmd_impl rdmsr_impl = {
    .cmd      = "rdmsr",
    .help_str = "rdmsr x [n]",
    .handler  = handle_rdmsr,
};
nk_register_shell_cmd(rdmsr_impl);

static struct shell_cmd_impl wrmsr_impl = {
    .cmd      = "wrmsr",
    .help_str = "wrmsr x y",
    .handler  = handle_wrmsr,
};
nk_register_shell_cmd(wrmsr_impl);
