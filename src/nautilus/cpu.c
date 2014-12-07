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

    return khz;

out_err:
    irq_enable_restore(flags);
    return -1;
}
