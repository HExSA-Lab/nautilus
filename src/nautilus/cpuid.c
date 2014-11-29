#include <nautilus/nautilus.h>
#include <nautilus/cpuid.h>
#include <nautilus/naut_string.h>


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
    

    
    


