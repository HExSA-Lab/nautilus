/******************************************************************************
 *
 * Name: acrestyp.h - Defines, types, and structures for resource descriptors
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

#ifndef __ACRESTYP_H__
#define __ACRESTYP_H__

/*
 * Definitions for Resource Attributes
 */
typedef uint16_t acpi_rs_length;	/* Resource Length field is fixed at 16 bits */
typedef uint32_t acpi_rsdesc_size;	/* Max Resource Descriptor size is (Length+3) = (64_k-1)+3 */

/*
 * Memory Attributes
 */
#define ACPI_READ_ONLY_MEMORY           (uint8_t) 0x00
#define ACPI_READ_WRITE_MEMORY          (uint8_t) 0x01

#define ACPI_NON_CACHEABLE_MEMORY       (uint8_t) 0x00
#define ACPI_CACHABLE_MEMORY            (uint8_t) 0x01
#define ACPI_WRITE_COMBINING_MEMORY     (uint8_t) 0x02
#define ACPI_PREFETCHABLE_MEMORY        (uint8_t) 0x03

/*
 * IO Attributes
 * The ISA IO ranges are:     n000-n0_fFh, n400-n4_fFh, n800-n8_fFh, n_c00-n_cFFh.
 * The non-ISA IO ranges are: n100-n3_fFh, n500-n7_fFh, n900-n_bFFh, n_cd0-n_fFFh.
 */
#define ACPI_NON_ISA_ONLY_RANGES        (uint8_t) 0x01
#define ACPI_ISA_ONLY_RANGES            (uint8_t) 0x02
#define ACPI_ENTIRE_RANGE               (ACPI_NON_ISA_ONLY_RANGES | ACPI_ISA_ONLY_RANGES)

/* Type of translation - 1=Sparse, 0=Dense */

#define ACPI_SPARSE_TRANSLATION         (uint8_t) 0x01

/*
 * IO Port Descriptor Decode
 */
#define ACPI_DECODE_10                  (uint8_t) 0x00	/* 10-bit IO address decode */
#define ACPI_DECODE_16                  (uint8_t) 0x01	/* 16-bit IO address decode */

/*
 * IRQ Attributes
 */
#define ACPI_LEVEL_SENSITIVE            (uint8_t) 0x00
#define ACPI_EDGE_SENSITIVE             (uint8_t) 0x01

#define ACPI_ACTIVE_HIGH                (uint8_t) 0x00
#define ACPI_ACTIVE_LOW                 (uint8_t) 0x01

#define ACPI_EXCLUSIVE                  (uint8_t) 0x00
#define ACPI_SHARED                     (uint8_t) 0x01

/*
 * DMA Attributes
 */
#define ACPI_COMPATIBILITY              (uint8_t) 0x00
#define ACPI_TYPE_A                     (uint8_t) 0x01
#define ACPI_TYPE_B                     (uint8_t) 0x02
#define ACPI_TYPE_F                     (uint8_t) 0x03

#define ACPI_NOT_BUS_MASTER             (uint8_t) 0x00
#define ACPI_BUS_MASTER                 (uint8_t) 0x01

#define ACPI_TRANSFER_8                 (uint8_t) 0x00
#define ACPI_TRANSFER_8_16              (uint8_t) 0x01
#define ACPI_TRANSFER_16                (uint8_t) 0x02

/*
 * Start Dependent Functions Priority definitions
 */
#define ACPI_GOOD_CONFIGURATION         (uint8_t) 0x00
#define ACPI_ACCEPTABLE_CONFIGURATION   (uint8_t) 0x01
#define ACPI_SUB_OPTIMAL_CONFIGURATION  (uint8_t) 0x02

/*
 * 16, 32 and 64-bit Address Descriptor resource types
 */
#define ACPI_MEMORY_RANGE               (uint8_t) 0x00
#define ACPI_IO_RANGE                   (uint8_t) 0x01
#define ACPI_BUS_NUMBER_RANGE           (uint8_t) 0x02

#define ACPI_ADDRESS_NOT_FIXED          (uint8_t) 0x00
#define ACPI_ADDRESS_FIXED              (uint8_t) 0x01

#define ACPI_POS_DECODE                 (uint8_t) 0x00
#define ACPI_SUB_DECODE                 (uint8_t) 0x01

#define ACPI_PRODUCER                   (uint8_t) 0x00
#define ACPI_CONSUMER                   (uint8_t) 0x01

/*
 * If possible, pack the following structures to byte alignment
 */
#ifndef ACPI_MISALIGNMENT_NOT_SUPPORTED
#pragma pack(1)
#endif

/* UUID data structures for use in vendor-defined resource descriptors */

struct acpi_uuid {
	uint8_t data[ACPI_UUID_LENGTH];
};

struct acpi_vendor_uuid {
	uint8_t subtype;
	uint8_t data[ACPI_UUID_LENGTH];
};

/*
 * Structures used to describe device resources
 */
struct acpi_resource_irq {
	uint8_t descriptor_length;
	uint8_t triggering;
	uint8_t polarity;
	uint8_t sharable;
	uint8_t interrupt_count;
	uint8_t interrupts[1];
};

struct acpi_resource_dma {
	uint8_t type;
	uint8_t bus_master;
	uint8_t transfer;
	uint8_t channel_count;
	uint8_t channels[1];
};

struct acpi_resource_start_dependent {
	uint8_t descriptor_length;
	uint8_t compatibility_priority;
	uint8_t performance_robustness;
};

/*
 * The END_DEPENDENT_FUNCTIONS_RESOURCE struct is not
 * needed because it has no fields
 */

struct acpi_resource_io {
	uint8_t io_decode;
	uint8_t alignment;
	uint8_t address_length;
	uint16_t minimum;
	uint16_t maximum;
};

struct acpi_resource_fixed_io {
	uint16_t address;
	uint8_t address_length;
};

struct acpi_resource_vendor {
	uint16_t byte_length;
	uint8_t byte_data[1];
};

/* Vendor resource with UUID info (introduced in ACPI 3.0) */

struct acpi_resource_vendor_typed {
	uint16_t byte_length;
	uint8_t uuid_subtype;
	uint8_t uuid[ACPI_UUID_LENGTH];
	uint8_t byte_data[1];
};

struct acpi_resource_end_tag {
	uint8_t checksum;
};

struct acpi_resource_memory24 {
	uint8_t write_protect;
	uint16_t minimum;
	uint16_t maximum;
	uint16_t alignment;
	uint16_t address_length;
};

struct acpi_resource_memory32 {
	uint8_t write_protect;
	uint32_t minimum;
	uint32_t maximum;
	uint32_t alignment;
	uint32_t address_length;
};

struct acpi_resource_fixed_memory32 {
	uint8_t write_protect;
	uint32_t address;
	uint32_t address_length;
};

struct acpi_memory_attribute {
	uint8_t write_protect;
	uint8_t caching;
	uint8_t range_type;
	uint8_t translation;
};

struct acpi_io_attribute {
	uint8_t range_type;
	uint8_t translation;
	uint8_t translation_type;
	uint8_t reserved1;
};

union acpi_resource_attribute {
	struct acpi_memory_attribute mem;
	struct acpi_io_attribute io;

	/* Used for the *word_space macros */

	uint8_t type_specific;
};

struct acpi_resource_source {
	uint8_t index;
	uint16_t string_length;
	char *string_ptr;
};

/* Fields common to all address descriptors, 16/32/64 bit */

#define ACPI_RESOURCE_ADDRESS_COMMON \
	uint8_t                                      resource_type; \
	uint8_t                                      producer_consumer; \
	uint8_t                                      decode; \
	uint8_t                                      min_address_fixed; \
	uint8_t                                      max_address_fixed; \
	union acpi_resource_attribute           info;

struct acpi_resource_address {
ACPI_RESOURCE_ADDRESS_COMMON};

struct acpi_resource_address16 {
	ACPI_RESOURCE_ADDRESS_COMMON uint16_t granularity;
	uint16_t minimum;
	uint16_t maximum;
	uint16_t translation_offset;
	uint16_t address_length;
	struct acpi_resource_source resource_source;
};

struct acpi_resource_address32 {
	ACPI_RESOURCE_ADDRESS_COMMON uint32_t granularity;
	uint32_t minimum;
	uint32_t maximum;
	uint32_t translation_offset;
	uint32_t address_length;
	struct acpi_resource_source resource_source;
};

struct acpi_resource_address64 {
	ACPI_RESOURCE_ADDRESS_COMMON uint64_t granularity;
	uint64_t minimum;
	uint64_t maximum;
	uint64_t translation_offset;
	uint64_t address_length;
	struct acpi_resource_source resource_source;
};

struct acpi_resource_extended_address64 {
	ACPI_RESOURCE_ADDRESS_COMMON uint8_t revision_iD;
	uint64_t granularity;
	uint64_t minimum;
	uint64_t maximum;
	uint64_t translation_offset;
	uint64_t address_length;
	uint64_t type_specific;
};

struct acpi_resource_extended_irq {
	uint8_t producer_consumer;
	uint8_t triggering;
	uint8_t polarity;
	uint8_t sharable;
	uint8_t interrupt_count;
	struct acpi_resource_source resource_source;
	uint32_t interrupts[1];
};

struct acpi_resource_generic_register {
	uint8_t space_id;
	uint8_t bit_width;
	uint8_t bit_offset;
	uint8_t access_size;
	uint64_t address;
};

/* ACPI_RESOURCE_TYPEs */

#define ACPI_RESOURCE_TYPE_IRQ                  0
#define ACPI_RESOURCE_TYPE_DMA                  1
#define ACPI_RESOURCE_TYPE_START_DEPENDENT      2
#define ACPI_RESOURCE_TYPE_END_DEPENDENT        3
#define ACPI_RESOURCE_TYPE_IO                   4
#define ACPI_RESOURCE_TYPE_FIXED_IO             5
#define ACPI_RESOURCE_TYPE_VENDOR               6
#define ACPI_RESOURCE_TYPE_END_TAG              7
#define ACPI_RESOURCE_TYPE_MEMORY24             8
#define ACPI_RESOURCE_TYPE_MEMORY32             9
#define ACPI_RESOURCE_TYPE_FIXED_MEMORY32       10
#define ACPI_RESOURCE_TYPE_ADDRESS16            11
#define ACPI_RESOURCE_TYPE_ADDRESS32            12
#define ACPI_RESOURCE_TYPE_ADDRESS64            13
#define ACPI_RESOURCE_TYPE_EXTENDED_ADDRESS64   14	/* ACPI 3.0 */
#define ACPI_RESOURCE_TYPE_EXTENDED_IRQ         15
#define ACPI_RESOURCE_TYPE_GENERIC_REGISTER     16
#define ACPI_RESOURCE_TYPE_MAX                  16

/* Master union for resource descriptors */

union acpi_resource_data {
	struct acpi_resource_irq irq;
	struct acpi_resource_dma dma;
	struct acpi_resource_start_dependent start_dpf;
	struct acpi_resource_io io;
	struct acpi_resource_fixed_io fixed_io;
	struct acpi_resource_vendor vendor;
	struct acpi_resource_vendor_typed vendor_typed;
	struct acpi_resource_end_tag end_tag;
	struct acpi_resource_memory24 memory24;
	struct acpi_resource_memory32 memory32;
	struct acpi_resource_fixed_memory32 fixed_memory32;
	struct acpi_resource_address16 address16;
	struct acpi_resource_address32 address32;
	struct acpi_resource_address64 address64;
	struct acpi_resource_extended_address64 ext_address64;
	struct acpi_resource_extended_irq extended_irq;
	struct acpi_resource_generic_register generic_reg;

	/* Common fields */

	struct acpi_resource_address address;	/* Common 16/32/64 address fields */
};

/* Common resource header */

struct acpi_resource {
	uint32_t type;
	uint32_t length;
	union acpi_resource_data data;
};

/* restore default alignment */

#pragma pack()

#define ACPI_RS_SIZE_NO_DATA                8	/* Id + Length fields */
#define ACPI_RS_SIZE_MIN                    (u32) ACPI_ROUND_UP_TO_NATIVE_WORD (12)
#define ACPI_RS_SIZE(type)                  (u32) (ACPI_RS_SIZE_NO_DATA + sizeof (type))

#define ACPI_NEXT_RESOURCE(res)             (struct acpi_resource *)((uint8_t *) res + res->length)

struct acpi_pci_routing_table {
	uint32_t length;
	uint32_t pin;
	uint64_t address;		/* here for 64-bit alignment */
	uint32_t source_index;
	char source[4];		/* pad to 64 bits so sizeof() works in all cases */
};

#endif				/* __ACRESTYP_H__ */
