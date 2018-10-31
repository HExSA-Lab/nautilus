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
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2018, Conghao Liu
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 *
 * Authors:  Conghao Liu <cliu115@hawk.iit.edu>
 *           Kyle Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/linker.h>
#include <nautilus/multiboot2.h>
#include <nautilus/mb_utils.h>
#include <nautilus/elf.h>
#include <nautilus/nautilus.h>
#include <nautilus/hashtable.h>
#include <nautilus/vc.h>
#include <nautilus/mm.h>
#include <nautilus/prog.h>

#define ERROR(fmt, args...) ERROR_PRINT("LINKER: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("LINKER: " fmt, ##args)
#define INFO(fmt, args...)  INFO_PRINT("LINKER: " fmt, ##args)
#define WARN(fmt, args...)  WARN_PRINT("LINKER: " fmt, ##args)

#define MAX_SYM_LEN 128

#ifndef NAUT_CONFIG_DEBUG_LINKER
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif


/*
 * @name is the string containing the symbol name
 * @addr will be filled in with the resolved address of the symbol on success
 * @value will be zero if the symbol does not 
 * reside in the program binary (meaning it lives
 * in the kernel somewhere)
 *
 * @return: 0 on success, -1 on error (when we can't resolve the symbol)
 *
 */
static int
resolve_symbol (struct nk_link_info * linfo, 
                struct nk_prog_info * pinfo,
                char * name,
                uint64_t * addr,
                const uint64_t value)
{
    if (value == 0) { //need to look up nautilus symbol table

        DEBUG("Looking up symbol (%s):\n", name);

        for (int i = 0; i < linfo->symtab.sym_count; i++) {

            int sym_offset = linfo->symtab.entries[i].offset;

            if (strncmp(name, &(linfo->symtab.strtab[sym_offset]), MAX_SYM_LEN) == 0) {

                DEBUG("-->name:           %s\n", name);
                DEBUG("-->GOT entry addr: %016llx\n", linfo->symtab.entries[i].value);
                DEBUG("-->PLT entry addr: %016llx\n", addr);

                *addr = linfo->symtab.entries[i].value;

                DEBUG("-->PLT entry value: %016llx\n", addr[0]);

                DEBUG("Symbol value resolved to %p\n", (void*)*addr);

                return 0;
            }
            
        }

        ERROR("Could not resolve symbol (%s)\n", name);

        return -1;

    } else {
        DEBUG("Resolving symbol in program binary (%s: %p)\n", name, (void*)value);
        *addr = value + pinfo->mod->start;
    }

    return 0;
}


int
nk_link_prog (struct nk_link_info * linfo, struct nk_prog_info * prog)
{
    void* filebuf = (void*) prog->mod->start;

    Elf64_Ehdr* ehd      = (Elf64_Ehdr*) filebuf;
    Elf64_Off sechdrt    = ehd->e_shoff;
    Elf64_Shdr* shdraddr = filebuf + sechdrt;
    Elf64_Half strtidx   = ehd->e_shstrndx;
    Elf64_Half secnum    = ehd->e_shnum;
    char* shdrstrtbl     = filebuf + shdraddr[strtidx].sh_offset;

    Elf64_Addr* dyn_rela = 0;
    Elf64_Xword dynenum  = 0;
    Elf64_Addr* plt_rela = 0;
    Elf64_Xword pltenum  = 0;
    Elf64_Addr* dynsym   = 0;
    Elf64_Xword dsymenum = 0;
    char* strtbl = 0;

    char * name     = NULL;
    uint64_t * addr = NULL;
    uint64_t value  = 0;

    DEBUG("Linking prog (%p)\n", prog);

    for (int i = 1; i < secnum; i++) {

        if (strncmp(&shdrstrtbl[shdraddr[i].sh_name], ".dynsym", 7) == 0) {
            dynsym = shdraddr[i].sh_offset + filebuf;
            dsymenum = shdraddr[i].sh_size / shdraddr[i].sh_entsize;
        }

        if (strncmp(&shdrstrtbl[shdraddr[i].sh_name],".rela.dyn", 9) == 0) {
            dyn_rela = shdraddr[i].sh_offset + filebuf;
            dynenum = shdraddr[i].sh_size / shdraddr[i].sh_entsize;
        }

        if (strncmp(&shdrstrtbl[shdraddr[i].sh_name], ".rela.plt", 9) == 0) {
            plt_rela = shdraddr[i].sh_offset + filebuf;
            pltenum = shdraddr[i].sh_size / shdraddr[i].sh_entsize;
        }

        if (strncmp(&shdrstrtbl[shdraddr[i].sh_name], ".dynstr", 7) == 0) {
            strtbl = (char*)(shdraddr[i].sh_offset + filebuf);
        }
    }
    
    // parse global symbol table
    
    DEBUG("Parsing global symbol table\n");

    Elf64_Sym* symidx   = 0;
    Elf64_Rela* relaidx = 0;

    relaidx = (Elf64_Rela*) dyn_rela;
    symidx  = (Elf64_Sym*) dynsym;

    for (int i = 0; i < dynenum; i++) {

        name  = &strtbl[symidx[relaidx[i].r_info >> 32].st_name];
        addr  = filebuf + relaidx[i].r_offset;
        value = symidx[relaidx[i].r_info >> 32].st_value;

        resolve_symbol(linfo, prog, name, addr, value);
    }

    // resolve PLT
    relaidx = (Elf64_Rela*) plt_rela;

    DEBUG("Resolving PLT\n");

    for (int i = 0; i < pltenum; i++) {

        name  = &strtbl[symidx[relaidx[i].r_info >> 32].st_name];
        addr  = filebuf + relaidx[i].r_offset;
        value = symidx[relaidx[i].r_info >> 32].st_value;

        resolve_symbol(linfo, prog, name, addr, value);
    }

    return 0;
}


/*
 * The kernel symbol table is a formatted blob containing
 * symbol descriptors derived from the kernel's ELF symbol
 * table. 
 *
 * |=======================================|
 * |       Symbol Table Magic (4B)         |
 * |---------------------------------------|
 * |        Symbol Desc. Offest (4B)       |
 * |---------------------------------------|
 * |         Symbol Count (4B)             |
 * |---------------------------------------|
 * |              String Table             |
 * |                  ...                  |
 * |                  ...                  |
 * |---------------------------------------|
 * |     Symbol Desciptors (16B each)      |
 * |         (see symentry_t)              |
 * |                ...                    |
 * |                ...                    |
 * |=======================================|
 *
 *
 */

int
nk_linker_init (struct naut_info * naut)
{
    struct list_head * tmp      = NULL;
    struct multiboot_mod * mod  = NULL;
    struct nk_link_info * linfo = NULL;

    DEBUG("Initializing linker\n");

    linfo = malloc(sizeof(struct nk_link_info));
    if (!linfo) {
        ERROR("Could not allocate linker info\n");
        return -1;
    }
    memset(linfo, 0, sizeof(struct nk_link_info));

    list_for_each(tmp, &(naut->sys.mb_info->mod_list)) {

        mod = list_entry(tmp, struct multiboot_mod, elm);

        if (mod->type == MOD_SYMTAB) {

            uint32_t * hnum = (uint32_t*) mod->start;

            DEBUG("Found kernel symbol table:\n");

            // See note above for explanation of these entries
            linfo->symtab.entries   = (symentry_t*) (mod->start + hnum[1]);
            linfo->symtab.sym_count = hnum[2];
            linfo->symtab.strtab    = (char*) (mod->start + 12);
            linfo->ready            = 1;

            DEBUG("|--> Symbol Table Addr:            0x%08llx\n", mod->start);
            DEBUG("|--> Magic Cookie:                 0x%08x\n", hnum[0]);
            DEBUG("|--> Symbol Desc. Table Offset:    0x%016x\n", hnum[1]);
            DEBUG("|--> Symbol Desc. Table Addr:      0x%016llx\n", linfo->symtab.entries);
            DEBUG("|--> Symbol Count:                 %d\n", linfo->symtab.sym_count);
            DEBUG("|--> String Table Addr:            0x%016llx\n", linfo->symtab.strtab);

            break;
        }

    }

    if (!linfo->ready) {
        WARN("No symbol table found...linker will not work properly\n");
    }

    naut->sys.linker_info = linfo;

    return 0;
}
