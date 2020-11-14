/******************************************************************************
 *
 * Module Name: tbutils   - table utilities
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

#include <nautilus/intrinsics.h>
#include <acpi/acpi.h>
#include <acpi/actypes.h>
#include "accommon.h"
#include "actables.h"

#define _COMPONENT          ACPI_TABLES
ACPI_MODULE_NAME("tbutils")


static acpi_physical_address
acpi_tb_get_root_table_entry(u8 *table_entry, u32 table_entry_size);

/*******************************************************************************
 *
 * FUNCTION:    acpi_tb_check_xsdt
 *
 * PARAMETERS:  address                    - Pointer to the XSDT
 *
 * RETURN:      status
 *		AE_OK - XSDT is okay
 *		AE_NO_MEMORY - can't map XSDT
 *		AE_INVALID_TABLE_LENGTH - invalid table length
 *		AE_NULL_ENTRY - XSDT has NULL entry
 *
 * DESCRIPTION: validate XSDT
******************************************************************************/

static acpi_status
acpi_tb_check_xsdt(acpi_physical_address address)
{
	struct acpi_table_header *table;
	u32 length;
	u64 xsdt_entry_address;
	u8 *table_entry;
	u32 table_count;
	int i;

	/* map table header, verify length */
	table = acpi_os_map_memory(address, sizeof(struct acpi_table_header));
	if (!table)
		return AE_NO_MEMORY;

	length = table->length; /* length of entire table, incl header */
	acpi_os_unmap_memory(table, sizeof(struct acpi_table_header));
	if (length < sizeof(struct acpi_table_header))
		return AE_INVALID_TABLE_LENGTH;

	/* map entire table, not just header */
	table = acpi_os_map_memory(address, length);
	if (!table)
		return AE_NO_MEMORY;

	/* Calculate the number of tables described in XSDT */
	table_count =
		(u32) ((table->length -
		sizeof(struct acpi_table_header)) / sizeof(u64));
	table_entry =
		ACPI_CAST_PTR(u8, table) + sizeof(struct acpi_table_header);
	for (i = 0; i < table_count; i++) {
		ACPI_MOVE_64_TO_64(&xsdt_entry_address, table_entry);
		if (!xsdt_entry_address) {
			/* XSDT has NULL entry */
			break;
		}
		table_entry += sizeof(u64);
	}
	acpi_os_unmap_memory(table, length);

	if (i < table_count)
		return AE_NULL_ENTRY;
	else
		return AE_OK;
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_tb_fix_string
 *
 * PARAMETERS:  String              - String to be repaired
 *              Length              - Maximum length
 *
 * RETURN:      None
 *
 * DESCRIPTION: Replace every non-printable or non-ascii byte in the string
 *              with a question mark '?'.
 *
 ******************************************************************************/

static void acpi_tb_fix_string(char *string, acpi_size length)
{

	while (length && *string) {
		if (!ACPI_IS_PRINT(*string)) {
			*string = '?';
		}
		string++;
		length--;
	}
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_tb_cleanup_table_header
 *
 * PARAMETERS:  out_header          - Where the cleaned header is returned
 *              Header              - Input ACPI table header
 *
 * RETURN:      Returns the cleaned header in out_header
 *
 * DESCRIPTION: Copy the table header and ensure that all "string" fields in
 *              the header consist of printable characters.
 *
 ******************************************************************************/

static void
acpi_tb_cleanup_table_header(struct acpi_table_header *out_header,
			     struct acpi_table_header *header)
{

	ACPI_MEMCPY(out_header, header, sizeof(struct acpi_table_header));

	acpi_tb_fix_string(out_header->signature, ACPI_NAME_SIZE);
	acpi_tb_fix_string(out_header->oem_id, ACPI_OEM_ID_SIZE);
	acpi_tb_fix_string(out_header->oem_table_id, ACPI_OEM_TABLE_ID_SIZE);
	acpi_tb_fix_string(out_header->asl_compiler_id, ACPI_NAME_SIZE);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_tb_print_table_header
 *
 * PARAMETERS:  Address             - Table physical address
 *              Header              - Table header
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print an ACPI table header. Special cases for FACS and RSDP.
 *
 ******************************************************************************/

void
acpi_tb_print_table_header(acpi_physical_address address,
			   struct acpi_table_header *header)
{
	struct acpi_table_header local_header;

	/*
	 * The reason that the Address is cast to a void pointer is so that we
	 * can use %p which will work properly on both 32-bit and 64-bit hosts.
	 */
	if (ACPI_COMPARE_NAME(header->signature, ACPI_SIG_FACS)) {

		/* FACS only has signature and length fields */

		ACPI_INFO((AE_INFO, "%4.4s %p %05X",
			   header->signature, ACPI_CAST_PTR(void, address),
			   header->length));
	} else if (ACPI_COMPARE_NAME(header->signature, ACPI_SIG_RSDP)) {

		/* RSDP has no common fields */

		ACPI_MEMCPY(local_header.oem_id,
			    ACPI_CAST_PTR(struct acpi_table_rsdp,
					  header)->oem_id, ACPI_OEM_ID_SIZE);
		acpi_tb_fix_string(local_header.oem_id, ACPI_OEM_ID_SIZE);

		ACPI_INFO((AE_INFO, "RSDP %p %05X (v%.2d %6.6s)",
			   ACPI_CAST_PTR (void, address),
			   (ACPI_CAST_PTR(struct acpi_table_rsdp, header)->
			    revision >
			    0) ? ACPI_CAST_PTR(struct acpi_table_rsdp,
					       header)->length : 20,
			   ACPI_CAST_PTR(struct acpi_table_rsdp,
					 header)->revision,
			   local_header.oem_id));
	} else {
		/* Standard ACPI table with full common header */

		acpi_tb_cleanup_table_header(&local_header, header);

		ACPI_INFO((AE_INFO,
			   "%4.4s %p %05X (v%.2d %6.6s %8.8s %08X %4.4s %08X)",
			   local_header.signature, ACPI_CAST_PTR(void, address),
			   local_header.length, local_header.revision,
			   local_header.oem_id, local_header.oem_table_id,
			   local_header.oem_revision,
			   local_header.asl_compiler_id,
			   local_header.asl_compiler_revision));

	}
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_tb_validate_checksum
 *
 * PARAMETERS:  Table               - ACPI table to verify
 *              Length              - Length of entire table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Verifies that the table checksums to zero. Optionally returns
 *              exception on bad checksum.
 *
 ******************************************************************************/

acpi_status acpi_tb_verify_checksum(struct acpi_table_header *table, u32 length)
{
	u8 checksum;

	/* Compute the checksum on the table */

	checksum = acpi_tb_checksum(ACPI_CAST_PTR(u8, table), length);

	/* Checksum ok? (should be zero) */

	if (checksum) {
		ACPI_WARNING((AE_INFO,
			      "Incorrect checksum in table [%4.4s] - 0x%2.2X, should be 0x%2.2X",
			      table->signature, table->checksum,
			      (u8) (table->checksum - checksum)));

#if (ACPI_CHECKSUM_ABORT)

		return (AE_BAD_CHECKSUM);
#endif
	}

	return (AE_OK);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_tb_checksum
 *
 * PARAMETERS:  Buffer          - Pointer to memory region to be checked
 *              Length          - Length of this memory region
 *
 * RETURN:      Checksum (u8)
 *
 * DESCRIPTION: Calculates circular checksum of memory region.
 *
 ******************************************************************************/

u8 acpi_tb_checksum(u8 *buffer, u32 length)
{
	u8 sum = 0;
	u8 *end = buffer + length;

	while (buffer < end) {
		sum = (u8) (sum + *(buffer++));
	}

	return sum;
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_tb_install_table
 *
 * PARAMETERS:  Address                 - Physical address of DSDT or FACS
 *              Signature               - Table signature, NULL if no need to
 *                                        match
 *              table_index             - Index into root table array
 *
 * RETURN:      None
 *
 * DESCRIPTION: Install an ACPI table into the global data structure. The
 *              table override mechanism is implemented here to allow the host
 *              OS to replace any table before it is installed in the root
 *              table array.
 *
 ******************************************************************************/

void
acpi_tb_install_table(acpi_physical_address address,
		      char *signature, u32 table_index)
{
	/* For Kitten's purposes, this function doesn't really do anything. The
	 * only side effect is calling acpi_ut_set_integer_width() if the table
	 * is DSDT (to initializes specific global variables).
	 */

	u8 flags;
	struct acpi_table_header *table_to_install = NULL;
	struct acpi_table_header *mapped_table = NULL;

    //printk("installing table using address %p\n", (void*)address);

	if (!address) {
		ACPI_ERROR((AE_INFO,
			    "Null physical address for ACPI table [%s]",
			    signature));
		return;
	}

	/* Map just the table header */

	mapped_table =
	    acpi_os_map_memory(address, sizeof(struct acpi_table_header));
	if (!mapped_table) {
		return;
	}

	/* If a particular signature is expected (DSDT/FACS), it must match */

	if (signature && !ACPI_COMPARE_NAME(mapped_table->signature, signature)) {
		ACPI_ERROR((AE_INFO,
			    "Invalid signature 0x%X for ACPI table, expected [%s]",
			    *ACPI_CAST_PTR(u32, mapped_table->signature),
			    signature));
		goto unmap_and_exit;
	}

	{
		table_to_install = mapped_table;
		flags = ACPI_TABLE_ORIGIN_MAPPED;
	}

	/* Initialize the table entry */

    //printk("installing table at index %u (root_table_list=%p, tables=%p)\n", table_index, acpi_gbl_root_table_list,
            //acpi_gbl_root_table_list.tables);
    //struct acpi_table_desc * t = &acpi_gbl_root_table_list.tables[table_index];
    //printk("printing table desc: %p\n", t);
    //printk("addr=%p\n", t->address);
    //printk("pointer=%p\n", t->pointer);
    //printk("length=0x%x\n", t->length);
	acpi_gbl_root_table_list.tables[table_index].address = address;
    //printk("table to install check (tbl=%p)\n", (void*)table_to_install);
	acpi_gbl_root_table_list.tables[table_index].length =
	    table_to_install->length;
    //printk("now gonna do flags\n");
	acpi_gbl_root_table_list.tables[table_index].flags = flags;

    //printk("installed flags etc\n");

	ACPI_MOVE_32_TO_32(&
			   (acpi_gbl_root_table_list.tables[table_index].
			    signature), table_to_install->signature);

    //printk("printking table header\n");

	acpi_tb_print_table_header(address, table_to_install);

	if (table_index == ACPI_TABLE_INDEX_DSDT) {

		/* Global integer width is based upon revision of the DSDT */

		acpi_ut_set_integer_width(table_to_install->revision);
	}

      unmap_and_exit:
	acpi_os_unmap_memory(mapped_table, sizeof(struct acpi_table_header));
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_tb_get_root_table_entry
 *
 * PARAMETERS:  table_entry         - Pointer to the RSDT/XSDT table entry
 *              table_entry_size    - sizeof 32 or 64 (RSDT or XSDT)
 *
 * RETURN:      Physical address extracted from the root table
 *
 * DESCRIPTION: Get one root table entry. Handles 32-bit and 64-bit cases on
 *              both 32-bit and 64-bit platforms
 *
 * NOTE:        acpi_physical_address is 32-bit on 32-bit platforms, 64-bit on
 *              64-bit platforms.
 *
 ******************************************************************************/

static acpi_physical_address
acpi_tb_get_root_table_entry(u8 *table_entry, u32 table_entry_size)
{
	u64 address64;

	/*
	 * Get the table physical address (32-bit for RSDT, 64-bit for XSDT):
	 * Note: Addresses are 32-bit aligned (not 64) in both RSDT and XSDT
	 */
	if (table_entry_size == sizeof(u32)) {
		/*
		 * 32-bit platform, RSDT: Return 32-bit table entry
		 * 64-bit platform, RSDT: Expand 32-bit to 64-bit and return
		 */
		return ((acpi_physical_address)
			(*ACPI_CAST_PTR(u32, table_entry)));
	} else {
		/*
		 * 32-bit platform, XSDT: Truncate 64-bit to 32-bit and return
		 * 64-bit platform, XSDT: Move (unaligned) 64-bit to local,
		 *  return 64-bit
		 */
		ACPI_MOVE_64_TO_64(&address64, table_entry);

#if ACPI_MACHINE_WIDTH == 32
		if (address64 > ACPI_UINT32_MAX) {

			/* Will truncate 64-bit address to 32 bits, issue warning */

			ACPI_WARNING((AE_INFO,
				      "64-bit Physical Address in XSDT is too large (0x%8.8X%8.8X),"
				      " truncating",
				      ACPI_FORMAT_UINT64(address64)));
		}
#endif
		return ((acpi_physical_address) (address64));
	}
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_tb_parse_root_table
 *
 * PARAMETERS:  Rsdp                    - Pointer to the RSDP
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to parse the Root System Description
 *              Table (RSDT or XSDT)
 *
 * NOTE:        Tables are mapped (not copied) for efficiency. The FACS must
 *              be mapped and cannot be copied because it contains the actual
 *              memory location of the ACPI Global Lock.
 *
 ******************************************************************************/

acpi_status __init
acpi_tb_parse_root_table(acpi_physical_address rsdp_address)
{
	struct acpi_table_rsdp *rsdp;
	u32 table_entry_size;
	u32 i;
	u32 table_count;
	struct acpi_table_header *table;
	acpi_physical_address address;
	acpi_physical_address uninitialized_var(rsdt_address);
	u32 length;
	u8 *table_entry;
	acpi_status status;

	ACPI_FUNCTION_TRACE(tb_parse_root_table);

	DEBUG_PRINT("ACPI: Parsing ACPI root table (at %p)\n", (void*)rsdp_address);

	/*
	 * Map the entire RSDP and extract the address of the RSDT or XSDT
	 */
	rsdp = acpi_os_map_memory(rsdp_address, sizeof(struct acpi_table_rsdp));
	if (!rsdp) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}


    //printk("rsdt_addr=0x%x\n", (uint32_t)rsdp->rsdt_physical_address);
    //printk("length=0x%x\n", rsdp->length);
    //printk("xsdtaddress=%p\n", (void*)rsdp->xsdt_physical_address);
	acpi_tb_print_table_header(rsdp_address,
				   ACPI_CAST_PTR(struct acpi_table_header,
						 rsdp));

	/* Differentiate between RSDT and XSDT root tables */

	if (rsdp->revision > 1 && rsdp->xsdt_physical_address
			&& !acpi_rsdt_forced) {
		/*
		 * Root table is an XSDT (64-bit physical addresses). We must use the
		 * XSDT if the revision is > 1 and the XSDT pointer is present, as per
		 * the ACPI specification.
		 */
		address = (acpi_physical_address) rsdp->xsdt_physical_address;
		table_entry_size = sizeof(u64);
		rsdt_address = (acpi_physical_address)
					rsdp->rsdt_physical_address;
	} else {
		/* Root table is an RSDT (32-bit physical addresses) */

		address = (acpi_physical_address) rsdp->rsdt_physical_address;
		table_entry_size = sizeof(u32);
	}


	/*
	 * It is not possible to map more than one entry in some environments,
	 * so unmap the RSDP here before mapping other tables
	 */
	acpi_os_unmap_memory(rsdp, sizeof(struct acpi_table_rsdp));

	if (table_entry_size == sizeof(u64)) {
		if (acpi_tb_check_xsdt(address) == AE_NULL_ENTRY) {
			/* XSDT has NULL entry, RSDT is used */
			address = rsdt_address;
			table_entry_size = sizeof(u32);
			ACPI_WARNING((AE_INFO, "BIOS XSDT has NULL entry, "
					"using RSDT"));
		}
	}
	/* Map the RSDT/XSDT table header to get the full table length */

	table = acpi_os_map_memory(address, sizeof(struct acpi_table_header));
	if (!table) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	acpi_tb_print_table_header(address, table);

	/* Get the length of the full table, verify length and map entire table */

	length = table->length;
	acpi_os_unmap_memory(table, sizeof(struct acpi_table_header));

	if (length < sizeof(struct acpi_table_header)) {
		ACPI_ERROR((AE_INFO, "Invalid length 0x%X in RSDT/XSDT",
			    length));
		return_ACPI_STATUS(AE_INVALID_TABLE_LENGTH);
	}

	table = acpi_os_map_memory(address, length);
	if (!table) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	/* Validate the root table checksum */

	status = acpi_tb_verify_checksum(table, length);
	if (ACPI_FAILURE(status)) {
		acpi_os_unmap_memory(table, length);
		return_ACPI_STATUS(status);
	}
	DEBUG_PRINT("ACPI: root table checksum pass\n");

	/* Calculate the number of tables described in the root table */

	table_count = (u32)((table->length - sizeof(struct acpi_table_header)) /
			    table_entry_size);
	/*
	 * First two entries in the table array are reserved for the DSDT
	 * and FACS, which are not actually present in the RSDT/XSDT - they
	 * come from the FADT
	 */
	table_entry =
	    ACPI_CAST_PTR(u8, table) + sizeof(struct acpi_table_header);
	acpi_gbl_root_table_list.current_table_count = 2;

	/*
	 * Initialize the root table array from the RSDT/XSDT
	 */
	for (i = 0; i < table_count; i++) {
		if (acpi_gbl_root_table_list.current_table_count >=
		    acpi_gbl_root_table_list.max_table_count) {

			/* There is no more room in the root table array, attempt resize */

			status = acpi_tb_resize_root_table_list();
			if (ACPI_FAILURE(status)) {
                printk("Truncating entries!\n");
				ACPI_WARNING((AE_INFO,
					      "Truncating %u table entries!",
					      (unsigned) (table_count -
					       (acpi_gbl_root_table_list.
							  current_table_count -
							  2))));
				break;
			}
		}

		/* Get the table physical address (32-bit for RSDT, 64-bit for XSDT) */

		acpi_gbl_root_table_list.tables[acpi_gbl_root_table_list.
						current_table_count].address =
		    acpi_tb_get_root_table_entry(table_entry, table_entry_size);

		table_entry += table_entry_size;
		acpi_gbl_root_table_list.current_table_count++;
	}

	/*
	 * It is not possible to map more than one entry in some environments,
	 * so unmap the root table here before mapping other tables
	 */
	acpi_os_unmap_memory(table, length);

	/*
	 * Complete the initialization of the root table array by examining
	 * the header of each table
	 */
	for (i = 2; i < acpi_gbl_root_table_list.current_table_count; i++) {
        //printk("installing table %u\n", i);
		acpi_tb_install_table(acpi_gbl_root_table_list.tables[i].
				      address, NULL, i);

	}

	DEBUG_PRINT("ACPI: Done parsing ACPI root table\n");

	return_ACPI_STATUS(AE_OK);
}
