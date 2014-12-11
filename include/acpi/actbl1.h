/******************************************************************************
 *
 * Name: actbl1.h - Additional ACPI table definitions
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

#ifndef __ACTBL1_H__
#define __ACTBL1_H__

/*******************************************************************************
 *
 * Additional ACPI Tables (1)
 *
 * These tables are not consumed directly by the ACPICA subsystem, but are
 * included here to support device drivers and the AML disassembler.
 *
 * The tables in this file are fully defined within the ACPI specification.
 *
 ******************************************************************************/

/*
 * Values for description table header signatures for tables defined in this
 * file. Useful because they make it more difficult to inadvertently type in
 * the wrong signature.
 */
#define ACPI_SIG_BERT           "BERT"	/* Boot Error Record Table */
#define ACPI_SIG_CPEP           "CPEP"	/* Corrected Platform Error Polling table */
#define ACPI_SIG_ECDT           "ECDT"	/* Embedded Controller Boot Resources Table */
#define ACPI_SIG_EINJ           "EINJ"	/* Error Injection table */
#define ACPI_SIG_ERST           "ERST"	/* Error Record Serialization Table */
#define ACPI_SIG_HEST           "HEST"	/* Hardware Error Source Table */
#define ACPI_SIG_MADT           "APIC"	/* Multiple APIC Description Table */
#define ACPI_SIG_MSCT           "MSCT"	/* Maximum System Characteristics Table */
#define ACPI_SIG_SBST           "SBST"	/* Smart Battery Specification Table */
#define ACPI_SIG_SLIT           "SLIT"	/* System Locality Distance Information Table */
#define ACPI_SIG_SRAT           "SRAT"	/* System Resource Affinity Table */

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
 * Common subtable headers
 *
 ******************************************************************************/

/* Generic subtable header (used in MADT, SRAT, etc.) */

struct acpi_subtable_header {
	uint8_t type;
	uint8_t length;
};

/* Subtable header for WHEA tables (EINJ, ERST, WDAT) */

struct acpi_whea_header {
	uint8_t action;
	uint8_t instruction;
	uint8_t flags;
	uint8_t reserved;
	struct acpi_generic_address register_region;
	uint64_t value;		/* Value used with Read/Write register */
	uint64_t mask;		/* Bitmask required for this register instruction */
};

/*******************************************************************************
 *
 * BERT - Boot Error Record Table (ACPI 4.0)
 *        Version 1
 *
 ******************************************************************************/

struct acpi_table_bert {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint32_t region_length;	/* Length of the boot error region */
	uint64_t address;		/* Physical address of the error region */
};

/* Boot Error Region (not a subtable, pointed to by Address field above) */

struct acpi_bert_region {
	uint32_t block_status;	/* Type of error information */
	uint32_t raw_data_offset;	/* Offset to raw error data */
	uint32_t raw_data_length;	/* Length of raw error data */
	uint32_t data_length;	/* Length of generic error data */
	uint32_t error_severity;	/* Severity code */
};

/* Values for block_status flags above */

#define ACPI_BERT_UNCORRECTABLE             (1)
#define ACPI_BERT_CORRECTABLE               (1<<1)
#define ACPI_BERT_MULTIPLE_UNCORRECTABLE    (1<<2)
#define ACPI_BERT_MULTIPLE_CORRECTABLE      (1<<3)
#define ACPI_BERT_ERROR_ENTRY_COUNT         (0xFF<<4)	/* 8 bits, error count */

/* Values for error_severity above */

enum acpi_bert_error_severity {
	ACPI_BERT_ERROR_CORRECTABLE = 0,
	ACPI_BERT_ERROR_FATAL = 1,
	ACPI_BERT_ERROR_CORRECTED = 2,
	ACPI_BERT_ERROR_NONE = 3,
	ACPI_BERT_ERROR_RESERVED = 4	/* 4 and greater are reserved */
};

/*
 * Note: The generic error data that follows the error_severity field above
 * uses the struct acpi_hest_generic_data defined under the HEST table below
 */

/*******************************************************************************
 *
 * CPEP - Corrected Platform Error Polling table (ACPI 4.0)
 *        Version 1
 *
 ******************************************************************************/

struct acpi_table_cpep {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint64_t reserved;
};

/* Subtable */

struct acpi_cpep_polling {
	struct acpi_subtable_header header;
	uint8_t id;			/* Processor ID */
	uint8_t eid;			/* Processor EID */
	uint32_t interval;		/* Polling interval (msec) */
};

/*******************************************************************************
 *
 * ECDT - Embedded Controller Boot Resources Table
 *        Version 1
 *
 ******************************************************************************/

struct acpi_table_ecdt {
	struct acpi_table_header header;	/* Common ACPI table header */
	struct acpi_generic_address control;	/* Address of EC command/status register */
	struct acpi_generic_address data;	/* Address of EC data register */
	uint32_t uid;		/* Unique ID - must be same as the EC _UID method */
	uint8_t gpe;			/* The GPE for the EC */
	uint8_t id[1];		/* Full namepath of the EC in the ACPI namespace */
};

/*******************************************************************************
 *
 * EINJ - Error Injection Table (ACPI 4.0)
 *        Version 1
 *
 ******************************************************************************/

struct acpi_table_einj {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint32_t header_length;
	uint8_t flags;
	uint8_t reserved[3];
	uint32_t entries;
};

/* EINJ Injection Instruction Entries (actions) */

struct acpi_einj_entry {
	struct acpi_whea_header whea_header;	/* Common header for WHEA tables */
};

/* Masks for Flags field above */

#define ACPI_EINJ_PRESERVE          (1)

/* Values for Action field above */

enum acpi_einj_actions {
	ACPI_EINJ_BEGIN_OPERATION = 0,
	ACPI_EINJ_GET_TRIGGER_TABLE = 1,
	ACPI_EINJ_SET_ERROR_TYPE = 2,
	ACPI_EINJ_GET_ERROR_TYPE = 3,
	ACPI_EINJ_END_OPERATION = 4,
	ACPI_EINJ_EXECUTE_OPERATION = 5,
	ACPI_EINJ_CHECK_BUSY_STATUS = 6,
	ACPI_EINJ_GET_COMMAND_STATUS = 7,
	ACPI_EINJ_ACTION_RESERVED = 8,	/* 8 and greater are reserved */
	ACPI_EINJ_TRIGGER_ERROR = 0xFF	/* Except for this value */
};

/* Values for Instruction field above */

enum acpi_einj_instructions {
	ACPI_EINJ_READ_REGISTER = 0,
	ACPI_EINJ_READ_REGISTER_VALUE = 1,
	ACPI_EINJ_WRITE_REGISTER = 2,
	ACPI_EINJ_WRITE_REGISTER_VALUE = 3,
	ACPI_EINJ_NOOP = 4,
	ACPI_EINJ_INSTRUCTION_RESERVED = 5	/* 5 and greater are reserved */
};

/* EINJ Trigger Error Action Table */

struct acpi_einj_trigger {
	uint32_t header_size;
	uint32_t revision;
	uint32_t table_size;
	uint32_t entry_count;
};

/* Command status return values */

enum acpi_einj_command_status {
	ACPI_EINJ_SUCCESS = 0,
	ACPI_EINJ_FAILURE = 1,
	ACPI_EINJ_INVALID_ACCESS = 2,
	ACPI_EINJ_STATUS_RESERVED = 3	/* 3 and greater are reserved */
};

/* Error types returned from ACPI_EINJ_GET_ERROR_TYPE (bitfield) */

#define ACPI_EINJ_PROCESSOR_CORRECTABLE     (1)
#define ACPI_EINJ_PROCESSOR_UNCORRECTABLE   (1<<1)
#define ACPI_EINJ_PROCESSOR_FATAL           (1<<2)
#define ACPI_EINJ_MEMORY_CORRECTABLE        (1<<3)
#define ACPI_EINJ_MEMORY_UNCORRECTABLE      (1<<4)
#define ACPI_EINJ_MEMORY_FATAL              (1<<5)
#define ACPI_EINJ_PCIX_CORRECTABLE          (1<<6)
#define ACPI_EINJ_PCIX_UNCORRECTABLE        (1<<7)
#define ACPI_EINJ_PCIX_FATAL                (1<<8)
#define ACPI_EINJ_PLATFORM_CORRECTABLE      (1<<9)
#define ACPI_EINJ_PLATFORM_UNCORRECTABLE    (1<<10)
#define ACPI_EINJ_PLATFORM_FATAL            (1<<11)

/*******************************************************************************
 *
 * ERST - Error Record Serialization Table (ACPI 4.0)
 *        Version 1
 *
 ******************************************************************************/

struct acpi_table_erst {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint32_t header_length;
	uint32_t reserved;
	uint32_t entries;
};

/* ERST Serialization Entries (actions) */

struct acpi_erst_entry {
	struct acpi_whea_header whea_header;	/* Common header for WHEA tables */
};

/* Masks for Flags field above */

#define ACPI_ERST_PRESERVE          (1)

/* Values for Action field above */

enum acpi_erst_actions {
	ACPI_ERST_BEGIN_WRITE = 0,
	ACPI_ERST_BEGIN_READ = 1,
	ACPI_ERST_BEGIN_CLEAR = 2,
	ACPI_ERST_END = 3,
	ACPI_ERST_SET_RECORD_OFFSET = 4,
	ACPI_ERST_EXECUTE_OPERATION = 5,
	ACPI_ERST_CHECK_BUSY_STATUS = 6,
	ACPI_ERST_GET_COMMAND_STATUS = 7,
	ACPI_ERST_GET_RECORD_ID = 8,
	ACPI_ERST_SET_RECORD_ID = 9,
	ACPI_ERST_GET_RECORD_COUNT = 10,
	ACPI_ERST_BEGIN_DUMMY_WRIITE = 11,
	ACPI_ERST_NOT_USED = 12,
	ACPI_ERST_GET_ERROR_RANGE = 13,
	ACPI_ERST_GET_ERROR_LENGTH = 14,
	ACPI_ERST_GET_ERROR_ATTRIBUTES = 15,
	ACPI_ERST_ACTION_RESERVED = 16	/* 16 and greater are reserved */
};

/* Values for Instruction field above */

enum acpi_erst_instructions {
	ACPI_ERST_READ_REGISTER = 0,
	ACPI_ERST_READ_REGISTER_VALUE = 1,
	ACPI_ERST_WRITE_REGISTER = 2,
	ACPI_ERST_WRITE_REGISTER_VALUE = 3,
	ACPI_ERST_NOOP = 4,
	ACPI_ERST_LOAD_VAR1 = 5,
	ACPI_ERST_LOAD_VAR2 = 6,
	ACPI_ERST_STORE_VAR1 = 7,
	ACPI_ERST_ADD = 8,
	ACPI_ERST_SUBTRACT = 9,
	ACPI_ERST_ADD_VALUE = 10,
	ACPI_ERST_SUBTRACT_VALUE = 11,
	ACPI_ERST_STALL = 12,
	ACPI_ERST_STALL_WHILE_TRUE = 13,
	ACPI_ERST_SKIP_NEXT_IF_TRUE = 14,
	ACPI_ERST_GOTO = 15,
	ACPI_ERST_SET_SRC_ADDRESS_BASE = 16,
	ACPI_ERST_SET_DST_ADDRESS_BASE = 17,
	ACPI_ERST_MOVE_DATA = 18,
	ACPI_ERST_INSTRUCTION_RESERVED = 19	/* 19 and greater are reserved */
};

/* Command status return values */

enum acpi_erst_command_status {
	ACPI_ERST_SUCESS = 0,
	ACPI_ERST_NO_SPACE = 1,
	ACPI_ERST_NOT_AVAILABLE = 2,
	ACPI_ERST_FAILURE = 3,
	ACPI_ERST_RECORD_EMPTY = 4,
	ACPI_ERST_NOT_FOUND = 5,
	ACPI_ERST_STATUS_RESERVED = 6	/* 6 and greater are reserved */
};

/* Error Record Serialization Information */

struct acpi_erst_info {
	uint16_t signature;		/* Should be "ER" */
	uint8_t data[48];
};

/*******************************************************************************
 *
 * HEST - Hardware Error Source Table (ACPI 4.0)
 *        Version 1
 *
 ******************************************************************************/

struct acpi_table_hest {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint32_t error_source_count;
};

/* HEST subtable header */

struct acpi_hest_header {
	uint16_t type;
	uint16_t source_id;
};

/* Values for Type field above for subtables */

enum acpi_hest_types {
	ACPI_HEST_TYPE_IA32_CHECK = 0,
	ACPI_HEST_TYPE_IA32_CORRECTED_CHECK = 1,
	ACPI_HEST_TYPE_IA32_NMI = 2,
	ACPI_HEST_TYPE_NOT_USED3 = 3,
	ACPI_HEST_TYPE_NOT_USED4 = 4,
	ACPI_HEST_TYPE_NOT_USED5 = 5,
	ACPI_HEST_TYPE_AER_ROOT_PORT = 6,
	ACPI_HEST_TYPE_AER_ENDPOINT = 7,
	ACPI_HEST_TYPE_AER_BRIDGE = 8,
	ACPI_HEST_TYPE_GENERIC_ERROR = 9,
	ACPI_HEST_TYPE_RESERVED = 10	/* 10 and greater are reserved */
};

/*
 * HEST substructures contained in subtables
 */

/*
 * IA32 Error Bank(s) - Follows the struct acpi_hest_ia_machine_check and
 * struct acpi_hest_ia_corrected structures.
 */
struct acpi_hest_ia_error_bank {
	uint8_t bank_number;
	uint8_t clear_status_on_init;
	uint8_t status_format;
	uint8_t reserved;
	uint32_t control_register;
	uint64_t control_data;
	uint32_t status_register;
	uint32_t address_register;
	uint32_t misc_register;
};

/* Common HEST sub-structure for PCI/AER structures below (6,7,8) */

struct acpi_hest_aer_common {
	uint16_t reserved1;
	uint8_t flags;
	uint8_t enabled;
	uint32_t records_to_preallocate;
	uint32_t max_sections_per_record;
	uint32_t bus;
	uint16_t device;
	uint16_t function;
	uint16_t device_control;
	uint16_t reserved2;
	uint32_t uncorrectable_mask;
	uint32_t uncorrectable_severity;
	uint32_t correctable_mask;
	uint32_t advanced_capabilities;
};

/* Masks for HEST Flags fields */

#define ACPI_HEST_FIRMWARE_FIRST        (1)
#define ACPI_HEST_GLOBAL                (1<<1)

/* Hardware Error Notification */

struct acpi_hest_notify {
	uint8_t type;
	uint8_t length;
	uint16_t config_write_enable;
	uint32_t poll_interval;
	uint32_t vector;
	uint32_t polling_threshold_value;
	uint32_t polling_threshold_window;
	uint32_t error_threshold_value;
	uint32_t error_threshold_window;
};

/* Values for Notify Type field above */

enum acpi_hest_notify_types {
	ACPI_HEST_NOTIFY_POLLED = 0,
	ACPI_HEST_NOTIFY_EXTERNAL = 1,
	ACPI_HEST_NOTIFY_LOCAL = 2,
	ACPI_HEST_NOTIFY_SCI = 3,
	ACPI_HEST_NOTIFY_NMI = 4,
	ACPI_HEST_NOTIFY_RESERVED = 5	/* 5 and greater are reserved */
};

/* Values for config_write_enable bitfield above */

#define ACPI_HEST_TYPE                  (1)
#define ACPI_HEST_POLL_INTERVAL         (1<<1)
#define ACPI_HEST_POLL_THRESHOLD_VALUE  (1<<2)
#define ACPI_HEST_POLL_THRESHOLD_WINDOW (1<<3)
#define ACPI_HEST_ERR_THRESHOLD_VALUE   (1<<4)
#define ACPI_HEST_ERR_THRESHOLD_WINDOW  (1<<5)

/*
 * HEST subtables
 */

/* 0: IA32 Machine Check Exception */

struct acpi_hest_ia_machine_check {
	struct acpi_hest_header header;
	uint16_t reserved1;
	uint8_t flags;
	uint8_t enabled;
	uint32_t records_to_preallocate;
	uint32_t max_sections_per_record;
	uint64_t global_capability_data;
	uint64_t global_control_data;
	uint8_t num_hardware_banks;
	uint8_t reserved3[7];
};

/* 1: IA32 Corrected Machine Check */

struct acpi_hest_ia_corrected {
	struct acpi_hest_header header;
	uint16_t reserved1;
	uint8_t flags;
	uint8_t enabled;
	uint32_t records_to_preallocate;
	uint32_t max_sections_per_record;
	struct acpi_hest_notify notify;
	uint8_t num_hardware_banks;
	uint8_t reserved2[3];
};

/* 2: IA32 Non-Maskable Interrupt */

struct acpi_hest_ia_nmi {
	struct acpi_hest_header header;
	uint32_t reserved;
	uint32_t records_to_preallocate;
	uint32_t max_sections_per_record;
	uint32_t max_raw_data_length;
};

/* 3,4,5: Not used */

/* 6: PCI Express Root Port AER */

struct acpi_hest_aer_root {
	struct acpi_hest_header header;
	struct acpi_hest_aer_common aer;
	uint32_t root_error_command;
};

/* 7: PCI Express AER (AER Endpoint) */

struct acpi_hest_aer {
	struct acpi_hest_header header;
	struct acpi_hest_aer_common aer;
};

/* 8: PCI Express/PCI-X Bridge AER */

struct acpi_hest_aer_bridge {
	struct acpi_hest_header header;
	struct acpi_hest_aer_common aer;
	uint32_t uncorrectable_mask2;
	uint32_t uncorrectable_severity2;
	uint32_t advanced_capabilities2;
};

/* 9: Generic Hardware Error Source */

struct acpi_hest_generic {
	struct acpi_hest_header header;
	uint16_t related_source_id;
	uint8_t reserved;
	uint8_t enabled;
	uint32_t records_to_preallocate;
	uint32_t max_sections_per_record;
	uint32_t max_raw_data_length;
	struct acpi_generic_address error_status_address;
	struct acpi_hest_notify notify;
	uint32_t error_block_length;
};

/* Generic Error Status block */

struct acpi_hest_generic_status {
	uint32_t block_status;
	uint32_t raw_data_offset;
	uint32_t raw_data_length;
	uint32_t data_length;
	uint32_t error_severity;
};

/* Values for block_status flags above */

#define ACPI_HEST_UNCORRECTABLE             (1)
#define ACPI_HEST_CORRECTABLE               (1<<1)
#define ACPI_HEST_MULTIPLE_UNCORRECTABLE    (1<<2)
#define ACPI_HEST_MULTIPLE_CORRECTABLE      (1<<3)
#define ACPI_HEST_ERROR_ENTRY_COUNT         (0xFF<<4)	/* 8 bits, error count */

/* Generic Error Data entry */

struct acpi_hest_generic_data {
	uint8_t section_type[16];
	uint32_t error_severity;
	uint16_t revision;
	uint8_t validation_bits;
	uint8_t flags;
	uint32_t error_data_length;
	uint8_t fru_id[16];
	uint8_t fru_text[20];
};

/*******************************************************************************
 *
 * MADT - Multiple APIC Description Table
 *        Version 3
 *
 ******************************************************************************/

struct acpi_table_madt {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint32_t address;		/* Physical address of local APIC */
	uint32_t flags;
};

/* Masks for Flags field above */

#define ACPI_MADT_PCAT_COMPAT       (1)	/* 00: System also has dual 8259s */

/* Values for PCATCompat flag */

#define ACPI_MADT_DUAL_PIC          0
#define ACPI_MADT_MULTIPLE_APIC     1

/* Values for MADT subtable type in struct acpi_subtable_header */

enum acpi_madt_type {
	ACPI_MADT_TYPE_LOCAL_APIC = 0,
	ACPI_MADT_TYPE_IO_APIC = 1,
	ACPI_MADT_TYPE_INTERRUPT_OVERRIDE = 2,
	ACPI_MADT_TYPE_NMI_SOURCE = 3,
	ACPI_MADT_TYPE_LOCAL_APIC_NMI = 4,
	ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE = 5,
	ACPI_MADT_TYPE_IO_SAPIC = 6,
	ACPI_MADT_TYPE_LOCAL_SAPIC = 7,
	ACPI_MADT_TYPE_INTERRUPT_SOURCE = 8,
	ACPI_MADT_TYPE_LOCAL_X2APIC = 9,
	ACPI_MADT_TYPE_LOCAL_X2APIC_NMI = 10,
	ACPI_MADT_TYPE_RESERVED = 11	/* 11 and greater are reserved */
};

/*
 * MADT Sub-tables, correspond to Type in struct acpi_subtable_header
 */

/* 0: Processor Local APIC */

struct acpi_madt_local_apic {
	struct acpi_subtable_header header;
	uint8_t processor_id;	/* ACPI processor id */
	uint8_t id;			/* Processor's local APIC id */
	uint32_t lapic_flags;
};

/* 1: IO APIC */

struct acpi_madt_io_apic {
	struct acpi_subtable_header header;
	uint8_t id;			/* I/O APIC ID */
	uint8_t reserved;		/* Reserved - must be zero */
	uint32_t address;		/* APIC physical address */
	uint32_t global_irq_base;	/* Global system interrupt where INTI lines start */
};

/* 2: Interrupt Override */

struct acpi_madt_interrupt_override {
	struct acpi_subtable_header header;
	uint8_t bus;			/* 0 - ISA */
	uint8_t source_irq;		/* Interrupt source (IRQ) */
	uint32_t global_irq;		/* Global system interrupt */
	uint16_t inti_flags;
};

/* 3: NMI Source */

struct acpi_madt_nmi_source {
	struct acpi_subtable_header header;
	uint16_t inti_flags;
	uint32_t global_irq;		/* Global system interrupt */
};

/* 4: Local APIC NMI */

struct acpi_madt_local_apic_nmi {
	struct acpi_subtable_header header;
	uint8_t processor_id;	/* ACPI processor id */
	uint16_t inti_flags;
	uint8_t lint;		/* LINTn to which NMI is connected */
};

/* 5: Address Override */

struct acpi_madt_local_apic_override {
	struct acpi_subtable_header header;
	uint16_t reserved;		/* Reserved, must be zero */
	uint64_t address;		/* APIC physical address */
};

/* 6: I/O Sapic */

struct acpi_madt_io_sapic {
	struct acpi_subtable_header header;
	uint8_t id;			/* I/O SAPIC ID */
	uint8_t reserved;		/* Reserved, must be zero */
	uint32_t global_irq_base;	/* Global interrupt for SAPIC start */
	uint64_t address;		/* SAPIC physical address */
};

/* 7: Local Sapic */

struct acpi_madt_local_sapic {
	struct acpi_subtable_header header;
	uint8_t processor_id;	/* ACPI processor id */
	uint8_t id;			/* SAPIC ID */
	uint8_t eid;			/* SAPIC EID */
	uint8_t reserved[3];		/* Reserved, must be zero */
	uint32_t lapic_flags;
	uint32_t uid;		/* Numeric UID - ACPI 3.0 */
	char uid_string[1];	/* String UID  - ACPI 3.0 */
};

/* 8: Platform Interrupt Source */

struct acpi_madt_interrupt_source {
	struct acpi_subtable_header header;
	uint16_t inti_flags;
	uint8_t type;		/* 1=PMI, 2=INIT, 3=corrected */
	uint8_t id;			/* Processor ID */
	uint8_t eid;			/* Processor EID */
	uint8_t io_sapic_vector;	/* Vector value for PMI interrupts */
	uint32_t global_irq;		/* Global system interrupt */
	uint32_t flags;		/* Interrupt Source Flags */
};

/* Masks for Flags field above */

#define ACPI_MADT_CPEI_OVERRIDE     (1)

/* 9: Processor Local X2APIC (ACPI 4.0) */

struct acpi_madt_local_x2apic {
	struct acpi_subtable_header header;
	uint16_t reserved;		/* Reserved - must be zero */
	uint32_t local_apic_id;	/* Processor x2APIC ID  */
	uint32_t lapic_flags;
	uint32_t uid;		/* ACPI processor UID */
};

/* 10: Local X2APIC NMI (ACPI 4.0) */

struct acpi_madt_local_x2apic_nmi {
	struct acpi_subtable_header header;
	uint16_t inti_flags;
	uint32_t uid;		/* ACPI processor UID */
	uint8_t lint;		/* LINTn to which NMI is connected */
	uint8_t reserved[3];
};

/*
 * Common flags fields for MADT subtables
 */

/* MADT Local APIC flags (lapic_flags) */

#define ACPI_MADT_ENABLED           (1)	/* 00: Processor is usable if set */

/* MADT MPS INTI flags (inti_flags) */

#define ACPI_MADT_POLARITY_MASK     (3)	/* 00-01: Polarity of APIC I/O input signals */
#define ACPI_MADT_TRIGGER_MASK      (3<<2)	/* 02-03: Trigger mode of APIC input signals */

/* Values for MPS INTI flags */

#define ACPI_MADT_POLARITY_CONFORMS       0
#define ACPI_MADT_POLARITY_ACTIVE_HIGH    1
#define ACPI_MADT_POLARITY_RESERVED       2
#define ACPI_MADT_POLARITY_ACTIVE_LOW     3

#define ACPI_MADT_TRIGGER_CONFORMS        (0)
#define ACPI_MADT_TRIGGER_EDGE            (1<<2)
#define ACPI_MADT_TRIGGER_RESERVED        (2<<2)
#define ACPI_MADT_TRIGGER_LEVEL           (3<<2)

/*******************************************************************************
 *
 * MSCT - Maximum System Characteristics Table (ACPI 4.0)
 *        Version 1
 *
 ******************************************************************************/

struct acpi_table_msct {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint32_t proximity_offset;	/* Location of proximity info struct(s) */
	uint32_t max_proximity_domains;	/* Max number of proximity domains */
	uint32_t max_clock_domains;	/* Max number of clock domains */
	uint64_t max_address;	/* Max physical address in system */
};

/* Subtable - Maximum Proximity Domain Information. Version 1 */

struct acpi_msct_proximity {
	uint8_t revision;
	uint8_t length;
	uint32_t range_start;	/* Start of domain range */
	uint32_t range_end;		/* End of domain range */
	uint32_t processor_capacity;
	uint64_t memory_capacity;	/* In bytes */
};

/*******************************************************************************
 *
 * SBST - Smart Battery Specification Table
 *        Version 1
 *
 ******************************************************************************/

struct acpi_table_sbst {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint32_t warning_level;
	uint32_t low_level;
	uint32_t critical_level;
};

/*******************************************************************************
 *
 * SLIT - System Locality Distance Information Table
 *        Version 1
 *
 ******************************************************************************/

struct acpi_table_slit {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint64_t locality_count;
	uint8_t entry[1];		/* Real size = localities^2 */
};

/*******************************************************************************
 *
 * SRAT - System Resource Affinity Table
 *        Version 3
 *
 ******************************************************************************/

struct acpi_table_srat {
	struct acpi_table_header header;	/* Common ACPI table header */
	uint32_t table_revision;	/* Must be value '1' */
	uint64_t reserved;		/* Reserved, must be zero */
};

/* Values for subtable type in struct acpi_subtable_header */

enum acpi_srat_type {
	ACPI_SRAT_TYPE_CPU_AFFINITY = 0,
	ACPI_SRAT_TYPE_MEMORY_AFFINITY = 1,
	ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY = 2,
	ACPI_SRAT_TYPE_RESERVED = 3	/* 3 and greater are reserved */
};

/*
 * SRAT Sub-tables, correspond to Type in struct acpi_subtable_header
 */

/* 0: Processor Local APIC/SAPIC Affinity */

struct acpi_srat_cpu_affinity {
	struct acpi_subtable_header header;
	uint8_t proximity_domain_lo;
	uint8_t apic_id;
	uint32_t flags;
	uint8_t local_sapic_eid;
	uint8_t proximity_domain_hi[3];
	uint32_t reserved;		/* Reserved, must be zero */
};

/* Flags */

#define ACPI_SRAT_CPU_USE_AFFINITY  (1)	/* 00: Use affinity structure */

/* 1: Memory Affinity */

struct acpi_srat_mem_affinity {
	struct acpi_subtable_header header;
	uint32_t proximity_domain;
	uint16_t reserved;		/* Reserved, must be zero */
	uint64_t base_address;
	uint64_t length;
        uint32_t memory_type;	/* See acpi_address_range_id */
	uint32_t flags;
	uint64_t reserved2;		/* Reserved, must be zero */
};

/* Flags */

#define ACPI_SRAT_MEM_ENABLED       (1)	/* 00: Use affinity structure */
#define ACPI_SRAT_MEM_HOT_PLUGGABLE (1<<1)	/* 01: Memory region is hot pluggable */
#define ACPI_SRAT_MEM_NON_VOLATILE  (1<<2)	/* 02: Memory region is non-volatile */

/* 2: Processor Local X2_APIC Affinity (ACPI 4.0) */

struct acpi_srat_x2apic_cpu_affinity {
	struct acpi_subtable_header header;
	uint16_t reserved;		/* Reserved, must be zero */
	uint32_t proximity_domain;
	uint32_t apic_id;
	uint32_t flags;
	uint32_t clock_domain;
	uint32_t reserved2;
};

/* Flags for struct acpi_srat_cpu_affinity and struct acpi_srat_x2apic_cpu_affinity */

#define ACPI_SRAT_CPU_ENABLED       (1)	/* 00: Use affinity structure */

/* Reset to default packing */

#pragma pack()

#endif				/* __ACTBL1_H__ */
