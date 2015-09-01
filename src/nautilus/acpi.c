/*
 *  acpi.c - Architecture-Specific Low-Level ACPI Support
 *
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *  Copyright (C) 2001 Jun Nakajima <jun.nakajima@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Modificaitons by Kyle Hale 2015, Northwestern University <kh@u.northwestern.edu>
 */


#include <nautilus/nautilus.h>
#include <nautilus/printk.h>
#include <nautilus/naut_types.h>
#include <nautilus/acpi.h>
#include <nautilus/numa.h>
#include <nautilus/smp.h>

uint32_t acpi_rsdt_forced;
int acpi_disabled;

#define BAD_MADT_ENTRY(entry, end) (					    \
		(!entry) || (unsigned long)entry + sizeof(*entry) > end ||  \
		((struct acpi_subtable_header *)entry)->length < sizeof(*entry))

#define PREFIX			"ACPI BOOT: "

int acpi_noirq;				/* skip ACPI IRQ initialization */
int acpi_pci_disabled;		/* skip ACPI PCI scan and IRQ initialization */

int acpi_lapic;
int acpi_ioapic;
int acpi_strict;

uint8_t acpi_sci_flags ;
int acpi_sci_override_gsi ;
int acpi_skip_timer_override ;
int acpi_use_timer_override ;
int acpi_fix_pin2_polarity ;

#ifdef CONFIG_X86_LOCAL_APIC
static uint64_t acpi_lapic_addr = APIC_DEFAULT_PHYS_BASE;
#endif


/* --------------------------------------------------------------------------
                              Boot-time Configuration
   -------------------------------------------------------------------------- */

/*
 * acpi_init() and acpi_boot_init()
 *  called from setup_arch(), always.
 *	1. checksums all tables
 *	2. enumerates lapics
 *	3. enumerates io-apics
 *
 * acpi_table_init() is separate to allow reading SRAT without
 * other side effects.
 *
 * side effects of acpi_boot_init:
 *	acpi_lapic = 1 if LAPIC found
 *	acpi_ioapic = 1 if IOAPIC found
 *	if (acpi_lapic && acpi_ioapic) smp_found_config = 1;
 *	if acpi_blacklisted() acpi_disabled = 1;
 *	acpi_irq_model=...
 *	...
 */

void 
nk_acpi_init (void)
{
	if (acpi_disabled) {
		ERROR_PRINT(PREFIX "ACPI is disabled, cannot initialize.\n");
		return; 
	}

	/* Initialize the ACPI boot-time table parser. */
	if (acpi_table_init()) {
		ERROR_PRINT(PREFIX "ACPI initialization failed, disabling ACPI.\n");
		disable_acpi();
		return;
	}
}
