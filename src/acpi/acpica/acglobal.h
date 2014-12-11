/******************************************************************************
 *
 * Name: acglobal.h - Declarations for global variables
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

#ifndef __ACGLOBAL_H__
#define __ACGLOBAL_H__

/*
 * Ensure that the globals are actually defined and initialized only once.
 *
 * The use of these macros allows a single list of globals (here) in order
 * to simplify maintenance of the code.
 *
 * utglobal.c defines DEFINE_ACPI_GLOBALS before including this file. Thus its
 * object file provides the linker with these variables, so other files can
 * reference them.
 */
#ifdef DEFINE_ACPI_GLOBALS
#define ACPI_EXTERN
#define ACPI_INIT_GLOBAL(a,b) a=b
#else
#define ACPI_EXTERN extern
#define ACPI_INIT_GLOBAL(a,b) a
#endif

#ifdef DEFINE_ACPI_GLOBALS

/* Public globals, available from outside ACPICA subsystem */

/*****************************************************************************
 *
 * Runtime configuration (static defaults that can be overriden at runtime)
 *
 ****************************************************************************/

/*
 * Enable "slack" in the AML interpreter?  Default is FALSE, and the
 * interpreter strictly follows the ACPI specification.  Setting to TRUE
 * allows the interpreter to ignore certain errors and/or bad AML constructs.
 *
 * Currently, these features are enabled by this flag:
 *
 * 1) Allow "implicit return" of last value in a control method
 * 2) Allow access beyond the end of an operation region
 * 3) Allow access to uninitialized locals/args (auto-init to integer 0)
 * 4) Allow ANY object type to be a source operand for the Store() operator
 * 5) Allow unresolved references (invalid target name) in package objects
 * 6) Enable warning messages for behavior that is not ACPI spec compliant
 */
u8 ACPI_INIT_GLOBAL(acpi_gbl_enable_interpreter_slack, FALSE);

/*
 * Automatically serialize ALL control methods? Default is FALSE, meaning
 * to use the Serialized/not_serialized method flags on a per method basis.
 * Only change this if the ASL code is poorly written and cannot handle
 * reentrancy even though methods are marked "NotSerialized".
 */
u8 ACPI_INIT_GLOBAL(acpi_gbl_all_methods_serialized, FALSE);

/*
 * Create the predefined _OSI method in the namespace? Default is TRUE
 * because ACPI CA is fully compatible with other ACPI implementations.
 * Changing this will revert ACPI CA (and machine ASL) to pre-OSI behavior.
 */
u8 ACPI_INIT_GLOBAL(acpi_gbl_create_osi_method, TRUE);

/*
 * Optionally use default values for the ACPI register widths. Set this to
 * TRUE to use the defaults, if an FADT contains incorrect widths/lengths.
 */
u8 ACPI_INIT_GLOBAL(acpi_gbl_use_default_register_widths, TRUE);

/*
 * Optionally enable output from the AML Debug Object.
 */
u32 ACPI_INIT_GLOBAL(acpi_gbl_enable_aml_debug_object, FALSE);

/*
 * Optionally copy the entire DSDT to local memory (instead of simply
 * mapping it.) There are some BIOSs that corrupt or replace the original
 * DSDT, creating the need for this option. Default is FALSE, do not copy
 * the DSDT.
 */
u8 ACPI_INIT_GLOBAL(acpi_gbl_copy_dsdt_locally, FALSE);

/*
 * Optionally truncate I/O addresses to 16 bits. Provides compatibility
 * with other ACPI implementations. NOTE: During ACPICA initialization,
 * this value is set to TRUE if any Windows OSI strings have been
 * requested by the BIOS.
 */
u8 ACPI_INIT_GLOBAL(acpi_gbl_truncate_io_addresses, FALSE);

/* acpi_gbl_FADT is a local copy of the FADT, converted to a common format. */

struct acpi_table_fadt acpi_gbl_FADT;
u32 acpi_current_gpe_count;
u32 acpi_gbl_trace_flags;
acpi_name acpi_gbl_trace_method_name;
u8 acpi_gbl_system_awake_and_running;

#endif		/* DEFINE_ACPI_GLOBALS */

/*****************************************************************************
 *
 * Debug support
 *
 ****************************************************************************/

/* Procedure nesting level for debug output */

extern u32 acpi_gbl_nesting_level;

ACPI_EXTERN u32 acpi_gpe_count;
ACPI_EXTERN u32 acpi_fixed_event_count[ACPI_NUM_FIXED_EVENTS];

/* Support for dynamic control method tracing mechanism */

ACPI_EXTERN u32 acpi_gbl_original_dbg_level;
ACPI_EXTERN u32 acpi_gbl_original_dbg_layer;
ACPI_EXTERN u32 acpi_gbl_trace_dbg_level;
ACPI_EXTERN u32 acpi_gbl_trace_dbg_layer;

/*****************************************************************************
 *
 * ACPI Table globals
 *
 ****************************************************************************/

/*
 * acpi_gbl_root_table_list is the master list of ACPI tables that were
 * found in the RSDT/XSDT.
 */
ACPI_EXTERN struct acpi_table_list acpi_gbl_root_table_list;

/*
 * Handle both ACPI 1.0 and ACPI 2.0 Integer widths. The integer width is
 * determined by the revision of the DSDT: If the DSDT revision is less than
 * 2, use only the lower 32 bits of the internal 64-bit Integer.
 */
ACPI_EXTERN u8 acpi_gbl_integer_bit_width;
ACPI_EXTERN u8 acpi_gbl_integer_byte_width;
ACPI_EXTERN u8 acpi_gbl_integer_nybble_width;


#endif				/* __ACGLOBAL_H__ */
