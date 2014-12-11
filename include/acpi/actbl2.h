/******************************************************************************
 *
 * Name: actbl2.h - ACPI Table Definitions (tables not in ACPI spec)
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef __ACTBL2_H__
#define __ACTBL2_H__

/*******************************************************************************
 *
 * Additional ACPI Tables (2)
 *
 * These tables are not consumed directly by the ACPICA subsystem, but are
 * included here to support device drivers and the AML disassembler.
 *
 * The tables in this file are defined by third-party specifications, and are
 * not defined directly by the ACPI specification itself.
 *
 ******************************************************************************/

/*
 * Values for description table header signatures for tables defined in this
 * file. Useful because they make it more difficult to inadvertently type in
 * the wrong signature.
 */
#define ACPI_SIG_ASF            "ASF!"	/* Alert Standard Format table */
#define ACPI_SIG_BOOT           "BOOT"	/* Simple Boot Flag Table */
#define ACPI_SIG_DBGP           "DBGP"	/* Debug Port table */
#define ACPI_SIG_DMAR           "DMAR"	/* DMA Remapping table */
#define ACPI_SIG_HPET           "HPET"	/* High Precision Event Timer table */
#define ACPI_SIG_IBFT           "IBFT"	/* i_sCSI Boot Firmware Table */
#define ACPI_SIG_IVRS           "IVRS"	/* I/O Virtualization Reporting Structure */
#define ACPI_SIG_MCFG           "MCFG"	/* PCI Memory Mapped Configuration table */
#define ACPI_SIG_MCHI           "MCHI"	/* Management Controller Host Interface table */
#define ACPI_SIG_SLIC           "SLIC"	/* Software Licensing Description Table */
#define ACPI_SIG_SPCR           "SPCR"	/* Serial Port Console Redirection table */
#define ACPI_SIG_SPMI           "SPMI"	/* Server Platform Management Interface table */
#define ACPI_SIG_TCPA           "TCPA"	/* Trusted Computing Platform Alliance table */
#define ACPI_SIG_UEFI           "UEFI"	/* Uefi Boot Optimization Table */
#define ACPI_SIG_WAET           "WAET"	/* Windows ACPI Emulated devices Table */
#define ACPI_SIG_WDAT           "WDAT"	/* Watchdog Action Table */
#define ACPI_SIG_WDDT           "WDDT"	/* Watchdog Timer Description Table */
#define ACPI_SIG_WDRT           "WDRT"	/* Watchdog Resource Table */

#ifdef ACPI_UNDEFINED_TABLES
/*
 * These tables have been seen in the field, but no definition has been found
 */
#define ACPI_SIG_ATKG           "ATKG"
#define ACPI_SIG_GSCI           "GSCI"	/* GMCH SCI table */
#define ACPI_SIG_IEIT           "IEIT"
#endif

/*
 * All tables must be byte-packed to match the ACPI specification, since
 * the tables are provided by the system BIOS.
 */
#pragma pack(1)

/*
 * Note about bitfields: The uint8_t type is used for bitfields in ACPI tables.
 * This is the only type that is even remotely portable. Anything else is not
 * portable, so do not use any other bitfield types.
 */

/*******************************************************************************
 *
 * ASF - Alert Standard Format table (Signature "ASF!")
 *       Revision 0x10
 *
 * Conforms to the Alert Standard Format Specification V2.0, 23 April 2003
 *
 ******************************************************************************/

struct acpi_table_asf {
	struct acpi_table_header header;	/* Common ACPI table header */
};

/* ASF subtable header */

struct acpi_asf_header {
	uint8_t type;
	uint8_t reserved;
	uint16_t length;
};

/* Values for Type field above */

enum acpi_asf_type {
	ACPI_ASF_TYPE_INFO = 0,
	ACPI_ASF_TYPE_ALERT = 1,
	ACPI_ASF_TYPE_CONTROL = 2,
	ACPI_ASF_TYPE_BOOT = 3,
	ACPI_ASF_TYPE_ADDRESS = 4,
	ACPI_ASF_TYPE_RESERVED = 5
};

/*
 * ASF subtables
 */

/* 0: ASF Information */

struct acpi_asf_info {
	struct acpi_asf_header header;
	uint8_t min_reset_value;
	uint8_t min_poll_interval;
	uint16_t system_id;
	uint32_t mfg_id;
	uint8_t flags;
	uint8_t reserved2[3];
};

/* Masks for Flags field above */

#define ACPI_ASF_SMBUS_PROTOCOLS    (1)

/* 1: ASF Alerts */

struct acpi_asf_alert {
	struct acpi_asf_header header;
	uint8_t assert_mask;
	uint8_t deassert_mask;
	uint8_t alerts;
	uint8_t data_length;
};

struct acpi_asf_alert_data {
	uint8_t address;
	uint8_t command;
	uint8_t mask;
	uint8_t value;
	uint8_t sensor_type;
	uint8_t type;
	uint8_t offset;
	uint8_t source_type;
	uint8_t severity;
	uint8_t sensor_number;
	uint8_t entity;
	uint8_t instance;
};

/* 2: ASF Remote Control */

struct acpi_asf_remote {
	struct acpi_asf_header header;
	uint8_t controls;
	uint8_t data_length;
	uint16_t reserved2;
};

struct acpi_asf_control_data {
	uint8_t function;
	uint8_t address;
	uint8_t command;
	uint8_t value;
};

/* 3: ASF RMCP Boot Options */

struct acpi_asf_rmcp {
	struct acpi_asf_header header;
	uint8_t capabilities[7];
	uint8_t completion_code;
	uint32_t enterprise_id;
	uint8_t command;
	uint16_t parameter;
	uint16_t boot_options;
	uint16_t oem_parameters;
};

/* 4: ASF Address */

struct acpi_asf_address {
	struct acpi_asf_header header;
	uint8_t eprom_address;
	uint8_t devices;
};

/*******************************************************************************
 *
 * BOOT - Simple Boot Flag Table
 *        Version 1
 *
 * Conforms to the "Simple Boot Flag Specification", Version 2.1
 *
 ******************************************************************************/

struct acpi_table_boot {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint8_t cmos_index;		/* Index in CMOS RAM for the boot register */
	uint8_t reserved[3];
};

/*******************************************************************************
 *
 * DBGP - Debug Port table
 *        Version 1
 *
 * Conforms to the "Debug Port Specification", Version 1.00, 2/9/2000
 *
 ******************************************************************************/

struct acpi_table_dbgp {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint8_t type;		/* 0=full 16550, 1=subset of 16550 */
	uint8_t reserved[3];
	struct acpi_generic_address debug_port;
};

/*******************************************************************************
 *
 * DMAR - DMA Remapping table
 *        Version 1
 *
 * Conforms to "Intel Virtualization Technology for Directed I/O",
 * Version 1.2, Sept. 2008
 *
 ******************************************************************************/

struct acpi_table_dmar {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint8_t width;		/* Host Address Width */
	uint8_t flags;
	uint8_t reserved[10];
};

/* Masks for Flags field above */

#define ACPI_DMAR_INTR_REMAP        (1)

/* DMAR subtable header */

struct acpi_dmar_header {
	uint16_t type;
	uint16_t length;
};

/* Values for subtable type in struct acpi_dmar_header */

enum acpi_dmar_type {
	ACPI_DMAR_TYPE_HARDWARE_UNIT = 0,
	ACPI_DMAR_TYPE_RESERVED_MEMORY = 1,
	ACPI_DMAR_TYPE_ATSR = 2,
	ACPI_DMAR_HARDWARE_AFFINITY = 3,
	ACPI_DMAR_TYPE_RESERVED = 4	/* 4 and greater are reserved */
};

/* DMAR Device Scope structure */

struct acpi_dmar_device_scope {
	uint8_t entry_type;
	uint8_t length;
	uint16_t reserved;
	uint8_t enumeration_id;
	uint8_t bus;
};

/* Values for entry_type in struct acpi_dmar_device_scope */

enum acpi_dmar_scope_type {
	ACPI_DMAR_SCOPE_TYPE_NOT_USED = 0,
	ACPI_DMAR_SCOPE_TYPE_ENDPOINT = 1,
	ACPI_DMAR_SCOPE_TYPE_BRIDGE = 2,
	ACPI_DMAR_SCOPE_TYPE_IOAPIC = 3,
	ACPI_DMAR_SCOPE_TYPE_HPET = 4,
	ACPI_DMAR_SCOPE_TYPE_RESERVED = 5	/* 5 and greater are reserved */
};

struct acpi_dmar_pci_path {
	uint8_t dev;
	uint8_t fn;
};

/*
 * DMAR Sub-tables, correspond to Type in struct acpi_dmar_header
 */

/* 0: Hardware Unit Definition */

struct acpi_dmar_hardware_unit {
	struct acpi_dmar_header header;
	uint8_t flags;
	uint8_t reserved;
	uint16_t segment;
	uint64_t address;		/* Register Base Address */
};

/* Masks for Flags field above */

#define ACPI_DMAR_INCLUDE_ALL       (1)

/* 1: Reserved Memory Defininition */

struct acpi_dmar_reserved_memory {
	struct acpi_dmar_header header;
	uint16_t reserved;
	uint16_t segment;
	uint64_t base_address;	/* 4_k aligned base address */
	uint64_t end_address;	/* 4_k aligned limit address */
};

/* Masks for Flags field above */

#define ACPI_DMAR_ALLOW_ALL         (1)

/* 2: Root Port ATS Capability Reporting Structure */

struct acpi_dmar_atsr {
	struct acpi_dmar_header header;
	uint8_t flags;
	uint8_t reserved;
	uint16_t segment;
};

/* Masks for Flags field above */

#define ACPI_DMAR_ALL_PORTS         (1)

/* 3: Remapping Hardware Static Affinity Structure */

struct acpi_dmar_rhsa {
	struct acpi_dmar_header header;
	uint32_t reserved;
	uint64_t base_address;
	uint32_t proximity_domain;
};

/*******************************************************************************
 *
 * HPET - High Precision Event Timer table
 *        Version 1
 *
 * Conforms to "IA-PC HPET (High Precision Event Timers) Specification",
 * Version 1.0a, October 2004
 *
 ******************************************************************************/

struct acpi_table_hpet {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint32_t id;			/* Hardware ID of event timer block */
	struct acpi_generic_address address;	/* Address of event timer block */
	uint8_t sequence;		/* HPET sequence number */
	uint16_t minimum_tick;	/* Main counter min tick, periodic mode */
	uint8_t flags;
};

/* Masks for Flags field above */

#define ACPI_HPET_PAGE_PROTECT_MASK (3)

/* Values for Page Protect flags */

enum acpi_hpet_page_protect {
	ACPI_HPET_NO_PAGE_PROTECT = 0,
	ACPI_HPET_PAGE_PROTECT4 = 1,
	ACPI_HPET_PAGE_PROTECT64 = 2
};

/*******************************************************************************
 *
 * IBFT - Boot Firmware Table
 *        Version 1
 *
 * Conforms to "iSCSI Boot Firmware Table (iBFT) as Defined in ACPI 3.0b
 * Specification", Version 1.01, March 1, 2007
 *
 * Note: It appears that this table is not intended to appear in the RSDT/XSDT.
 * Therefore, it is not currently supported by the disassembler.
 *
 ******************************************************************************/

struct acpi_table_ibft {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint8_t reserved[12];
};

/* IBFT common subtable header */

struct acpi_ibft_header {
	uint8_t type;
	uint8_t version;
	uint16_t length;
	uint8_t index;
	uint8_t flags;
};

/* Values for Type field above */

enum acpi_ibft_type {
	ACPI_IBFT_TYPE_NOT_USED = 0,
	ACPI_IBFT_TYPE_CONTROL = 1,
	ACPI_IBFT_TYPE_INITIATOR = 2,
	ACPI_IBFT_TYPE_NIC = 3,
	ACPI_IBFT_TYPE_TARGET = 4,
	ACPI_IBFT_TYPE_EXTENSIONS = 5,
	ACPI_IBFT_TYPE_RESERVED = 6	/* 6 and greater are reserved */
};

/* IBFT subtables */

struct acpi_ibft_control {
	struct acpi_ibft_header header;
	uint16_t extensions;
	uint16_t initiator_offset;
	uint16_t nic0_offset;
	uint16_t target0_offset;
	uint16_t nic1_offset;
	uint16_t target1_offset;
};

struct acpi_ibft_initiator {
	struct acpi_ibft_header header;
	uint8_t sns_server[16];
	uint8_t slp_server[16];
	uint8_t primary_server[16];
	uint8_t secondary_server[16];
	uint16_t name_length;
	uint16_t name_offset;
};

struct acpi_ibft_nic {
	struct acpi_ibft_header header;
	uint8_t ip_address[16];
	uint8_t subnet_mask_prefix;
	uint8_t origin;
	uint8_t gateway[16];
	uint8_t primary_dns[16];
	uint8_t secondary_dns[16];
	uint8_t dhcp[16];
	uint16_t vlan;
	uint8_t mac_address[6];
	uint16_t pci_address;
	uint16_t name_length;
	uint16_t name_offset;
};

struct acpi_ibft_target {
	struct acpi_ibft_header header;
	uint8_t target_ip_address[16];
	uint16_t target_ip_socket;
	uint8_t target_boot_lun[8];
	uint8_t chap_type;
	uint8_t nic_association;
	uint16_t target_name_length;
	uint16_t target_name_offset;
	uint16_t chap_name_length;
	uint16_t chap_name_offset;
	uint16_t chap_secret_length;
	uint16_t chap_secret_offset;
	uint16_t reverse_chap_name_length;
	uint16_t reverse_chap_name_offset;
	uint16_t reverse_chap_secret_length;
	uint16_t reverse_chap_secret_offset;
};

/*******************************************************************************
 *
 * IVRS - I/O Virtualization Reporting Structure
 *        Version 1
 *
 * Conforms to "AMD I/O Virtualization Technology (IOMMU) Specification",
 * Revision 1.26, February 2009.
 *
 ******************************************************************************/

struct acpi_table_ivrs {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint32_t info;		/* Common virtualization info */
	uint64_t reserved;
};

/* Values for Info field above */

#define ACPI_IVRS_PHYSICAL_SIZE     0x00007F00	/* 7 bits, physical address size */
#define ACPI_IVRS_VIRTUAL_SIZE      0x003F8000	/* 7 bits, virtual address size */
#define ACPI_IVRS_ATS_RESERVED      0x00400000	/* ATS address translation range reserved */

/* IVRS subtable header */

struct acpi_ivrs_header {
	uint8_t type;		/* Subtable type */
	uint8_t flags;
	uint16_t length;		/* Subtable length */
	uint16_t device_id;		/* ID of IOMMU */
};

/* Values for subtable Type above */

enum acpi_ivrs_type {
	ACPI_IVRS_TYPE_HARDWARE = 0x10,
	ACPI_IVRS_TYPE_MEMORY1 = 0x20,
	ACPI_IVRS_TYPE_MEMORY2 = 0x21,
	ACPI_IVRS_TYPE_MEMORY3 = 0x22
};

/* Masks for Flags field above for IVHD subtable */

#define ACPI_IVHD_TT_ENABLE         (1)
#define ACPI_IVHD_PASS_PW           (1<<1)
#define ACPI_IVHD_RES_PASS_PW       (1<<2)
#define ACPI_IVHD_ISOC              (1<<3)
#define ACPI_IVHD_IOTLB             (1<<4)

/* Masks for Flags field above for IVMD subtable */

#define ACPI_IVMD_UNITY             (1)
#define ACPI_IVMD_READ              (1<<1)
#define ACPI_IVMD_WRITE             (1<<2)
#define ACPI_IVMD_EXCLUSION_RANGE   (1<<3)

/*
 * IVRS subtables, correspond to Type in struct acpi_ivrs_header
 */

/* 0x10: I/O Virtualization Hardware Definition Block (IVHD) */

struct acpi_ivrs_hardware {
	struct acpi_ivrs_header header;
	uint16_t capability_offset;	/* Offset for IOMMU control fields */
	uint64_t base_address;	/* IOMMU control registers */
	uint16_t pci_segment_group;
	uint16_t info;		/* MSI number and unit ID */
	uint32_t reserved;
};

/* Masks for Info field above */

#define ACPI_IVHD_MSI_NUMBER_MASK   0x001F	/* 5 bits, MSI message number */
#define ACPI_IVHD_UNIT_ID_MASK      0x1F00	/* 5 bits, unit_iD */

/*
 * Device Entries for IVHD subtable, appear after struct acpi_ivrs_hardware structure.
 * Upper two bits of the Type field are the (encoded) length of the structure.
 * Currently, only 4 and 8 byte entries are defined. 16 and 32 byte entries
 * are reserved for future use but not defined.
 */
struct acpi_ivrs_de_header {
	uint8_t type;
	uint16_t id;
	uint8_t data_setting;
};

/* Length of device entry is in the top two bits of Type field above */

#define ACPI_IVHD_ENTRY_LENGTH      0xC0

/* Values for device entry Type field above */

enum acpi_ivrs_device_entry_type {
	/* 4-byte device entries, all use struct acpi_ivrs_device4 */

	ACPI_IVRS_TYPE_PAD4 = 0,
	ACPI_IVRS_TYPE_ALL = 1,
	ACPI_IVRS_TYPE_SELECT = 2,
	ACPI_IVRS_TYPE_START = 3,
	ACPI_IVRS_TYPE_END = 4,

	/* 8-byte device entries */

	ACPI_IVRS_TYPE_PAD8 = 64,
	ACPI_IVRS_TYPE_NOT_USED = 65,
	ACPI_IVRS_TYPE_ALIAS_SELECT = 66,	/* Uses struct acpi_ivrs_device8a */
	ACPI_IVRS_TYPE_ALIAS_START = 67,	/* Uses struct acpi_ivrs_device8a */
	ACPI_IVRS_TYPE_EXT_SELECT = 70,	/* Uses struct acpi_ivrs_device8b */
	ACPI_IVRS_TYPE_EXT_START = 71,	/* Uses struct acpi_ivrs_device8b */
	ACPI_IVRS_TYPE_SPECIAL = 72	/* Uses struct acpi_ivrs_device8c */
};

/* Values for Data field above */

#define ACPI_IVHD_INIT_PASS         (1)
#define ACPI_IVHD_EINT_PASS         (1<<1)
#define ACPI_IVHD_NMI_PASS          (1<<2)
#define ACPI_IVHD_SYSTEM_MGMT       (3<<4)
#define ACPI_IVHD_LINT0_PASS        (1<<6)
#define ACPI_IVHD_LINT1_PASS        (1<<7)

/* Types 0-4: 4-byte device entry */

struct acpi_ivrs_device4 {
	struct acpi_ivrs_de_header header;
};

/* Types 66-67: 8-byte device entry */

struct acpi_ivrs_device8a {
	struct acpi_ivrs_de_header header;
	uint8_t reserved1;
	uint16_t used_id;
	uint8_t reserved2;
};

/* Types 70-71: 8-byte device entry */

struct acpi_ivrs_device8b {
	struct acpi_ivrs_de_header header;
	uint32_t extended_data;
};

/* Values for extended_data above */

#define ACPI_IVHD_ATS_DISABLED      (1<<31)

/* Type 72: 8-byte device entry */

struct acpi_ivrs_device8c {
	struct acpi_ivrs_de_header header;
	uint8_t handle;
	uint16_t used_id;
	uint8_t variety;
};

/* Values for Variety field above */

#define ACPI_IVHD_IOAPIC            1
#define ACPI_IVHD_HPET              2

/* 0x20, 0x21, 0x22: I/O Virtualization Memory Definition Block (IVMD) */

struct acpi_ivrs_memory {
	struct acpi_ivrs_header header;
	uint16_t aux_data;
	uint64_t reserved;
	uint64_t start_address;
	uint64_t memory_length;
};

/*******************************************************************************
 *
 * MCFG - PCI Memory Mapped Configuration table and sub-table
 *        Version 1
 *
 * Conforms to "PCI Firmware Specification", Revision 3.0, June 20, 2005
 *
 ******************************************************************************/

struct acpi_table_mcfg {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint8_t reserved[8];
};

/* Subtable */

struct acpi_mcfg_allocation {
	uint64_t address;		/* Base address, processor-relative */
	uint16_t pci_segment;	/* PCI segment group number */
	uint8_t start_bus_number;	/* Starting PCI Bus number */
	uint8_t end_bus_number;	/* Final PCI Bus number */
	uint32_t reserved;
};

/*******************************************************************************
 *
 * MCHI - Management Controller Host Interface Table
 *        Version 1
 *
 * Conforms to "Management Component Transport Protocol (MCTP) Host
 * Interface Specification", Revision 1.0.0a, October 13, 2009
 *
 ******************************************************************************/

struct acpi_table_mchi {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint8_t interface_type;
	uint8_t protocol;
	uint64_t protocol_data;
	uint8_t interrupt_type;
	uint8_t gpe;
	uint8_t pci_device_flag;
	uint32_t global_interrupt;
	struct acpi_generic_address control_register;
	uint8_t pci_segment;
	uint8_t pci_bus;
	uint8_t pci_device;
	uint8_t pci_function;
};

/*******************************************************************************
 *
 * SLIC - Software Licensing Description Table
 *        Version 1
 *
 * Conforms to "OEM Activation 2.0 for Windows Vista Operating Systems",
 * Copyright 2006
 *
 ******************************************************************************/

/* Basic SLIC table is only the common ACPI header */

struct acpi_table_slic {
	struct acpi_table_header header;	/* Common ACPI table header */
};

/* Common SLIC subtable header */

struct acpi_slic_header {
	uint32_t type;
	uint32_t length;
};

/* Values for Type field above */

enum acpi_slic_type {
	ACPI_SLIC_TYPE_PUBLIC_KEY = 0,
	ACPI_SLIC_TYPE_WINDOWS_MARKER = 1,
	ACPI_SLIC_TYPE_RESERVED = 2	/* 2 and greater are reserved */
};

/*
 * SLIC Sub-tables, correspond to Type in struct acpi_slic_header
 */

/* 0: Public Key Structure */

struct acpi_slic_key {
	struct acpi_slic_header header;
	uint8_t key_type;
	uint8_t version;
	uint16_t reserved;
	uint32_t algorithm;
	char magic[4];
	uint32_t bit_length;
	uint32_t exponent;
	uint8_t modulus[128];
};

/* 1: Windows Marker Structure */

struct acpi_slic_marker {
	struct acpi_slic_header header;
	uint32_t version;
	char oem_id[ACPI_OEM_ID_SIZE];	/* ASCII OEM identification */
	char oem_table_id[ACPI_OEM_TABLE_ID_SIZE];	/* ASCII OEM table identification */
	char windows_flag[8];
	uint32_t slic_version;
	uint8_t reserved[16];
	uint8_t signature[128];
};

/*******************************************************************************
 *
 * SPCR - Serial Port Console Redirection table
 *        Version 1
 *
 * Conforms to "Serial Port Console Redirection Table",
 * Version 1.00, January 11, 2002
 *
 ******************************************************************************/

struct acpi_table_spcr {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint8_t interface_type;	/* 0=full 16550, 1=subset of 16550 */
	uint8_t reserved[3];
	struct acpi_generic_address serial_port;
	uint8_t interrupt_type;
	uint8_t pc_interrupt;
	uint32_t interrupt;
	uint8_t baud_rate;
	uint8_t parity;
	uint8_t stop_bits;
	uint8_t flow_control;
	uint8_t terminal_type;
	uint8_t reserved1;
	uint16_t pci_device_id;
	uint16_t pci_vendor_id;
	uint8_t pci_bus;
	uint8_t pci_device;
	uint8_t pci_function;
	uint32_t pci_flags;
	uint8_t pci_segment;
	uint32_t reserved2;
};

/* Masks for pci_flags field above */

#define ACPI_SPCR_DO_NOT_DISABLE    (1)

/*******************************************************************************
 *
 * SPMI - Server Platform Management Interface table
 *        Version 5
 *
 * Conforms to "Intelligent Platform Management Interface Specification
 * Second Generation v2.0", Document Revision 1.0, February 12, 2004 with
 * June 12, 2009 markup.
 *
 ******************************************************************************/

struct acpi_table_spmi {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint8_t interface_type;
	uint8_t reserved;		/* Must be 1 */
	uint16_t spec_revision;	/* Version of IPMI */
	uint8_t interrupt_type;
	uint8_t gpe_number;		/* GPE assigned */
	uint8_t reserved1;
	uint8_t pci_device_flag;
	uint32_t interrupt;
	struct acpi_generic_address ipmi_register;
	uint8_t pci_segment;
	uint8_t pci_bus;
	uint8_t pci_device;
	uint8_t pci_function;
	uint8_t reserved2;
};

/* Values for interface_type above */

enum acpi_spmi_interface_types {
	ACPI_SPMI_NOT_USED = 0,
	ACPI_SPMI_KEYBOARD = 1,
	ACPI_SPMI_SMI = 2,
	ACPI_SPMI_BLOCK_TRANSFER = 3,
	ACPI_SPMI_SMBUS = 4,
	ACPI_SPMI_RESERVED = 5	/* 5 and above are reserved */
};

/*******************************************************************************
 *
 * TCPA - Trusted Computing Platform Alliance table
 *        Version 1
 *
 * Conforms to "TCG PC Specific Implementation Specification",
 * Version 1.1, August 18, 2003
 *
 ******************************************************************************/

struct acpi_table_tcpa {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint16_t reserved;
	uint32_t max_log_length;	/* Maximum length for the event log area */
	uint64_t log_address;	/* Address of the event log area */
};

/*******************************************************************************
 *
 * UEFI - UEFI Boot optimization Table
 *        Version 1
 *
 * Conforms to "Unified Extensible Firmware Interface Specification",
 * Version 2.3, May 8, 2009
 *
 ******************************************************************************/

struct acpi_table_uefi {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint8_t identifier[16];	/* UUID identifier */
	uint16_t data_offset;	/* Offset of remaining data in table */
};

/*******************************************************************************
 *
 * WAET - Windows ACPI Emulated devices Table
 *        Version 1
 *
 * Conforms to "Windows ACPI Emulated Devices Table", version 1.0, April 6, 2009
 *
 ******************************************************************************/

struct acpi_table_waet {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint32_t flags;
};

/* Masks for Flags field above */

#define ACPI_WAET_RTC_NO_ACK        (1)	/* RTC requires no int acknowledge */
#define ACPI_WAET_TIMER_ONE_READ    (1<<1)	/* PM timer requires only one read */

/*******************************************************************************
 *
 * WDAT - Watchdog Action Table
 *        Version 1
 *
 * Conforms to "Hardware Watchdog Timers Design Specification",
 * Copyright 2006 Microsoft Corporation.
 *
 ******************************************************************************/

struct acpi_table_wdat {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint32_t header_length;	/* Watchdog Header Length */
	uint16_t pci_segment;	/* PCI Segment number */
	uint8_t pci_bus;		/* PCI Bus number */
	uint8_t pci_device;		/* PCI Device number */
	uint8_t pci_function;	/* PCI Function number */
	uint8_t reserved[3];
	uint32_t timer_period;	/* Period of one timer count (msec) */
	uint32_t max_count;		/* Maximum counter value supported */
	uint32_t min_count;		/* Minimum counter value */
	uint8_t flags;
	uint8_t reserved2[3];
	uint32_t entries;		/* Number of watchdog entries that follow */
};

/* Masks for Flags field above */

#define ACPI_WDAT_ENABLED           (1)
#define ACPI_WDAT_STOPPED           0x80

/* WDAT Instruction Entries (actions) */

struct acpi_wdat_entry {
	uint8_t action;
	uint8_t instruction;
	uint16_t reserved;
	struct acpi_generic_address register_region;
	uint32_t value;		/* Value used with Read/Write register */
	uint32_t mask;		/* Bitmask required for this register instruction */
};

/* Values for Action field above */

enum acpi_wdat_actions {
	ACPI_WDAT_RESET = 1,
	ACPI_WDAT_GET_CURRENT_COUNTDOWN = 4,
	ACPI_WDAT_GET_COUNTDOWN = 5,
	ACPI_WDAT_SET_COUNTDOWN = 6,
	ACPI_WDAT_GET_RUNNING_STATE = 8,
	ACPI_WDAT_SET_RUNNING_STATE = 9,
	ACPI_WDAT_GET_STOPPED_STATE = 10,
	ACPI_WDAT_SET_STOPPED_STATE = 11,
	ACPI_WDAT_GET_REBOOT = 16,
	ACPI_WDAT_SET_REBOOT = 17,
	ACPI_WDAT_GET_SHUTDOWN = 18,
	ACPI_WDAT_SET_SHUTDOWN = 19,
	ACPI_WDAT_GET_STATUS = 32,
	ACPI_WDAT_SET_STATUS = 33,
	ACPI_WDAT_ACTION_RESERVED = 34	/* 34 and greater are reserved */
};

/* Values for Instruction field above */

enum acpi_wdat_instructions {
	ACPI_WDAT_READ_VALUE = 0,
	ACPI_WDAT_READ_COUNTDOWN = 1,
	ACPI_WDAT_WRITE_VALUE = 2,
	ACPI_WDAT_WRITE_COUNTDOWN = 3,
	ACPI_WDAT_INSTRUCTION_RESERVED = 4,	/* 4 and greater are reserved */
	ACPI_WDAT_PRESERVE_REGISTER = 0x80	/* Except for this value */
};

/*******************************************************************************
 *
 * WDDT - Watchdog Descriptor Table
 *        Version 1
 *
 * Conforms to "Using the Intel ICH Family Watchdog Timer (WDT)",
 * Version 001, September 2002
 *
 ******************************************************************************/

struct acpi_table_wddt {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint16_t spec_version;
	uint16_t table_version;
	uint16_t pci_vendor_id;
	struct acpi_generic_address address;
	uint16_t max_count;		/* Maximum counter value supported */
	uint16_t min_count;		/* Minimum counter value supported */
	uint16_t period;
	uint16_t status;
	uint16_t capability;
};

/* Flags for Status field above */

#define ACPI_WDDT_AVAILABLE     (1)
#define ACPI_WDDT_ACTIVE        (1<<1)
#define ACPI_WDDT_TCO_OS_OWNED  (1<<2)
#define ACPI_WDDT_USER_RESET    (1<<11)
#define ACPI_WDDT_WDT_RESET     (1<<12)
#define ACPI_WDDT_POWER_FAIL    (1<<13)
#define ACPI_WDDT_UNKNOWN_RESET (1<<14)

/* Flags for Capability field above */

#define ACPI_WDDT_AUTO_RESET    (1)
#define ACPI_WDDT_ALERT_SUPPORT (1<<1)

/*******************************************************************************
 *
 * WDRT - Watchdog Resource Table
 *        Version 1
 *
 * Conforms to "Watchdog Timer Hardware Requirements for Windows Server 2003",
 * Version 1.01, August 28, 2006
 *
 ******************************************************************************/

struct acpi_table_wdrt {
	struct acpi_table_header header;	/* Common ACPI table header */
	struct acpi_generic_address control_register;
	struct acpi_generic_address count_register;
	uint16_t pci_device_id;
	uint16_t pci_vendor_id;
	uint8_t pci_bus;		/* PCI Bus number */
	uint8_t pci_device;		/* PCI Device number */
	uint8_t pci_function;	/* PCI Function number */
	uint8_t pci_segment;		/* PCI Segment number */
	uint16_t max_count;		/* Maximum counter value supported */
	uint8_t units;
};

/* Reset to default packing */

#pragma pack()

#endif				/* __ACTBL2_H__ */
