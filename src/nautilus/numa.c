#include <nautilus/nautilus.h>
#include <nautilus/cpuid.h>
#include <nautilus/cpu.h>
#include <nautilus/msr.h>
#include <nautilus/numa.h>
#include <nautilus/percpu.h>
#include <dev/apic.h>
#include <lib/liballoc.h>


#ifndef NAUT_CONFIG_DEBUG_NUMA
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define NUMA_PRINT(fmt, args...) printk("NUMA: " fmt, ##args)
#define NUMA_DEBUG(fmt, args...) DEBUG_PRINT("NUMA: " fmt, ##args)

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

static uint8_t
get_max_id_per_pkg (void)
{
    cpuid_ret_t ret;
    cpuid(1, &ret);
    return ((ret.b >> 16) & 0xff);
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

    if (ret.a >= 0x80000008) {
        NUMA_DEBUG("AMD probing topo using CPUID fn 8000_0008\n");

        cpuid(0x80000008, &ret);

        if (((ret.c >> 12) & 0xf)) {
            tp->core_bits = ((ret.c >> 12) & 0xf);
        } else {
            tp->core_bits = next_pow2(ret.c & 0xff);
        }

        tp->smt_bits = next_pow2(get_max_id_per_pkg() >> tp->core_bits);

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
assign_core_coords (struct nk_cpu_coords * coord, struct nk_topo_params *tp)
{
    uint32_t my_apic_id = apic_get_id(per_cpu_get(apic));

    coord->smt_id  = my_apic_id & ((1 << tp->smt_bits) - 1);
    coord->core_id = (my_apic_id >> tp->smt_bits) & ((1 << tp->core_bits) - 1);
    coord->pkg_id  = my_apic_id & ~((1 << (tp->smt_bits + tp->core_bits)) - 1);

    NUMA_DEBUG("OS Core %u:\n", my_apic_id);
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
    assign_core_coords(coord, tp);

    me->tp    = tp;
    me->coord = coord;

    return 0;
}


static void
parse_srat (struct naut_info * naut)
{
    NUMA_DEBUG("Parsing SRAT...\n");

}


/*
 *
 * only called by BSP once
 *
 */
int
nk_topo_setup (struct naut_info * naut)
{
    NUMA_DEBUG("Parsing NUMA-related structures\n");
    parse_srat(naut);
    return 0;
}

