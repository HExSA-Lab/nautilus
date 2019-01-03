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
#include <nautilus/cpuid.h>
#include <nautilus/naut_string.h>
#include <nautilus/shell.h>


uint8_t 
cpuid_get_family (void)
{
    uint8_t base_fam;
    uint8_t ext_fam;
    cpuid_ret_t ret;
    cpuid(CPUID_FEATURE_INFO, &ret);
    base_fam = CPUID_GET_BASE_FAM(ret.a);

    if (base_fam < 0xfu) {
        return base_fam;
    }

    ext_fam  = CPUID_GET_EXT_FAM(ret.a);

    return base_fam + ext_fam;
}


uint8_t
cpuid_get_model (void)
{
    uint8_t base_fam;
    uint8_t base_mod;
    uint8_t ext_mod;
    cpuid_ret_t ret;
    cpuid(CPUID_FEATURE_INFO, &ret);
    base_fam = CPUID_GET_BASE_FAM(ret.a);
    base_mod = CPUID_GET_BASE_MOD(ret.a);

    if (base_fam < 0xfu) {
        return base_mod;
    } 

    ext_mod = CPUID_GET_EXT_MOD(ret.a);

    return  ext_mod << 4 | base_mod;
}


uint8_t
cpuid_get_step (void)
{
    cpuid_ret_t ret;
    cpuid(CPUID_FEATURE_INFO, &ret);
    return CPUID_GET_PROC_STEP(ret.a);
}


uint32_t
cpuid_leaf_max (void)
{
    cpuid_ret_t ret;
    cpuid(0, &ret);
    return ret.a & 0xff;
}


void 
detect_cpu (void)
{
    cpuid_ret_t id;
    char branding[16];

    cpuid(CPUID_LEAF_BASIC_INFO0, &id);

    memcpy(&branding[0], &id.b, 4);
    memcpy(&branding[4], &id.d, 4);
    memcpy(&branding[8], &id.c, 4);
    memset(&branding[12], 0, 4);
    
    printk("Detected %s Processor\n", branding);
}


static int
handle_cpuid (char * buf, void * priv)
{
    uint32_t id, idsub, sub;
    uint64_t size;

    if ((sub=0, sscanf(buf,"cpuid %x %lu", &id, &size)==2) ||
            (size=1, sub=0, sscanf(buf,"cpuid %x",&id)==1) ||
            (size=1, sub=1, sscanf(buf,"cpuid sub %x %x",&id,&idsub)==2)) {
        uint64_t i,j,k;
        cpuid_ret_t r;
        uint32_t val[4];

        for (i=0;i<size;i++) {
            if (sub) { 
                cpuid_sub(id,idsub,&r);
                nk_vc_printf("CPUID[0x%08x, 0x%08x] =",(uint32_t)(id+i),idsub);
            } else {
                cpuid(id+i,&r);
                nk_vc_printf("CPUID[0x%08x] =",id+i);
            }
            val[0]=r.a; val[1]=r.b; val[2]=r.c; val[3]=r.d;
            for (j=0;j<4;j++) {
                nk_vc_printf(" ");
                for (k=0;k<4;k++) { 
                    nk_vc_printf("%02x",*(k + (uint8_t*)&(val[j])));
                }
            }
            for (j=0;j<4;j++) {
                nk_vc_printf(" ");
                for (k=0;k<4;k++) { 
                    nk_vc_printf("%c",isalnum(*(k + (uint8_t*)&(val[j]))) ?
                            (*(k + (uint8_t*)&(val[j]))) : '.');
                }
            }
            nk_vc_printf("\n");
        }
        return 0;
    }

    nk_vc_printf("unknown CPUID command\n");

    return 0;
}
    
static struct shell_cmd_impl cpuid_impl = {
    .cmd      = "cpuid",
    .help_str = "cpuid [sub] f [n]",
    .handler  = handle_cpuid,
};
nk_register_shell_cmd(cpuid_impl);
