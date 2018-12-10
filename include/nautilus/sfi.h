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
#ifndef __SFI_H__
#define __SFI_H__

#define SFI_SYST_SRCH_START 0xe0000
#define SFI_SYST_SRCH_END   0xfffff


#define SFI_SYST_SIG 0x54535953 // "SYST"
#define SFI_CPUS_SIG 0x53555043 // "CPUS"
#define SFI_APIC_SIG 0x43495041 // "APIC"
#define SFI_MMAP_SIG 0x50414d4d // "MMAP"
#define SFI_FREQ_SIG 0x51455246 // "FREQ"
#define SFI_MTMR_SIG 0x524d544d // "MTMR"
#define SFI_MRTC_SIG 0x4352544d // "MTRC"
#define SFI_DEVS_SIG 0x53564544 // "DEVS"
#define SFI_WAKE_SIG 0x454b4157 // "WAKE"
#define SFI_GPIO_SIG 0x4f495047 // "GPIO"
#define SFI_OEM_SIG  "OEM"

/* EFI memory descriptor attributes
 * each one defines a *capability* of the region 
 */
#define EFI_MEMORY_UC      0x0000000000000001 // no cache
#define EFI_MEMORY_WC      0x0000000000000002 // write combine
#define EFI_MEMORY_WT      0x0000000000000004 // write through
#define EFI_MEMORY_WB      0x0000000000000008 // write back
#define EFI_MEMORY_UCE     0x0000000000000010 // no cache + exported + 'fetch & add'
#define EFI_MEMORY_WP      0x0000000000001000 // write protected
#define EFI_MEMORY_RP      0x0000000000002000 // read protected
#define EFI_MEMORY_XP      0x0000000000004000 // NX
#define EFI_MEMORY_RUNTIME 0x8000000000000000 // needs virtual mapping from OS

#include <nautilus/naut_types.h>
#include <nautilus/intrinsics.h>


typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiMaxMemoryType
} efi_mem_type_t;


/* from SFI Spec 0.8.2 */

struct sfi_common_hdr {
    uint32_t sig;
    uint32_t len;
    uint8_t  rev_id;
    uint8_t  cksum;
    char     oem_id[6];
    char     oeim_tbl_id[8];
} __packed;

typedef uint64_t sfi_tbl_desc_t;
typedef uint32_t sfi_cpu_desc_t;
typedef uint64_t sfi_ioapic_desc_t;

struct efi_mem_desc {
    uint32_t type;
    uint64_t phys_start;
    uint64_t virt_start;
    uint64_t num_pages;
    uint64_t attr;
} __packed; 

typedef struct efi_mem_desc efi_mem_desc_t;
    
struct sfi_sys_tbl {
    struct sfi_common_hdr hdr;
    sfi_tbl_desc_t entries[0];
};

struct sfi_cpu_tbl {
    struct sfi_common_hdr hdr;
    sfi_cpu_desc_t entries[0];
};

struct sfi_ioapic_tbl {
    struct sfi_common_hdr hdr;
    sfi_ioapic_desc_t entries[0];
};

struct sfi_mmap_tbl {
    struct sfi_common_hdr hdr;
    efi_mem_desc_t        entries[0];
};

typedef struct sfi_freq_entry {
    uint32_t freq; // in MHz
    uint32_t trans_latency; // transition latency
    uint32_t p_ctrl; // value to write to PERF_CTL to enter this P-state
} __packed sfi_freq_entry_t;
    

struct sfi_freq_tbl {
    struct sfi_common_hdr hdr;
    sfi_freq_entry_t entries[0];
};


struct sfi_m_timer_entry {
    uint64_t phys_addr;
    uint32_t timer_freq;
    uint32_t irq; // IRQ number for this timer
} __packed;

typedef struct sfi_m_timer_entry sfi_m_timer_entry_t;

struct sfi_m_timer_tbl {
    struct sfi_common_hdr hdr;
    sfi_m_timer_entry_t entries[0];
};

struct sfi_m_rtc_entry {
    uint64_t phys_addr;
    uint32_t irq;
} __packed;

typedef struct sfi_m_rtc_entry sfi_m_rtc_entry_t;


struct sfi_m_rtc_tbl {
    struct sfi_common_hdr hdr;
    sfi_m_rtc_entry_t entries[0];
};


struct sfi_wake_vec_tbl {
    struct sfi_common_hdr hdr;
    uint64_t wake_vec_addr;
};

struct sfi_plat_dev_entry {
    uint8_t  host_type;
    uint8_t  host_num;
    uint16_t dev_addr;
    uint8_t  dev_irq;
    uint32_t dev_max_freq; // in Hz
    char     dev_name[16];
} __packed;

typedef struct sfi_plat_dev_entry sfi_plat_dev_entry_t;

struct sfi_plat_dev_tbl {
    struct sfi_common_hdr hdr;
    sfi_plat_dev_entry_t entries[0];
};


struct sfi_gpio_entry {
    char     cntrl_name[16]; // controller name
    uint16_t pin_num;
    char     pin_name[16];
} __packed;

typedef struct sfi_gpio_entry sfi_gpio_entry_t;
    

struct sfi_sys_tbl* sfi_find_syst(void);
struct sys_info;
struct nk_mem_info;
unsigned sfi_get_mmap_nentries(struct sfi_mmap_tbl * tbl);
struct sfi_mmap_tbl * sfi_get_mmap(void);
long sfi_parse_phys_mem (struct nk_mem_info * mem);
int sfi_parse_syst (struct sys_info * sys, struct sfi_sys_tbl * sfi);

#endif
