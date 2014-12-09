#ifndef __ACPI_H__
#define __ACPI_H__

#include <nautilus/intrinsics.h>

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
    char    bus_type_string[48];
} __packed;

struct mp_table_entry_ioapic {
    uint8_t  type;
    uint8_t  id;
    uint8_t  version;
    uint8_t  enabled : 1;
    uint8_t  unused  : 7;
    uint32_t addr;
} __packed;

/* NOTE: for the following two, the last two
 * fields are reversed in order from the Intel 
 * MP Spec. QEMU and KVM put them in this order
 * not sure why
 */
struct mp_table_entry_lint {
    uint8_t type;
    uint8_t int_type;
    uint8_t po         : 2;
    uint8_t el         : 2;
    uint8_t src_bus_id;
    uint8_t src_bus_irq; 
    uint8_t dst_lapic_lintin;
    uint8_t dst_lapic_id;
} __packed;

struct mp_table_entry_ioint {
    uint8_t type;
    uint8_t int_type;
    uint8_t po         : 2;
    uint8_t el         : 2;
    uint8_t src_bus_id;
    uint8_t src_bus_irq; 
    uint8_t dst_ioapic_intin;
    uint8_t dst_ioapic_id;
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


struct acpi_header {
    char     sig[ACPI_NAME_LEN];
    uint32_t len;
    uint8_t  rev;
    uint8_t  cksum;
    char     oem_id[ACPI_OEM_ID_LEN];
    char     oem_tbl_id[ACPI_OEM_TABLE_ID_LEN];
    uint32_t oem_rev;
    char     asl_comp_id[ACPI_NAME_LEN];
    uint32_t asl_comp_rev;
} __packed;

struct acpi_slit {
    struct acpi_header hdr;
    uint64_t locality_count;
    uint8_t entry[1];
} __packed;
    

struct acpi_srat {
    struct   acpi_header hdr;
    uint32_t tbl_rev;
    uint64_t rsvd;
} __packed;


struct acpi_subtable_hdr {
    uint8_t type;
    uint8_t len;
} __packed;

enum acpi_srat_type {
    ACPI_SRAT_TYPE_CPU_AFFINITY = 0,
    ACPI_SRAT_TYPE_MEMORY_AFFINITY = 1,
    ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY = 2,
    ACPI_SRAT_TYPE_RESERVED = 3 /* 3 and greater are reserved */
};

/* 0: Processor Local APIC/SAPIC Affinity */

struct acpi_srat_cpu_affinity {
    struct   acpi_subtable_header header;
    uint8_t  proximity_domain_lo;
    uint8_t  apic_id;
    uint32_t flags;
    uint8_t  local_sapic_eid;
    uint8_t  proximity_domain_hi[3];
    uint32_t clk_domain;
} __packed;

/* Flags */

#define ACPI_SRAT_CPU_USE_AFFINITY  (1) /* 00: Use affinity structure */

/* 1: Memory Affinity */

struct acpi_srat_mem_affinity {
    struct acpi_subtable_header header;
    uint32_t proximity_domain;
    uint16_t reserved;       /* Reserved, must be zero */
    uint64_t base_address;
    uint64_t length;
    uint32_t reserved1;
    uint32_t flags;
    uint64_t reserved2;          /* Reserved, must be zero */
} __packed;

/* Flags */

#define ACPI_SRAT_MEM_ENABLED       (1) /* 00: Use affinity structure */
#define ACPI_SRAT_MEM_HOT_PLUGGABLE (1<<1)  /* 01: Memory region is hot pluggable */
#define ACPI_SRAT_MEM_NON_VOLATILE  (1<<2)  /* 02: Memory region is non-volatile */

/* 2: Processor Local X2_APIC Affinity (ACPI 4.0) */

struct acpi_srat_x2apic_cpu_affinity {
    struct acpi_subtable_header header;
    uint16_t reserved;       /* Reserved, must be zero */
    uint32_t proximity_domain;
    uint32_t apic_id;
    uint32_t flags;
    uint32_t clock_domain;
    uint32_t reserved2;
} __packed;

/* Flags for struct acpi_srat_cpu_affinity and struct acpi_srat_x2apic_cpu_affinity */

#define ACPI_SRAT_CPU_ENABLED       (1) /* 00: Use affinity structure */
    

#endif
