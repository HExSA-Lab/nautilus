#include <nautilus/nautilus.h>
#include <nautilus/cpuid.h>
#include <nautilus/cpu.h>
#include <nautilus/msr.h>
#include <nautilus/numa.h>
#include <nautilus/errno.h>
#include <nautilus/acpi.h>
#include <nautilus/list.h>
#include <nautilus/percpu.h>
#include <nautilus/math.h>
#include <nautilus/paging.h>
#include <nautilus/atomic.h>
#include <nautilus/naut_string.h>
#include <dev/apic.h>
#include <lib/liballoc.h>

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t


#ifndef NAUT_CONFIG_DEBUG_NUMA
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define NUMA_PRINT(fmt, args...) printk("NUMA: " fmt, ##args)
#define NUMA_DEBUG(fmt, args...) DEBUG_PRINT("NUMA: " fmt, ##args)

static int srat_rev;

static inline uint32_t 
next_pow2 (uint32_t v)
{
    v--;
    v |= v>>1;
    v |= v>>2;
    v |= v>>4;
    v |= v>>8;
    return ++v;
}

/* logical processor count */
static uint8_t
get_max_id_per_pkg (void)
{
    cpuid_ret_t ret;
    cpuid(1, &ret);
    return ((ret.b >> 16) & 0xff);
}


static uint8_t
amd_get_cmplegacy (void)
{
    cpuid_ret_t ret;
    cpuid(CPUID_AMD_FEATURE_INFO, &ret);
    return ret.c & 0x2;
}

static uint8_t
has_leafb (void)
{
    cpuid_ret_t ret;
    cpuid_sub(11, 0, &ret);
    return (ret.b != 0);
}

static uint8_t 
has_htt (void)
{
    cpuid_ret_t ret;
    cpuid(1, &ret);
    return !!(ret.d & (1<<28));
}

static void 
intel_probe_with_leafb (struct nk_topo_params *tp)
{
    panic("Intel CPUID leaf 0xB not currently supported\n");
}

static void
intel_probe_with_leaves14 (struct nk_topo_params * tp)
{
    cpuid_ret_t ret;
    if (cpuid_leaf_max() >= 0x4) {
        NUMA_DEBUG("Intel probing topo with leaves 1 and 4\n");
        cpuid_sub(4, 0, &ret);
        uint32_t eax_bits = (ret.a >> 26) & 0xff;
        tp->smt_bits  = next_pow2(get_max_id_per_pkg());
        tp->core_bits = next_pow2(eax_bits >> tp->smt_bits);
    } else { 
        NUMA_DEBUG("Intel probing topo using leaf 1 only\n");
        tp->smt_bits  = next_pow2(get_max_id_per_pkg());
        tp->core_bits = 0;
    }
}

static void
amd_get_topo_secondary (struct nk_topo_params * tp)
{
    cpuid_ret_t ret;
    NUMA_DEBUG("Attempting AMD topo probe\n");

    cpuid(CPUID_AMD_BASIC_INFO, &ret);

    unsigned num_cores;
    unsigned max_num_cores;
    unsigned threads_per_core;
    unsigned cores_per_proc;

    if (ret.a >= CPUID_AMD_EXT_INFO) {
        NUMA_DEBUG("AMD probing topo using CPUID fn 8000_0008\n");

        cpuid(CPUID_AMD_EXT_INFO, &ret);

        tp->core_bits = (ret.c >> 12) & 0xf;

        num_cores = (ret.c & 0xff) + 1;

        if (tp->core_bits) {
            max_num_cores  = 1 << tp->core_bits;
        } else {
            max_num_cores = num_cores;
        }

        cores_per_proc   = num_cores;
        threads_per_core = amd_get_cmplegacy() ? 1 : 
                                                 get_max_id_per_pkg()/cores_per_proc;
        NUMA_DEBUG("%u Cores per proc, %u threads per core (max logical count=%u)\n", cores_per_proc, threads_per_core, 
                get_max_id_per_pkg());

        tp->smt_bits = ilog2(threads_per_core);

    } else {
        NUMA_DEBUG("AMD probing topo using CPUID FN 0x0000_0001\n");
        tp->core_bits = next_pow2(get_max_id_per_pkg());
        tp->smt_bits  = 0;
    }
}


static void 
intel_get_topo_secondary (struct nk_topo_params * tp)
{
    if (cpuid_leaf_max() >= 0xb && has_leafb()) {
        intel_probe_with_leafb(tp);
    }  else {
        intel_probe_with_leaves14(tp);
    }
}

static inline void
get_topo_secondary (struct nk_topo_params * tp)
{
    if (nk_is_amd()) {
        amd_get_topo_secondary(tp);
    } else {
        intel_get_topo_secondary(tp);
    }
}


static void
get_topo_params (struct nk_topo_params * tp)
{
    if (has_htt()) {

        get_topo_secondary(tp);

    } else {
        NUMA_DEBUG("No HTT support, using default values\n");
        /* no HTT support, 1 core per package */
        tp->core_bits = 0;
        tp->smt_bits  = 0;
    }


    NUMA_DEBUG("Finished topo probe...\n");
    NUMA_DEBUG("\tcore_bits=0x%x\n", tp->core_bits);
    NUMA_DEBUG("\tsmt_bits =0x%x\n", tp->smt_bits);
}

static void 
assign_core_coords (struct cpu * me, struct nk_cpu_coords * coord, struct nk_topo_params *tp)
{
    uint32_t my_apic_id = apic_get_id(per_cpu_get(apic));

    coord->smt_id  = my_apic_id & ((1 << tp->smt_bits) - 1);
    coord->core_id = (my_apic_id >> tp->smt_bits) & ((1 << tp->core_bits) - 1);
    coord->pkg_id  = my_apic_id & ~((1 << (tp->smt_bits + tp->core_bits)) - 1);

    NUMA_DEBUG("Core OS ID: %u (APIC ID=0x%x):\n", me->id, my_apic_id);
    NUMA_DEBUG("\tLogical Core ID:  %u\n", coord->smt_id);
    NUMA_DEBUG("\tPhysical Core ID: %u\n", coord->core_id);
    NUMA_DEBUG("\tPhysical Chip ID: %u\n", coord->pkg_id);
}


/* 
 *
 * to be called by all cores
 *
 */
int
nk_cpu_topo_discover (struct cpu * me)
{
    struct nk_cpu_coords * coord = NULL;
    struct nk_topo_params * tp = NULL;

    coord = (struct nk_cpu_coords*)malloc(sizeof(struct nk_cpu_coords));
    if (!coord) {
        ERROR_PRINT("Could not allocate coord struct for CPU %u\n", my_cpu_id());
        return -1;
    }
    memset(coord, 0, sizeof(struct nk_cpu_coords));

    tp = (struct nk_topo_params*)malloc(sizeof(struct nk_topo_params));
    if (!tp) {
        ERROR_PRINT("Could not allocate param struct for CPU %u\n", my_cpu_id());
        return -1;
    }
    memset(tp, 0, sizeof(struct nk_topo_params));

    get_topo_params(tp);

    /* now we can determine the CPU coordinates */
    assign_core_coords(me, coord, tp);

    me->tp    = tp;
    me->coord = coord;

    return 0;
}


void acpi_table_print_srat_entry(struct acpi_subtable_header * header)
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
    if (!processor_affinity)
        return -EINVAL;

    acpi_table_print_srat_entry(header);

    return 0;
}


static void
numa_srat_init (struct acpi_subtable_header * hdr,
                struct sys_info * sys)
{

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

    acpi_table_print_srat_entry(header);

    /* TODO: wildly inefficient and kinda dumb */
    for (i = 0; i < sys->num_cpus; i++) {

        if (sys->cpus[i]->lapic_id == processor_affinity->apic_id) {
            domain_id = processor_affinity->proximity_domain_lo | 
                (((*(uint32_t*)(&(processor_affinity->proximity_domain_hi))) & 0xffffff) << 8);

            sys->cpus[i]->numa_domain = domain_id;
            break;
        }
    }

    return 0;
}

static int 
acpi_parse_memory_affinity(struct acpi_subtable_header * header,
               const unsigned long end)
{
    struct acpi_srat_mem_affinity *memory_affinity
        = container_of(header, struct acpi_srat_mem_affinity, header);

    struct sys_info * sys       = &(nk_get_nautilus_info()->sys);
    struct mem_region * mem     = NULL;
    struct numa_domain * domain = NULL;

    if (!memory_affinity)
        return -EINVAL;

    acpi_table_print_srat_entry(header);

    mem = malloc(sizeof(struct mem_region));
    if (!mem) {
        ERROR_PRINT("Could not allocate mem region\n");
        return -1;
    }
    memset(mem, 0, sizeof(struct mem_region));

    mem->domain_id     = memory_affinity->proximity_domain;
    mem->base_addr     = memory_affinity->base_address;
    mem->len           = memory_affinity->length;
    mem->enabled       = memory_affinity->flags & 0x1;
    mem->hot_pluggable = memory_affinity->flags & 0x2;
    mem->nonvolatile   = memory_affinity->flags & 0x4;

    /* update NUMA domain-specific info */
    domain = &(sys->locality_info.domains[mem->domain_id]);

    list_add(&(mem->entry), &(domain->regions));

    domain->num_regions++;

    domain->addr_space_size += mem->len;

    return 0;
}


static int
acpi_parse_srat (struct acpi_table_header * hdr, void * arg)
{
    NUMA_DEBUG("Parsing SRAT...\n");

    return 0;
}


static void 
dump_mem_regions (struct numa_domain * d)
{
    struct mem_region * reg = NULL;
    unsigned i = 0;

    list_for_each_entry(reg, &(d->regions), entry) {
        printk("\tMemory Region %u:\n", i);
        printk("\t\tRange:         %p - %p\n", (void*)reg->base_addr, (void*)(reg->base_addr + reg->len));
        printk("\t\tSize:          %lu.%lu MB\n", reg->len / 1000000, reg->len % 1000000);
        printk("\t\tEnabled:       %s\n", reg->enabled ? "Yes" : "No");
        printk("\t\tHot Pluggable: %s\n", reg->hot_pluggable ? "Yes" : "No");
        printk("\t\tNon-volatile:  %s\n", reg->nonvolatile ? "Yes" : "No");
        i++;
    }
}


static void
dump_numa_domains (struct nk_locality_info * numa)
{
    unsigned i;

    printk("%u NUMA Domains:\n", numa->num_domains);
    printk("----------------\n");

    for (i = 0; i < numa->num_domains; i++) {

        struct numa_domain * domain = &(numa->domains[i]);

        printk("Domain %u (Total Size=%lu.%lu MB, %u Region%s):\n", 
                domain->id,
                domain->addr_space_size / 1000000,
                domain->addr_space_size % 1000000,
                domain->num_regions,
                domain->num_regions > 1 ? "s" : "");

        dump_mem_regions(domain);
        printk("\n");
    }
}


static void 
dump_numa_matrix (struct nk_locality_info * numa)
{
    unsigned i,j;

    if (!numa || !numa->numa_matrix) {
        return;
    }

    printk("NUMA Distance Matrix:\n");
    printk("---------------------\n");
    printk("   ");

    for (i = 0; i < numa->num_domains; i++) {
        printk("%02u ", i);
    }
    printk("\n");

    for (i = 0; i < numa->num_domains; i++) {
        printk("%02u ", i);
        for (j = 0; j < numa->num_domains; j++) {
            printk("%02u ", *(numa->numa_matrix + i*numa->num_domains + j));
        }
        printk("\n");
    }

}

struct mem_region *
nk_get_base_region_by_num (unsigned num)
{
    struct nk_locality_info * loc = &(nautilus_info.sys.locality_info);

    ASSERT(num < loc->num_domains);

    struct list_head * first = loc->domains[num].regions.next;
    /* TODO: add a better way to do this */
    return list_entry(first, struct mem_region, entry);
}

struct mem_region * 
nk_get_base_region_by_cpu (unsigned cpu)
{
    ASSERT(cpu >= 0);

    struct nk_locality_info * loc = &(nk_get_nautilus_info()->sys.locality_info);
    uint32_t domain_id = nk_get_nautilus_info()->sys.cpus[cpu]->numa_domain;
    struct numa_domain * domain = &(loc->domains[domain_id]);

    NUMA_DEBUG("CPU %u's domain is %u\n", cpu, domain_id);

    /* TODO what to return if there are more regions? */
    struct list_head * first = domain->regions.next;
    return list_entry(first, struct mem_region, entry);
}

unsigned
nk_my_numa_node (void)
{
    return per_cpu_get(numa_domain);
}


void
nk_dump_numa_info (void)
{
    struct nk_locality_info * numa = &(nk_get_nautilus_info()->sys.locality_info);

    printk("===================\n");
    printk("     NUMA INFO:\n");
    printk("===================\n");
    
    dump_numa_domains(numa);

    dump_numa_matrix(numa);

    printk("\n");
    printk("======================\n");
    printk("     END NUMA INFO:\n");
    printk("======================\n");
}

static int 
acpi_parse_slit(struct acpi_table_header *table, void * arg)
{
    struct nk_locality_info * numa = (struct nk_locality_info*)arg;
    struct acpi_table_slit * slit = (struct acpi_table_slit*)table;
    unsigned i;
#ifdef NAUT_CONFIG_DEBUG_NUMA
    unsigned j;

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

    numa->num_domains = slit->locality_count;
    numa->domains = malloc(sizeof(struct numa_domain) * numa->num_domains);
    if (!numa->num_domains) {
        ERROR_PRINT("Could not allocate NUMA domain structures\n");
        return -1;
    }

    /* initialize each domain, we'll fill in it's memory regions soon */
    memset(numa->domains, 0, sizeof(struct numa_domain) * numa->num_domains);

    for (i = 0; i < numa->num_domains; i++) {
        numa->domains[i].id = i;
        INIT_LIST_HEAD(&(numa->domains[i].regions));
    }

    numa->numa_matrix = malloc(slit->locality_count * slit->locality_count);
    if (!numa->numa_matrix) {
        ERROR_PRINT("Could not allocate NUMA matrix\n");
        return -1;
    }

    memcpy((void*)numa->numa_matrix, 
           (void*)slit->entry, 
           slit->locality_count * slit->locality_count);

    return 0;
}


unsigned 
nk_get_num_domains (void)
{
    struct nk_locality_info * l = &(nk_get_nautilus_info()->sys.locality_info);

    return l->num_domains;
}



/*
 *
 * only called by BSP once
 *
 */
int
nk_numa_init (void)
{
    struct sys_info * sys = &(nk_get_nautilus_info()->sys);
    unsigned i;
    NUMA_PRINT("Parsing ACPI NUMA information...\n");

    /* SLIT: System Locality Information Table */
    acpi_table_parse(ACPI_SIG_SLIT, acpi_parse_slit, &(sys->locality_info));

    /* SRAT: Static Resource Affinity Table */
    if (!acpi_table_parse(ACPI_SIG_SRAT, acpi_parse_srat, &(sys->locality_info))) {
        NUMA_DEBUG("Parsing SRAT_TYPE_X2APIC_CPU_AFFINITY table...\n");

        acpi_table_parse_srat(ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY,
                           acpi_parse_x2apic_affinity, NAUT_CONFIG_MAX_CPUS);
        NUMA_DEBUG("DONE.\n");

        NUMA_DEBUG("Parsing SRAT_PROCESSOR_AFFINITY table...\n");

        acpi_table_parse_srat(ACPI_SRAT_PROCESSOR_AFFINITY,
                           acpi_parse_processor_affinity,
                           NAUT_CONFIG_MAX_CPUS);
        NUMA_DEBUG("DONE.\n");

        NUMA_DEBUG("Parsing SRAT_MEMORY_AFFINITY table...\n");

        acpi_table_parse_srat(ACPI_SRAT_MEMORY_AFFINITY,
                acpi_parse_memory_affinity, NAUT_CONFIG_MAX_CPUS * 2);

        NUMA_DEBUG("DONE.\n");
    }


    NUMA_PRINT("DONE.\n");

#ifdef NAUT_CONFIG_DEBUG_NUMA
    nk_dump_numa_info();
#endif

    return 0;
}

