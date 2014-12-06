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

#endif
