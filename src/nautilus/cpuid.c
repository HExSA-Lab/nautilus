#include <nautilus/nautilus.h>
#include <nautilus/cpuid.h>
#include <nautilus/naut_string.h>


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


uint8_t
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
    

    
    


