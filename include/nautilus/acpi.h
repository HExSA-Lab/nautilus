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
#ifndef __NAUT_ACPI_H__
#define __NAUT_ACPI_H__

#include <acpi/acpi.h>
#include <nautilus/intrinsics.h>
#include <nautilus/acpi-x86_64.h>

#define MP_TAB_TYPE_CPU    0
#define MP_TAB_TYPE_BUS    1
#define MP_TAB_TYPE_IOAPIC 2
#define MP_TAB_TYPE_IO_INT 3
#define MP_TAB_TYPE_LINT   4

#define MP_TAB_CPU_LEN     20
#define MP_TAB_BUS_LEN     8
#define MP_TAB_IOAPIC_LEN  8
#define MP_TAB_IO_INT_LEN  8
#define MP_TAB_LINT_LEN    8

struct mp_float_ptr_struct {
    uint32_t sig;
    uint32_t mp_cfg_ptr;
    uint8_t  len;
    uint8_t  version;
    uint8_t  checksum;
    uint8_t  mp_feat1;
    uint8_t  mp_feat2;
    uint8_t  mp_feat3;
    uint8_t  mp_feat4;
    uint8_t  mp_feat5;
} __packed;


struct mp_table_entry_cpu {
   uint8_t  type;
   uint8_t  lapic_id;
   uint8_t  lapic_version;
   uint8_t  enabled     : 1;
   uint8_t  is_bsp      : 1;
   uint8_t  unused      : 6;
   uint32_t sig;
   uint32_t feat_flags;
   uint32_t rsvd0;
   uint32_t rsvd1;
} __packed;

struct mp_table_entry_bus {
    uint8_t type;
    uint8_t bus_id; 
    char    bus_type_string[6];
} __packed;

struct mp_table_entry_ioapic {
    uint8_t  type;
    uint8_t  id;
    uint8_t  version;
    uint8_t  enabled : 1;
    uint8_t  unused  : 7;
    uint32_t addr;
} __packed;

struct mp_table_entry_lint {
    uint8_t  type;
    uint8_t  int_type;
    uint16_t int_flags;
    uint8_t  src_bus_id;
    uint8_t  src_bus_irq; 
    uint8_t  dst_lapic_id;
    uint8_t  dst_lapic_lintin;
} __packed;

struct mp_table_entry_ioint {
    uint8_t  type;
    uint8_t  int_type;
    uint16_t int_flags;
    uint8_t  src_bus_id;
    uint8_t  src_bus_irq; 
    uint8_t  dst_ioapic_id;
    uint8_t  dst_ioapic_intin;
} __packed;


struct mp_table_entry {
    union {
        struct mp_table_entry_cpu cpu;
        struct mp_table_entry_ioapic ioapic;
    } __packed;
} __packed;

struct mp_table {
    uint32_t sig;
    uint16_t len;          /* base table length */
    uint8_t  rev;          /* spec revision */
    uint8_t  cksum;        /* checksum */
    char     oem_id[8];
    char     prod_id[12];
    uint32_t oem_table_ptr;
    uint16_t oem_table_size;
    uint16_t entry_cnt;
    uint32_t lapic_addr;
    uint16_t ext_table_len;
    uint8_t  ext_table_checksum;
    uint8_t  rsvd;

    struct mp_table_entry entries[0];

} __packed;


enum acpi_irq_model_id {
	ACPI_IRQ_MODEL_PIC = 0,
	ACPI_IRQ_MODEL_IOAPIC,
	ACPI_IRQ_MODEL_IOSAPIC,
	ACPI_IRQ_MODEL_PLATFORM,
	ACPI_IRQ_MODEL_COUNT
};

extern enum acpi_irq_model_id	acpi_irq_model;

enum acpi_interrupt_id {
	ACPI_INTERRUPT_PMI	= 1,
	ACPI_INTERRUPT_INIT,
	ACPI_INTERRUPT_CPEI,
	ACPI_INTERRUPT_COUNT
};

#define	ACPI_SPACE_MEM		0

enum acpi_srat_entry_id {
	ACPI_SRAT_PROCESSOR_AFFINITY = 0,
	ACPI_SRAT_MEMORY_AFFINITY,
    ACPI_SRAT_X2APIC_CPU_AFFINITY,
	ACPI_SRAT_ENTRY_COUNT
};

enum acpi_address_range_id {
	ACPI_ADDRESS_RANGE_MEMORY = 1,
	ACPI_ADDRESS_RANGE_RESERVED = 2,
	ACPI_ADDRESS_RANGE_ACPI = 3,
	ACPI_ADDRESS_RANGE_NVS	= 4,
	ACPI_ADDRESS_RANGE_COUNT
};

typedef int (*acpi_madt_entry_handler) (struct acpi_subtable_header *header,
		        const unsigned long end);

/* Table Handlers */

typedef int (*acpi_table_handler) (struct acpi_table_header *table, void * arg);

typedef int (*acpi_table_entry_handler) (struct acpi_subtable_header *header, const unsigned long end);

#define BAD_MADT_ENTRY(entry, end) (					    \
		(!entry) || (unsigned long)entry + sizeof(*entry) > end ||  \
		((struct acpi_subtable_header *)entry)->length < sizeof(*entry))

char * __acpi_map_table (unsigned long phys_addr, unsigned long size);
void __acpi_unmap_table(char *map, unsigned long size);
int early_acpi_boot_init(void);
int acpi_boot_init (void);
void nk_acpi_init (void);
int acpi_mps_check (void);
//int acpi_parse_madt (void);
//int acpi_numa_init (void);

int acpi_table_init (void);
int acpi_table_parse (char *id, acpi_table_handler handler, void * arg);
int acpi_table_parse_entries(char *id, unsigned long table_size,
	int entry_id, acpi_table_entry_handler handler, unsigned int max_entries);
//int acpi_table_parse_madt (enum acpi_madt_type id, acpi_table_entry_handler handler, unsigned int max_entries);
int acpi_parse_mcfg (struct acpi_table_header *header);
void acpi_table_print_madt_entry (struct acpi_subtable_header *madt);

/* the following four functions are architecture-dependent */
void acpi_numa_slit_init (struct acpi_table_slit *slit);
void acpi_numa_processor_affinity_init (struct acpi_srat_cpu_affinity *pa);
void acpi_numa_x2apic_affinity_init(struct acpi_srat_x2apic_cpu_affinity *pa);
void acpi_numa_memory_affinity_init (struct acpi_srat_mem_affinity *ma);
void acpi_numa_arch_fixup(void);
void acpi_set_pmem_numa_info(void);

int acpi_register_ioapic(acpi_handle handle, uint64_t phys_addr, uint32_t gsi_base);
int acpi_unregister_ioapic(acpi_handle handle, uint32_t gsi_base);
void acpi_irq_stats_init(void);
extern uint32_t acpi_irq_handled;
extern uint32_t acpi_irq_not_handled;

extern int sbf_port;
extern unsigned long acpi_realmode_flags;

#ifdef CONFIG_X86_IO_APIC
extern int acpi_get_override_irq(uint32_t gsi, int *trigger, int *polarity);
#else
#define acpi_get_override_irq(gsi, trigger, polarity) (-1)
#endif
/*
 * This function undoes the effect of one call to acpi_register_gsi().
 * If this matches the last registration, any IRQ resources for gsi
 * are freed.
 */
void acpi_unregister_gsi (uint32_t gsi);

struct pci_dev;

int acpi_pci_irq_enable (struct pci_dev *dev);
void acpi_penalize_isa_irq(int irq, int active);

void acpi_pci_irq_disable (struct pci_dev *dev);

struct acpi_pci_driver {
	struct acpi_pci_driver *next;
	int (*add)(acpi_handle handle);
	void (*remove)(acpi_handle handle);
};

int acpi_pci_register_driver(struct acpi_pci_driver *driver);
void acpi_pci_unregister_driver(struct acpi_pci_driver *driver);

extern int ec_read(uint8_t addr, uint8_t *val);
extern int ec_write(uint8_t addr, uint8_t val);
extern int ec_transaction(uint8_t command,
                          const uint8_t *wdata, unsigned wdata_len,
                          uint8_t *rdata, unsigned rdata_len);

#define ACPI_VIDEO_OUTPUT_SWITCHING			0x0001
#define ACPI_VIDEO_DEVICE_POSTING			0x0002
#define ACPI_VIDEO_ROM_AVAILABLE			0x0004
#define ACPI_VIDEO_BACKLIGHT				0x0008
#define ACPI_VIDEO_BACKLIGHT_FORCE_VENDOR		0x0010
#define ACPI_VIDEO_BACKLIGHT_FORCE_VIDEO		0x0020
#define ACPI_VIDEO_OUTPUT_SWITCHING_FORCE_VENDOR	0x0040
#define ACPI_VIDEO_OUTPUT_SWITCHING_FORCE_VIDEO		0x0080
#define ACPI_VIDEO_BACKLIGHT_DMI_VENDOR			0x0100
#define ACPI_VIDEO_BACKLIGHT_DMI_VIDEO			0x0200
#define ACPI_VIDEO_OUTPUT_SWITCHING_DMI_VENDOR		0x0400
#define ACPI_VIDEO_OUTPUT_SWITCHING_DMI_VIDEO		0x0800

int acpi_get_pxm(acpi_handle handle);
int acpi_get_node(acpi_handle *handle);
extern int acpi_paddr_to_node(uint64_t start_addr, uint64_t size);

extern int pnpacpi_disabled;

#define PXM_INVAL	(-1)
#define NID_INVAL	(-1)

struct acpi_osc_context {
	char *uuid_str; /* uuid string */
	int rev;
	struct acpi_buffer cap; /* arg2/arg3 */
	struct acpi_buffer ret; /* free by caller if success */
};

#define OSC_QUERY_TYPE			0
#define OSC_SUPPORT_TYPE 		1
#define OSC_CONTROL_TYPE		2

/* _OSC DW0 Definition */
#define OSC_QUERY_ENABLE		1
#define OSC_REQUEST_ERROR		2
#define OSC_INVALID_UUID_ERROR		4
#define OSC_INVALID_REVISION_ERROR	8
#define OSC_CAPABILITIES_MASK_ERROR	16

acpi_status acpi_run_osc(acpi_handle handle, struct acpi_osc_context *context);

/* platform-wide _OSC bits */
#define OSC_SB_PAD_SUPPORT		1
#define OSC_SB_PPC_OST_SUPPORT		2
#define OSC_SB_PR3_SUPPORT		4
#define OSC_SB_CPUHP_OST_SUPPORT	8
#define OSC_SB_APEI_SUPPORT		16

/* PCI defined _OSC bits */
/* _OSC DW1 Definition (OS Support Fields) */
#define OSC_EXT_PCI_CONFIG_SUPPORT		1
#define OSC_ACTIVE_STATE_PWR_SUPPORT 		2
#define OSC_CLOCK_PWR_CAPABILITY_SUPPORT	4
#define OSC_PCI_SEGMENT_GROUPS_SUPPORT		8
#define OSC_MSI_SUPPORT				16
#define OSC_PCI_SUPPORT_MASKS			0x1f

/* _OSC DW1 Definition (OS Control Fields) */
#define OSC_PCI_EXPRESS_NATIVE_HP_CONTROL	1
#define OSC_SHPC_NATIVE_HP_CONTROL 		2
#define OSC_PCI_EXPRESS_PME_CONTROL		4
#define OSC_PCI_EXPRESS_AER_CONTROL		8
#define OSC_PCI_EXPRESS_CAP_STRUCTURE_CONTROL	16

#define OSC_PCI_CONTROL_MASKS 	(OSC_PCI_EXPRESS_NATIVE_HP_CONTROL | 	\
				OSC_SHPC_NATIVE_HP_CONTROL | 		\
				OSC_PCI_EXPRESS_PME_CONTROL |		\
				OSC_PCI_EXPRESS_AER_CONTROL |		\
				OSC_PCI_EXPRESS_CAP_STRUCTURE_CONTROL)
extern acpi_status acpi_pci_osc_control_set(acpi_handle handle,
					     uint32_t *mask, uint32_t req);
extern void acpi_early_init(void);

static inline void *acpi_os_ioremap(acpi_physical_address phys,
					    acpi_size size)
{
    panic("Not supported\n");
	//return ioremap(phys, size);
    return NULL;
}
    

#endif
