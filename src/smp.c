#include <nautilus.h>
#include <smp.h>
#include <paging.h>
#include <acpi.h>

#ifndef NAUT_CONFIG_DEBUG_SMP
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif


static uint8_t mp_entry_lengths[5] = {
    MP_TAB_CPU_LEN,
    MP_TAB_BUS_LEN,
    MP_TAB_IOAPIC_LEN,
    MP_TAB_IO_INT_LEN,
    MP_TAB_LINT_LEN,
};


/* TODO: dynamically allocate these */
static struct cpu cpus[MAX_CPUS];
static uint32_t num_cpus = 0;


static void
parse_mptable_cpu (struct mp_table_entry_cpu * cpu)
{

    cpus[num_cpus].id = num_cpus;
    cpus[num_cpus].lapic_id = cpu->lapic_id;
    cpus[num_cpus].enabled = cpu->enabled;
    cpus[num_cpus].is_bsp = cpu->is_bsp;
    cpus[num_cpus].cpu_sig = cpu->sig;
    cpus[num_cpus].feat_flags = cpu->feat_flags;

    num_cpus++;
}


static void
parse_mptable_ioapic (struct mp_table_entry_ioapic * ioapic)
{
    /* TODO: fill in */
    return;
}


static int
parse_mp_table (struct mp_table * mp)
{
    int count = mp->entry_cnt;
    uint8_t * mp_entry;


    /* make sure everything is as expected */
    if (strncmp((char*)&mp->sig, "PCMP", 4) != 0) {
        ERROR_PRINT("MP Table unexpected format\n");
    }

    mp_entry = (uint8_t*)&mp->entries;

    while (count--) {

        uint8_t type = *mp_entry;

        switch (type) {
            case MP_TAB_TYPE_CPU:
                parse_mptable_cpu((struct mp_table_entry_cpu*)mp_entry);
                break;
            case MP_TAB_TYPE_IOAPIC:
                parse_mptable_ioapic((struct mp_table_entry_ioapic*)mp_entry);
                break;
            case MP_TAB_TYPE_IO_INT:
            case MP_TAB_TYPE_BUS:
            case MP_TAB_TYPE_LINT:
                break;
            default:
                ERROR_PRINT("Unexpected MP Table Entry (type=%d)\n", type);
                return -1;
        }

        mp_entry += mp_entry_lengths[type];
    }

    return 0;
}


static struct mp_float_ptr_struct* 
find_mp_pointer (void)
{
    char * cursor = (char*)BASE_MEM_LAST_KILO;

    /* NOTE: these memory regions should already be mapped, 
     * if not, this will fail 
     */

    while (cursor != (char*)(BASE_MEM_LAST_KILO+PAGE_SIZE)) {

        if (strncmp(cursor, "_MP_", 4) == 0) {
            DEBUG_PRINT("\n");
            return (struct mp_float_ptr_struct*)cursor;
        }

        cursor += 4;
    }

    cursor = (char*)BIOS_ROM_BASE;

    while (cursor != (char*)BIOS_ROM_END) {

        if (strncmp(cursor, "_MP_", 4) == 0) {
            return (struct mp_float_ptr_struct*)cursor;
        }

        cursor += 4;
    }

    return 0;
}


int
smp_init (void)
{
    struct mp_float_ptr_struct * mp_ptr;

    mp_ptr = find_mp_pointer();

    if (!mp_ptr) {
        ERROR_PRINT("Could not find MP floating pointer struct\n");
        return -1;
    }

    parse_mp_table((struct mp_table*)(uint64_t)mp_ptr->mp_cfg_ptr);

    printk("SMP: Detected %d CPUs\n", num_cpus);

    return 0;
}


