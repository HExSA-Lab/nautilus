
#ifndef __MB_UTIL_H__
#define __MB_UTIL_H__
#include <naut_types.h>
#include <paging.h>
uint_t multiboot_get_size(ulong_t mbd);
addr_t multiboot_get_phys_mem(ulong_t mbd);
void multiboot_parse(ulong_t mbd, ulong_t magic);
void multiboot_rsv_mem_regions(struct mem_info * mem, ulong_t mbd);


#endif
