/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/numa.h>
#include <nautilus/mm.h>
#include <nautilus/macros.h>
#include <nautilus/errno.h>
#include <nautilus/acpi.h>

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

static int srat_rev;


#ifndef NAUT_CONFIG_DEBUG_NUMA
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define NUMA_PRINT(fmt, args...) printk("NUMA: " fmt, ##args)
#define NUMA_DEBUG(fmt, args...) DEBUG_PRINT("NUMA: " fmt, ##args)
#define NUMA_ERROR(fmt, args...) ERROR_PRINT("NUMA: " fmt, ##args)

static void 
acpi_table_print_srat_entry (struct acpi_subtable_header * header)
{

    ACPI_FUNCTION_NAME("acpi_table_print_srat_entry");

    if (!header)
        return;

    switch (header->type) {

    case ACPI_SRAT_TYPE_CPU_AFFINITY:
        {
            struct acpi_srat_cpu_affinity *p =
                container_of(header, struct acpi_srat_cpu_affinity, header);
            u32 proximity_domain = p->proximity_domain_lo;

            if (srat_rev >= 2) {
                proximity_domain |= p->proximity_domain_hi[0] << 8;
                proximity_domain |= p->proximity_domain_hi[1] << 16;
                proximity_domain |= p->proximity_domain_hi[2] << 24;
            }
            NUMA_DEBUG("SRAT Processor (id[0x%02x] eid[0x%02x]) in proximity domain %d %s\n",
                      p->apic_id, p->local_sapic_eid,
                      proximity_domain,
                      p->flags & ACPI_SRAT_CPU_ENABLED
                      ? "enabled" : "disabled");
        }
        break;

    case ACPI_SRAT_TYPE_MEMORY_AFFINITY:
        {
            struct acpi_srat_mem_affinity *p =
                container_of(header, struct acpi_srat_mem_affinity, header);
            u32 proximity_domain = p->proximity_domain;

            if (srat_rev < 2)
                proximity_domain &= 0xff;
            NUMA_DEBUG("SRAT Memory (0x%016llx length 0x%016llx type 0x%x) in proximity domain %d %s%s\n",
                      p->base_address, p->length,
                      p->memory_type, proximity_domain,
                      p->flags & ACPI_SRAT_MEM_ENABLED
                      ? "enabled" : "disabled",
                      p->flags & ACPI_SRAT_MEM_HOT_PLUGGABLE
                      ? " hot-pluggable" : "");
        }
        break;

    case ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY:
        {
            struct acpi_srat_x2apic_cpu_affinity *p =
                (struct acpi_srat_x2apic_cpu_affinity *)header;
            NUMA_DEBUG("SRAT Processor (x2apicid[0x%08x]) in"
                      " proximity domain %d %s\n",
                      p->apic_id,
                      p->proximity_domain,
                      (p->flags & ACPI_SRAT_CPU_ENABLED) ?
                      "enabled" : "disabled");
        }
        break;
    default:
        NUMA_DEBUG("Found unsupported SRAT entry (type = 0x%x)\n",
               header->type);
        break;
    }
}

static int 
acpi_parse_x2apic_affinity(struct acpi_subtable_header *header,
               const unsigned long end)
{
    struct acpi_srat_x2apic_cpu_affinity *processor_affinity;

    processor_affinity = (struct acpi_srat_x2apic_cpu_affinity *)header;

    if (!processor_affinity) {
        return -EINVAL;
    }

    struct sys_info * sys = &(nk_get_nautilus_info()->sys);
    unsigned i;
    uint32_t domain_id;

    if (!(processor_affinity->flags & ACPI_SRAT_CPU_ENABLED)) { 
	NUMA_DEBUG("Processor affinity for disabled x2apic processor...\n");
	return 0;
    }

    acpi_table_print_srat_entry(header);

    for (i = 0; i < sys->num_cpus; i++) {
        if (sys->cpus[i]->lapic_id == processor_affinity->apic_id) {
            domain_id = processor_affinity->proximity_domain;
            sys->cpus[i]->domain = sys->locality_info.domains[domain_id];
            break;
        }
    }

    if (i==sys->num_cpus) {
	NUMA_ERROR("Affinity for x2apic processor %x not found\n",processor_affinity->apic_id);
    }

    return 0;
}


static int 
acpi_table_parse_srat(enum acpi_srat_entry_id id,
              acpi_madt_entry_handler handler, unsigned int max_entries)
{
    return acpi_table_parse_entries(ACPI_SIG_SRAT,
                    sizeof(struct acpi_table_srat), id,
                    handler, max_entries);
}


static int 
acpi_parse_processor_affinity(struct acpi_subtable_header * header,
                  const unsigned long end)
{
    struct acpi_srat_cpu_affinity *processor_affinity
        = container_of(header, struct acpi_srat_cpu_affinity, header);

    struct sys_info * sys = &(nk_get_nautilus_info()->sys);
    unsigned i;
    uint32_t domain_id;

    if (!processor_affinity)
        return -EINVAL;

    if (!(processor_affinity->flags & ACPI_SRAT_CPU_ENABLED)) { 
		NUMA_DEBUG("Processor affinity for disabled processor...\n");
		return 0;
    }

    acpi_table_print_srat_entry(header);


    for (i = 0; i < sys->num_cpus; i++) {
        if (sys->cpus[i]->lapic_id == processor_affinity->apic_id) {
            domain_id = processor_affinity->proximity_domain_lo | 
                (((*(uint32_t*)(&(processor_affinity->proximity_domain_hi))) & 0xffffff) << 8);

            sys->cpus[i]->domain = sys->locality_info.domains[domain_id];
            break;
        }
    }

    if (i==sys->num_cpus) {
		NUMA_ERROR("Affinity for processor %d not found\n",processor_affinity->apic_id);
    }

    return 0;
}


/*
 * This is where we construct all of our NUMA domain 
 * information 
 */
static int 
acpi_parse_memory_affinity(struct acpi_subtable_header * header,
               const unsigned long end)
{
    struct acpi_srat_mem_affinity *memory_affinity
        = container_of(header, struct acpi_srat_mem_affinity, header);

    struct sys_info * sys       = &(nk_get_nautilus_info()->sys);
    struct mem_region * mem     = NULL;
    struct numa_domain * domain = NULL;
    struct mem_region * ent     = NULL;

    if (!memory_affinity)
        return -EINVAL;

    if (!(memory_affinity->flags & ACPI_SRAT_MEM_ENABLED)) {
		NUMA_DEBUG("Disabled memory region affinity...\n");
		return 0;
    }

    if (memory_affinity->length == 0 ) { 
		NUMA_DEBUG("Whacky length zero memory region...\n");
    }


    acpi_table_print_srat_entry(header);

    mem = mm_boot_alloc(sizeof(struct mem_region));
    if (!mem) {
        ERROR_PRINT("Could not allocate mem region\n");
        return -1;
    }
    memset(mem, 0, sizeof(struct mem_region));

    mem->domain_id     = memory_affinity->proximity_domain & ((srat_rev < 2) ? 0xff : 0xffffffff);
    mem->base_addr     = memory_affinity->base_address;
    mem->len           = memory_affinity->length;
    mem->enabled       = memory_affinity->flags & ACPI_SRAT_MEM_ENABLED;
    mem->hot_pluggable = memory_affinity->flags & ACPI_SRAT_MEM_HOT_PLUGGABLE;
    mem->nonvolatile   = memory_affinity->flags & ACPI_SRAT_MEM_NON_VOLATILE;

    ASSERT(mem->domain_id < MAX_NUMA_DOMAINS);

    NUMA_DEBUG("Memory region: domain 0x%lx, base %p, len 0x%lx, enabled=%d hot_plug=%d nv=%d\n", mem->domain_id, mem->base_addr, mem->len, mem->enabled, mem->hot_pluggable, mem->nonvolatile);

    /* we've detected a new NUMA domain */
    if (sys->locality_info.domains[mem->domain_id] == NULL) {

	NUMA_DEBUG("Region is in new domain\n");

        domain = (struct numa_domain *)mm_boot_alloc(sizeof(struct numa_domain));
        if (!domain) {
            ERROR_PRINT("Could not allocate NUMA domain\n");
            return -1;
        }
        memset(domain, 0, sizeof(struct numa_domain));
        domain->id = mem->domain_id;
        INIT_LIST_HEAD(&(domain->regions));
        INIT_LIST_HEAD(&(domain->adj_list));

	if (mem->domain_id != (sys->locality_info.num_domains+1)) { 
	    NUMA_DEBUG("Memory regions are not in expected domain order, but that should be OK\n");
	}

        sys->locality_info.domains[domain->id] = domain;
        sys->locality_info.num_domains++;
        
    } else {
	NUMA_DEBUG("Region is in existing domain\n");
        domain = sys->locality_info.domains[mem->domain_id];
    }

    domain->num_regions++;
    domain->addr_space_size += mem->len;

    NUMA_DEBUG("Domain now has 0x%lx regions and size 0x%lx\n", domain->num_regions, domain->addr_space_size);

    if (list_empty(&domain->regions)) {
        list_add(&mem->entry, &domain->regions);
        return 0;
    } else {
        list_for_each_entry(ent, &domain->regions, entry) {
            if (mem->base_addr < ent->base_addr) {
		NUMA_DEBUG("Added region prior to region with base address 0x%lx\n",ent->base_addr);
                list_add_tail(&mem->entry, &ent->entry);
                return 0;
            }
        }
        list_add_tail(&mem->entry, &domain->regions);
    }


    return 0;
}


static int
acpi_parse_srat (struct acpi_table_header * hdr, void * arg)
{
    NUMA_DEBUG("Parsing SRAT...\n");
    srat_rev = hdr->revision;

    return 0;
}

/* NOTE: this is an optional table, and chances are we may not even see it */
static int 
acpi_parse_slit(struct acpi_table_header *table, void * arg)
{
    struct nk_locality_info * numa = (struct nk_locality_info*)arg;
    struct acpi_table_slit * slit = (struct acpi_table_slit*)table;
#ifdef NAUT_CONFIG_DEBUG_NUMA
    unsigned i, j;

    NUMA_DEBUG("Parsing SLIT...\n");
    NUMA_DEBUG("Locality Count: %llu\n", slit->locality_count);

    NUMA_DEBUG("  Entries:\n");
    NUMA_DEBUG("   ");
    for (i = 0; i < slit->locality_count; i++) {
        printk("%02u ");
    }
    printk("\n");

    for (i = 0; i < slit->locality_count; i++) {
        NUMA_DEBUG("%02u ", i);
        for (j = 0; j < slit->locality_count; j++) {
            printk("%02u ", *(uint8_t*)(slit->entry + i*slit->locality_count + j));
        }
        printk("\n");
    }

    NUMA_DEBUG("DONE.\n");

#endif

    numa->numa_matrix = mm_boot_alloc(slit->locality_count * slit->locality_count);
    if (!numa->numa_matrix) {
        ERROR_PRINT("Could not allocate NUMA matrix\n");
        return -1;
    }

    memcpy((void*)numa->numa_matrix, 
           (void*)slit->entry, 
           slit->locality_count * slit->locality_count);

    return 0;
}


/* 
 * Assumes that nk_acpi_init() has 
 * already been called 
 *
 */
int 
arch_numa_init (struct sys_info * sys)
{
    NUMA_PRINT("Parsing ACPI NUMA information...\n");

    /* SLIT: System Locality Information Table */
    if (acpi_table_parse(ACPI_SIG_SLIT, acpi_parse_slit, &(sys->locality_info))) { 
	NUMA_DEBUG("Unable to parse SLIT\n");
    }

    /* SRAT: Static Resource Affinity Table */
    if (!acpi_table_parse(ACPI_SIG_SRAT, acpi_parse_srat, &(sys->locality_info))) {

        NUMA_DEBUG("Parsing SRAT_MEMORY_AFFINITY table...\n");

        if (acpi_table_parse_srat(ACPI_SRAT_MEMORY_AFFINITY,
				  acpi_parse_memory_affinity, NAUT_CONFIG_MAX_CPUS * 2)) {
	    NUMA_DEBUG("Unable to parse memory affinity\n");
	}

        NUMA_DEBUG("DONE.\n");

        NUMA_DEBUG("Parsing SRAT_TYPE_X2APIC_CPU_AFFINITY table...\n");

        if (acpi_table_parse_srat(ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY,
				  acpi_parse_x2apic_affinity, NAUT_CONFIG_MAX_CPUS))	    {
	    NUMA_DEBUG("Unable to parse x2apic\n");
	}

        NUMA_DEBUG("DONE.\n");

        NUMA_DEBUG("Parsing SRAT_PROCESSOR_AFFINITY table...\n");

        if (acpi_table_parse_srat(ACPI_SRAT_PROCESSOR_AFFINITY,
				  acpi_parse_processor_affinity,
				  NAUT_CONFIG_MAX_CPUS)) { 
	    NUMA_DEBUG("Unable to parse processor affinity\n");
	}
        NUMA_DEBUG("DONE.\n");

    }

    NUMA_PRINT("DONE.\n");

    return 0;
}
