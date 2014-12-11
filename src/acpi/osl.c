/*
 *  osl.c - OS-dependent functions ($Revision: 83 $)
 *
 *  Copyright (C) 2000       Andrew Henroid
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *  Copyright (c) 2008 Intel Corporation
 *   Author: Matthew Wilcox <willy@linux.intel.com>
 *  Copyright (C) 2011, Alexander Merritt <merritt.alex@gmail.com>
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
 */

#include <nautilus/acpi.h>
#include <nautilus/list.h>
#include <nautilus/printk.h>
#include <nautilus/spinlock.h>

#include <acpi/acpi.h>

#define _COMPONENT		ACPI_OS_SERVICES
ACPI_MODULE_NAME("osl");
#define PREFIX		"ACPI: "
struct acpi_os_dpc {
	acpi_osd_exec_callback function;
	void *context;
	int wait;
};

#ifdef CONFIG_ACPI_CUSTOM_DSDT
#include CONFIG_ACPI_CUSTOM_DSDT_FILE
#endif

#define __init 
#define __iomem
#define __ref
#define __refdata
#define __init_refok
#define __initdata_refok
#define __exit_refok

void acpi_os_printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	acpi_os_vprintf(fmt, args);
	va_end(args);
}

void acpi_os_vprintf(const char *fmt, va_list args)
{
	static char buffer[512];

	vsprintf(buffer, fmt, args);

	printk("%s", buffer);
}

acpi_physical_address acpi_os_get_root_pointer(void)
{
	{
		acpi_physical_address pa = 0;

		acpi_find_root_pointer(&pa);
		return pa;
	}
}


void *
acpi_os_map_memory(acpi_physical_address phys, acpi_size size)
{
    if (phys < nk_get_nautilus_info()->sys.mem.phys_mem_avail) {
        return (void*)phys;
    } else {
        printk("mapping page for %p\n", (void*)phys);
        void * addr = (void*)(phys & ~((1ULL<<PAGE_SHIFT)-1));
        ASSERT(((void*)phys + size) < (addr+PAGE_SIZE));
        nk_map_page_nocache((addr_t)addr, PTE_WRITABLE_BIT|PTE_PRESENT_BIT);
        return (void*)phys;
    }
}

void __ref acpi_os_unmap_memory(void __iomem *virt, acpi_size size)
{
    /* KCH: we don't really need to do this since we're direct mapped */
    return;
}

void early_acpi_os_unmap_memory(void __iomem *virt, acpi_size size)
{
	return;
}

