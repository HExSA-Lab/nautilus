#include <nautilus/nautilus.h>
#include <nautilus/cpu.h>
#include <nautilus/percpu.h>
#include <nautilus/naut_types.h>
#include <nautilus/irq.h>
#include <dev/i8254.h>


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
        goto out;
    }

    printk("CPU %u frequency detected as %lu.%03lu MHz\n", 
            cpu,
            (ulong_t)khz / 1000, 
            (ulong_t)khz % 1000);

    return khz;

out:
    irq_enable_restore(flags);
    return 0;
}
