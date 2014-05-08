#include <dev/apic.h>
#include <cpuid.h>
#include <msr.h>


int 
check_apic_avail (void)
{
    cpuid_ret_t cp;
    struct cpuid_feature_flags * flags;

    cp = cpuid(CPUID_FEATURE_INFO);
    flags = (struct cpuid_feature_flags *)&cp.c;
    return flags->edx.apic;
}


/*
void 
apic_enable (void) 
{
    msr_write(IA32_APIC_BASE

}
*/


