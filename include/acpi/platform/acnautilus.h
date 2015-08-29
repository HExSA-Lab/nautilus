/******************************************************************************
 *
 * acnautilus.h - OS specific defines, etc. for Nautilus
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

#ifndef __ACNAUTILUS_H__
#define __ACNAUTILUS_H__

/* Common (in-kernel/user-space) ACPICA configuration */

#define ACPI_USE_SYSTEM_CLIBRARY
#define ACPI_USE_DO_WHILE_0
#define ACPI_MUTEX_TYPE             ACPI_BINARY_SEMAPHORE

#include <nautilus/nautilus.h>
#include <nautilus/naut_string.h>
#include <nautilus/naut_types.h>
#include <nautilus/spinlock.h>
#include <nautilus/thread.h>
#include <nautilus/atomic.h>
#include <lib/bitops.h>
#include <nautilus/mm.h>
//#include <acpi/acpi.h>
//#include <nautilus/acpi.h>
#include <nautilus/acpi-x86_64.h>

/* Host-dependent types and defines for in-kernel ACPICA */

#define ACPI_MACHINE_WIDTH          64
#define ACPI_EXPORT_SYMBOL(symbol)
#define strtoul                     simple_strtoul

#define acpi_spinlock                       spinlock_t *
#define acpi_cpu_flags                      unsigned long

/* Nautilus uses GCC */
#include "acgcc.h"

#include <acpi/actypes.h>

static inline acpi_thread_id acpi_os_get_thread_id(void)
{
	return (acpi_thread_id)(unsigned long)get_cur_thread();
}

static inline void *acpi_os_allocate(acpi_size size)
{
	return malloc(size);
}

static inline void *acpi_os_allocate_zeroed(acpi_size size)
{
	void * ptr = acpi_os_allocate(size);
    memset(ptr, 0, size);
    return ptr;
}

#define ACPI_ALLOCATE(a)        acpi_os_allocate(a)
#define ACPI_ALLOCATE_ZEROED(a) acpi_os_allocate_zeroed(a)
#define ACPI_FREE(a)            free(a)

#endif /* __ACNAUTILUS_H__ */
