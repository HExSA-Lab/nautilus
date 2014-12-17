#ifndef __NUMA_H__
#define __NUMA_H__

#ifdef __cplusplus
extern "C" {
#endif

struct nk_locality_info {
    uint64_t num_regions;
    uint8_t * numa_matrix;
};

struct cpu;
int nk_cpu_topo_discover(struct cpu* me);
int nk_numa_init(void);
void nk_dump_numa_matrix(struct nk_locality_info * numa);


struct nk_topo_params {
    uint32_t smt_bits;
    uint32_t core_bits;
};

struct nk_cpu_coords {
    uint32_t smt_id;  // logical CPU ID within core
    uint32_t core_id; // physical core within package
    uint32_t pkg_id;  // package id
};

    

#ifdef __cplusplus
}
#endif

#endif
