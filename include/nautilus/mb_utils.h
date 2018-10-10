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

#ifndef __MB_UTIL_H__
#define __MB_UTIL_H__
#include <nautilus/naut_types.h>
#include <nautilus/list.h>
#include <nautilus/paging.h>
#include <nautilus/multiboot2.h>


uint_t multiboot_get_size(ulong_t mbd);
addr_t multiboot_get_phys_mem(ulong_t mbd);
addr_t multiboot_get_modules_end(ulong_t mbd);
ulong_t multiboot_get_sys_ram(ulong_t mbd);
struct multiboot_info * multiboot_parse(ulong_t mbd, ulong_t magic);
int mb_is_hrt_environ (ulong_t mbd);
void* mb_get_first_hrt_addr (ulong_t mbd);

typedef enum {
    MOD_SYMTAB,
    MOD_PROGRAM,
} mod_type_t;

struct multiboot_mod {
    addr_t start;
    addr_t end;
    mod_type_t type;
    char * cmdline;
    struct list_head elm;
};

struct multiboot_info {
    char * boot_loader;
    char * boot_cmd_line;
    void * sec_hdr_start;
    struct multiboot_tag_hrt * hrt_info;
    int mod_count;
    struct list_head mod_list;
};




#endif
