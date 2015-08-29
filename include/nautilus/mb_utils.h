
#ifndef __MB_UTIL_H__
#define __MB_UTIL_H__
#include <nautilus/naut_types.h>
#include <nautilus/paging.h>
#include <nautilus/multiboot2.h>


uint_t multiboot_get_size(ulong_t mbd);
addr_t multiboot_get_phys_mem(ulong_t mbd);
ulong_t multiboot_get_sys_ram(ulong_t mbd);
struct multiboot_info * multiboot_parse(ulong_t mbd, ulong_t magic);
int mb_is_hrt_environ (ulong_t mbd);
void* mb_get_first_hrt_addr (ulong_t mbd);


struct multiboot_info {
    char * boot_loader;
    char * boot_cmd_line;
    void * sec_hdr_start;
    struct multiboot_tag_hrt * hrt_info;
};




#endif
