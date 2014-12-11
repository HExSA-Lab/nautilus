#include <nautilus/nautilus.h>
#include <nautilus/acpi.h>
#include <nautilus/cpu.h>
#include <nautilus/paging.h>
#include <nautilus/intrinsics.h>
#include <dev/timer.h>
#include <dev/hpet.h>

#ifndef NAUT_CONFIG_DEBUG_HPET
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif 

#define HPET_PRINT(fmt, args...) printk("HPET: " fmt, ##args)
#define HPET_DEBUG(fmt, args...) DEBUG_PRINT("HPET: " fmt, ##args)


static inline uint64_t 
hpet_read_main_cntr (struct hpet_dev * hpet)
{
    return hpet_read(hpet, HPET_MAIN_CTR_REG);
}

/* 
 * This enables the HPET
 */
static inline void 
hpet_cntr_run (struct hpet_dev * hpet)
{
    uint64_t val = hpet_read(hpet, HPET_GEN_CFG_REG);
    hpet_write(hpet, HPET_GEN_CFG_REG, val | 0x1);
    hpet->cntr_status = HPET_TIMER_RUNNING;
}


static inline void
hpet_cntr_halt (struct hpet_dev * hpet)
{
    uint64_t val = hpet_read(hpet, HPET_GEN_CFG_REG);
    hpet_write(hpet, HPET_GEN_CFG_REG, val & ~0x1);
    hpet->cntr_status = HPET_TIMER_HALTED;
}


static inline hpet_stat_t
hpet_get_cntr_stat (struct hpet_dev * hpet)
{
    return hpet->cntr_status;
}


static inline uint8_t 
hpet_get_cmp_stat (struct hpet_comparator * cmp)
{
    return cmp->stat;
}


/* 
 * this is really just the bare minimum initialization
 * we will do more when we actually assign the timer
 *
 */
static int
hpet_init_comparator (struct hpet_dev * hpet,
                      struct hpet_comparator * cmp, 
                      unsigned idx)
{
    uint64_t val;

    HPET_DEBUG("Setting up Timer %u\n", idx, (void*)cmp);

    val = hpet_read(hpet, TIMER_N_CFG_CAP_REG(idx));

    cmp->idx               = idx;
    cmp->parent            = hpet;
    cmp->int_route_cap     = TN_GET_INT_ROUTE_CAP(val);
    cmp->supports_periodic = TN_GET_PER_INT_CAP(val);
    cmp->supports_fsb_del  = TN_GET_FSB_INT_DEL_CAP(val);
    cmp->size              = TN_GET_SIZE_CAP(val);
    
    /* all of our comparators will be in 64-bit mode */
    cmp->timer_32mode      = 0;

    cmp->stat     = TIMER_DISABLED;
    cmp->int_type = TIMER_EDGE; 

    /* we don't need to actually write the cfg register
     * at this point since we're using default (0) values
     */
    
    return 0;
}

static int
parse_hpet_tbl (struct acpi_table_header * hdr, void * arg)
{
    struct hpet_dev ** hpet_ptr = (struct hpet_dev **)arg;
    struct acpi_table_hpet * hpet_tbl = (struct acpi_table_hpet*)hdr;
    struct hpet_dev * hpet = NULL;
    uint64_t cap;
    unsigned i;

    HPET_PRINT("Initializing HPET Device\n", (void*)hpet_tbl);
    HPET_DEBUG("\tID: 0x%x\n", hpet_tbl->id);
    HPET_DEBUG("\tBase Address: %p\n", (void*)hpet_tbl->address.address);
    HPET_DEBUG("\t\tAccess Width: %u\n", hpet_tbl->address.access_width);
    HPET_DEBUG("\t\tBit Width:    %u\n", hpet_tbl->address.bit_width);
    HPET_DEBUG("\t\tBit Offset:   %u\n", hpet_tbl->address.bit_offset);
    HPET_DEBUG("\t\tAspace ID:  0x%x\n", hpet_tbl->address.space_id);
    HPET_DEBUG("\tSeq Num: 0x%x\n", hpet_tbl->sequence);
    HPET_DEBUG("\tMinimum Tick Rate %u\n", hpet_tbl->minimum_tick);
    HPET_DEBUG("\tFlags: 0x%x\n", hpet_tbl->flags);

    /* first map in the page */
    nk_map_page_nocache(PAGE_MASK(hpet_tbl->address.address), PTE_PRESENT_BIT|PTE_WRITABLE_BIT);

    /* get the number of comparators */
    cap = *((volatile uint64_t*)(hpet_tbl->address.address + HPET_GEN_CAP_ID_REG));

    hpet = malloc(sizeof(struct hpet_dev) + sizeof(struct hpet_comparator)*GEN_CAP_GET_NUM_TIMERS(cap));
    if (!hpet) {
        ERROR_PRINT("Could not allocate HPET device\n");
        return -1;
    }
    memset(hpet, 0, sizeof(struct hpet_dev) + sizeof(struct hpet_comparator)*GEN_CAP_GET_NUM_TIMERS(cap));

    hpet->id       = hpet_tbl->id;
    hpet->min_tick = hpet_tbl->minimum_tick;
    hpet->flags    = hpet_tbl->flags;
    hpet->seq      = hpet_tbl->sequence;
    
    hpet->bit_width    = hpet_tbl->address.bit_width;
    hpet->bit_offset   = hpet_tbl->address.bit_offset;
    hpet->space_id     = hpet_tbl->address.space_id;
    hpet->access_width = hpet_tbl->address.access_width;
    hpet->base_addr    = hpet_tbl->address.address;

    hpet->freq     = 0x38D7EA4C68000/GEN_CAP_GET_CLK_PER(cap); // 10^15 / period
    hpet->vendor   = GEN_CAP_GET_VENDOR_ID(cap);
    hpet->num_cmps = GEN_CAP_GET_NUM_TIMERS(cap);
    hpet->rev_id   = GEN_CAP_GET_REV_ID(cap);

    HPET_DEBUG("\tFrequency: %lu.%luMHz\n", hpet->freq / 1000000, hpet->freq % 1000000);
    HPET_DEBUG("\tVendor ID: 0x%x\n", hpet->vendor);
    HPET_DEBUG("\tRevision ID: 0x%x\n", hpet->rev_id);
    HPET_DEBUG("\tComparator Count: %u\n", hpet->num_cmps);

    for (i = 0; i < hpet->num_cmps; i++) {
        hpet_init_comparator(hpet, &(hpet->cmps[i]), i);
    }

    *hpet_ptr = hpet;

    return 0;
}


int 
nk_hpet_init (void)
{
    struct hpet_dev * hpet = NULL;

    if (acpi_table_parse(ACPI_SIG_HPET, parse_hpet_tbl, &hpet)) {
        printk("No HPET device found\n");
        return 0;
    }

    nk_get_nautilus_info()->sys.hpet = hpet;

    return 0;
}





