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
 * http://xtack.sandia.gov/hobbes
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
#include <nautilus/sfi.h>
#include <nautilus/naut_string.h>
#include <nautilus/nautilus.h>
#include <nautilus/naut_assert.h>
#include <nautilus/msr.h>
#include <nautilus/paging.h>
#include <nautilus/cpuid.h>
#include <nautilus/mm.h>
#include <dev/apic.h>
#include <dev/ioapic.h>


#if NAUT_CONFIG_DEBUG_SFI
#define SFI_DEBUG(fmt, args...) DEBUG_PRINT("SFI: " fmt, ##args)
#else 
#define SFI_DEBUG(fmt, args...) 
#endif

static char * efi_mem_types[] = {
    "Reserved",
    "Loader Code",
    "Loader Data",
    "Boot Srvc Code",
    "Boot Srvc Data",
    "Runtime Srvc Code",
    "Runtime Srvc Data",
    "Conventional",
    "Unusable",
    "ACPI Reclaim",
    "ACPI Mem NVS",
    "MMAP IO",
    "MMAP IO Port Space",
    "Pal Code",
    "Max Mem Type",
};


static uint8_t 
blk_cksum_ok (const uint8_t * cursor, unsigned len)
{
    unsigned sum = 0;
    while (len--) {
        sum += *cursor++;
    }
    return ((sum & 0xff) == 0);
}


/*
"The OS finds the System Table by searching 16-byte boundaries between physical address 0x000E0000 and 0x000FFFFF. The SYST shall not cross a 4KB boundary. The OS shall search this region starting at the low address and shall stop searching when the 1st valid SFI System Table is found."
*/

struct sfi_sys_tbl * 
sfi_find_syst (void)
{
    uint32_t * cursor = (uint32_t*)SFI_SYST_SRCH_START;
    uint32_t * end    = (uint32_t*)SFI_SYST_SRCH_END;
    while (cursor++ <= end) {
        if (*cursor == SFI_SYST_SIG) {
            return (struct sfi_sys_tbl*)cursor;
        }
    }
    
    return NULL;
}


static int
parse_sfi_ioapic (struct sfi_ioapic_tbl * tbl, struct sys_info * sys)
{
    struct ioapic * ioa = NULL;
    unsigned nents;
    unsigned i;
    
    if (!blk_cksum_ok((uint8_t*)tbl, tbl->hdr.len)) {
        ERROR_PRINT("SFI IOAPIC table checksum failed\n");
        return -1;
    }

    nents = (tbl->hdr.len - sizeof(struct sfi_common_hdr)) / sizeof(sfi_ioapic_desc_t);

    for (i = 0; i < nents; i++) {

        sfi_ioapic_desc_t ioapic = tbl->entries[i];

        SFI_DEBUG("Found IOAPIC %u (base=%p)\n", i, (void*)ioapic);

        if (sys->num_ioapics == NAUT_CONFIG_MAX_IOAPICS) {
            panic("IOAPIC count exceeded max (change it in .config)\n");
        }

        if (!(ioa = mm_boot_alloc(sizeof(struct ioapic)))) {
            panic("Couldn't allocate struct for IOAPIC %u\n", i);
        }
        memset(ioa, 0, sizeof(struct ioapic));
    
        ioa->id      = i;
        ioa->version = 0;
        ioa->usable  = 1;
        ioa->base    = ioapic;

        sys->ioapics[sys->num_ioapics] = ioa;
        sys->num_ioapics++;
    }

    return 0;
}


/* 
 * at this point we haven't any APIC data structures. We need
 * to be able to tell if we're the BSP or not 
 */
static uint32_t
get_my_apicid (void)
{
    /* TODO: make sure we have this CPUID leaf (family > 5) */
    cpuid_ret_t r;
    cpuid(CPUID_FEATURE_INFO, &r);
    return (r.b >> 24) & 0xff;
}


static int
parse_sfi_cpu (struct sfi_cpu_tbl * tbl, struct sys_info * sys)
{
    unsigned nents;
    unsigned i;

    if (!blk_cksum_ok((uint8_t*)tbl, tbl->hdr.len)) {
        ERROR_PRINT("SFI CPU table checksum failed\n");
        return -1;
    }
    
    nents = (tbl->hdr.len - sizeof(struct sfi_common_hdr)) / sizeof(sfi_cpu_desc_t);

    for (i = 0; i < nents; i++) {
        struct cpu * new_cpu = NULL;

        sfi_cpu_desc_t apicid = tbl->entries[i];

        SFI_DEBUG("Found CPU (%u) with APIC ID %u\n", i, apicid);
        if (sys->num_cpus == NAUT_CONFIG_MAX_CPUS) {
            panic("CPU count exceeded max (check your .config)\n");
        }

		new_cpu = mm_boot_alloc(sizeof(struct cpu));
        if (!new_cpu) {
            panic("Couldn't allocate new CPU struct (%u)\n", sys->num_cpus);
        }
        memset(new_cpu, 0, sizeof(struct cpu));

        if (apicid == get_my_apicid()) { 
            new_cpu->is_bsp = 1;
            sys->bsp_id     = sys->num_cpus;
        } else {
            new_cpu->is_bsp = 0;
        }

        new_cpu->id         = sys->num_cpus;
        new_cpu->lapic_id   = apicid;
        new_cpu->enabled    = 1;
        new_cpu->cpu_sig    = 0;
        new_cpu->feat_flags = 0;
        new_cpu->system     = sys;
        /* TODO: hardcoded for Xeon Phi */
        new_cpu->cpu_khz    = 1100;

        spinlock_init(&new_cpu->lock);

        sys->cpus[sys->num_cpus] = new_cpu;
        sys->num_cpus++;

    }

    return 0;
}

static void 
sfi_fill_attrs (char * str, uint64_t attr)
{
    if (attr & EFI_MEMORY_UC) {
        strcat(str, "uc ");
    }

    if (attr & EFI_MEMORY_WC) {
        strcat(str, "wc ");
    }

    if (attr & EFI_MEMORY_WT) {
        strcat(str, "wt ");
    }

    if (attr & EFI_MEMORY_WB) {
        strcat(str, "wb ");
    }

    if (attr & EFI_MEMORY_UCE) {
        strcat(str, "uce ");
    }

    if (attr & EFI_MEMORY_WP) {
        strcat(str, "wp ");
    }

    if (attr & EFI_MEMORY_RP) {
        strcat(str, "rp ");
    }

    if (attr & EFI_MEMORY_XP) {
        strcat(str, "xp ");
    }

    if (attr & EFI_MEMORY_RUNTIME) {
        strcat(str, "rt");
    }

}


unsigned 
sfi_get_mmap_nentries (struct sfi_mmap_tbl * tbl)
{
    return (tbl->hdr.len - sizeof(struct sfi_common_hdr)) / sizeof(efi_mem_desc_t);
}


static uint64_t
parse_sfi_mmap (struct sfi_mmap_tbl * tbl, struct nk_mem_info * sysmem)
{
    unsigned nents;
    unsigned i;
    uint64_t pmem = 0;

    if (!blk_cksum_ok((uint8_t*)tbl, tbl->hdr.len)) {
        ERROR_PRINT("SFI MMAP table checksum failed\n");
        return -1;
    }
    
    nents = (tbl->hdr.len - sizeof(struct sfi_common_hdr)) / sizeof(efi_mem_desc_t);

    for (i = 0; i < nents; i++) {
        char attrs[64];
        memset(attrs, 0, sizeof(attrs));

        efi_mem_desc_t mem = tbl->entries[i]; 
        SFI_DEBUG("Memory Region %u:\n", i);
        SFI_DEBUG("  Type:       [%s]\n", (mem.type > EfiMaxMemoryType) ? "N/A" : efi_mem_types[mem.type]);
        SFI_DEBUG("  Phys Start: %p\n", (void*)mem.phys_start);
        SFI_DEBUG("  Virt Start: %p\n", (void*)mem.virt_start);
        SFI_DEBUG("  Num Pages:  %llu\n", mem.num_pages);

        //printk("adding %llu bytes to pmem\n", ((uint64_t)mem.num_pages)*((uint64_t)PAGE_SIZE_4KB));
        pmem += ((uint64_t)mem.num_pages)*((uint64_t)PAGE_SIZE_4KB);

        sfi_fill_attrs(attrs, mem.attr);
        
        SFI_DEBUG("  Attrs:      %s\n", attrs);
    }

    return pmem;
}


static int 
parse_entry (struct sfi_common_hdr * entry, struct sys_info * sys)
{
    ASSERT(entry);

    switch (entry->sig) { 
        case SFI_CPUS_SIG:
            parse_sfi_cpu((struct sfi_cpu_tbl*)entry, sys);
            break;
        case SFI_APIC_SIG:
            parse_sfi_ioapic((struct sfi_ioapic_tbl*)entry, sys);
            break;
        case SFI_MMAP_SIG:
            //parse_sfi_mmap((struct sfi_mmap_tbl*)entry, sys);
            break;
        case SFI_FREQ_SIG:
            SFI_DEBUG("Found FREQ table\n");
            break;
        case SFI_MTMR_SIG:
            SFI_DEBUG("Found MTMR table\n");
            break;
        case SFI_MRTC_SIG:
            SFI_DEBUG("Found MRTC table\n");
            break;
        case SFI_DEVS_SIG:
            SFI_DEBUG("Found DEVS table\n");
            break;
        case SFI_WAKE_SIG: 
            SFI_DEBUG("Found WAKE table\n");
            break;
        case SFI_GPIO_SIG:
            SFI_DEBUG("Found GPIO table\n");
            break;
        default:
            /* TODO: OEM table */
            ERROR_PRINT("Unknown SFI entry type: 0x%x\n", entry->sig);
            return -1;
    }
    
    return 0;
}


int 
sfi_parse_syst (struct sys_info * sys, struct sfi_sys_tbl * sfi)
{
    unsigned nents;
    unsigned i;

    if (!blk_cksum_ok((uint8_t*)sfi, sfi->hdr.len)) {
        ERROR_PRINT("Checksum failed on SFI SYST table\n");
        return -1;
    }

    nents = (sfi->hdr.len - sizeof(struct sfi_common_hdr))/sizeof(sfi->entries[0]);

    SFI_DEBUG("Detected %u entries in the SFI SYST table\n", nents);

    for (i = 0; i < nents; i++) {
        parse_entry((struct sfi_common_hdr*)(sfi->entries[i]), sys);
    }

    SFI_DEBUG("All SFI SYST entries parsed\n");
    
    return 0;
}

struct sfi_mmap_tbl *
sfi_get_mmap (void) 
{
    struct sfi_sys_tbl * sfi = sfi_find_syst();
    unsigned nents = 0;
    unsigned i;

    if (!sfi) {
        ERROR_PRINT("Could not find SFI SYST table\n");
        return NULL;
    }

    nents = (sfi->hdr.len - sizeof(struct sfi_common_hdr))/sizeof(sfi->entries[0]);

    for (i = 0; i < nents; i++) {
        struct sfi_common_hdr* entry = (struct sfi_common_hdr*)sfi->entries[i];
        if (entry->sig != SFI_MMAP_SIG) {
            continue;
        } else {
            return (struct sfi_mmap_tbl*)entry;
        }
    }

    return NULL;
}


long
sfi_parse_phys_mem (struct nk_mem_info * mem)
{
    struct sfi_sys_tbl * sfi = sfi_find_syst();
    unsigned nents = 0;
    unsigned i;

    if (!sfi) {
        ERROR_PRINT("Could not find SFI SYST table\n");
        return -1;
    }

    nents = (sfi->hdr.len - sizeof(struct sfi_common_hdr))/sizeof(sfi->entries[0]);

    for (i = 0; i < nents; i++) {
        struct sfi_common_hdr* entry = (struct sfi_common_hdr*)sfi->entries[i];
        if (entry->sig != SFI_MMAP_SIG) {
            continue;
        } else {
            return parse_sfi_mmap((struct sfi_mmap_tbl*)entry, mem);
        }
    }

    return -1;
}
