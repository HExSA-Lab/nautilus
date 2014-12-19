#ifndef __RANDOM_H__
#define __RANDOM_H__

#ifdef __cplusplus
extern "C" {
#endif


#include <nautilus/spinlock.h>

struct nk_rand_info {
    spinlock_t lock;
    uint64_t seed;
    uint64_t xi; 
    uint64_t n;
};

struct cpu;
int nk_rand_init(struct cpu * cpu);
void nk_rand_set_xi(uint64_t xi);
void nk_rand_seed(uint64_t seed);
void nk_get_rand_bytes(uint8_t *buf, unsigned len);

#ifdef __cplusplus
}
#endif

#endif
