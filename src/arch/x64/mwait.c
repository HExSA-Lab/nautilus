#include <nautilus/nautilus.h>
#include <nautilus/cpuid.h>
#include <nautilus/mwait.h>


static struct mwait_info {
    uint8_t available;
    uint16_t min_line_size;
    uint16_t max_line_size;
    uint8_t ints_as_breaks;
    uint8_t c0_substates;
    uint8_t c1_substates;
    uint8_t c2_substates;
    uint8_t c3_substates;
    uint8_t c4_substates;
} mwait;


static uint8_t
has_mwait (void) 
{
    cpuid_ret_t ret;
    struct cpuid_ecx_flags f;
    cpuid(CPUID_FEATURE_INFO, &ret);
    f.val = ret.c;
    return f.monitor;
}


static void
dump_mwait_info (void)
{
    printk("MONITOR/MWAIT Feature Set:\n");
    printk("\tSmallest monitor-line size: %uB\n", mwait.min_line_size);
    printk("\tLargest monitor-line size: %uB\n", mwait.max_line_size);
    printk("\tSupports interrupts as break events: %s\n", mwait.ints_as_breaks ? "yes" : "no");
    printk("\tNumber of C0 sub C-states supported: %u\n", mwait.c0_substates);
    printk("\tNumber of C1 sub C-states supported: %u\n", mwait.c1_substates);
    printk("\tNumber of C2 sub C-states supported: %u\n", mwait.c2_substates);
    printk("\tNumber of C3 sub C-states supported: %u\n", mwait.c3_substates);
    printk("\tNumber of C4 sub C-states supported: %u\n", mwait.c4_substates);
}


int 
nk_mwait_init (void)
{
    cpuid_ret_t ret;

    if (has_mwait()) {
        printk("Processor supports MONITOR/MWAIT extensions\n");
        mwait.available = 1;
    } else {
        printk("MWAIT not supported\n");
        return 0;
    }

    memset(&mwait, 0, sizeof(mwait));

    cpuid(0x5, &ret);

    mwait.min_line_size = ret.a & 0xffff;
    mwait.max_line_size = ret.b & 0xffff;
    
    if (ret.c & 0x1) {
        mwait.ints_as_breaks = !!(ret.c & 0x2);
        mwait.c0_substates   = ret.d & 0xf;
        mwait.c1_substates   = (ret.d >> 4) & 0xf;
        mwait.c2_substates   = (ret.d >> 8) & 0xf;
        mwait.c3_substates   = (ret.d >> 12) & 0xf;
        mwait.c4_substates   = (ret.d >> 16) & 0xf;
    }

    dump_mwait_info();

    return 0;
}
