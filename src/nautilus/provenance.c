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
 * Copyright (c) 2020, Nanda Velugoti <nvelugoti@hawk.iit.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Nanda Velugoti <nvelugoti@hawk.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/list.h>
#include <nautilus/mb_utils.h>
#include <nautilus/nautilus.h>
#include <nautilus/provenance.h>

#ifndef NAUT_CONFIG_DEBUG_PROVENANCE
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define PROV_PRINT(fmt, args...) printk("PROVENANCE: " fmt, ##args)
#define PROV_DEBUG(fmt, args...) DEBUG_PRINT("PROVENANCE DEBUG: " fmt, ##args)
#define PROV_ERROR(fmt, args...) ERROR_PRINT("PROVENANCE ERROR: " fmt, ##args)

#define PROV_MALLOC_CHECKED(ptr, type, size)				\
	ptr = (type*) malloc(sizeof(type)*size); 				\
	if (!ptr) {												\
		PROV_ERROR("Failed to allocate memory\n");			\
		return;												\
	}														\

static bool_t prov_sectab_initialized = false;
static bool_t prov_symtab_initialized = false;
static bool_t prov_initialized = false;

extern struct naut_info nautilus_info;
static prov_table *symtab = NULL, *sectab = NULL;

/* Binary (Interval) Search over Sorted Section/Symbol Table */
static prov_entry* provtab_lookup(prov_table* table, uint64_t addr) {
	int start = 0, end = table->count;
	while (start <= end) {
		int middle = (start + end) / 2;
		prov_entry* entry = table->entries[middle];
		if((entry->start_addr <= addr) && (addr <= entry->end_addr))
			return entry;
		else if(entry->start_addr > addr)
			end = middle - 1;
		else if (addr > entry->end_addr)
			start = middle + 1;
	}

	return NULL;
}

static char* prov_get_name(prov_table* table, uint64_t addr) {
	prov_entry* entry = provtab_lookup(table, addr);
	return (entry != NULL) ? entry->name : NULL;
}

static void sectab_resolve_addr_ranges() {
	for (int i=0; i<(sectab->count); i++) {
		prov_entry* entry = sectab->entries[i];
		entry->end_addr = entry->start_addr + entry->size;
	}
}

static void symtab_resolve_last_entry() {
	/* 
		Upate the end address of the last entry of symbol table table with the last
		entry of section table.

		This is done because we don't have size or end address information for symbol table entries.
		All we have are start addresses of symbols. So symtab_resolve_addr_ranges() takes care of 
		the end addresses of all symbol table entries except for the last one. So we use section table
		information, which has the size of each entry, to determine the end address of the last entry
		of section table.
	*/
	prov_entry* last_entry = symtab->entries[symtab->count-1];
	last_entry->end_addr = sectab->entries[sectab->count-1]->end_addr;
	last_entry->size = last_entry->end_addr - last_entry->start_addr;
}

static void symtab_resolve_addr_ranges() {
	for (int i=0; i<(symtab->count-1); i++) {
		int offset = ((symtab->entries[i]->start_addr) < (symtab->entries[i+1]->start_addr)) ? -1 : 0;
		symtab->entries[i]->end_addr = symtab->entries[i+1]->start_addr + offset;
		symtab->entries[i]->size = symtab->entries[i]->end_addr - symtab->entries[i]->start_addr;
	}
	prov_entry* last_entry = symtab->entries[(symtab->count)-1];
	last_entry->size = last_entry->end_addr - last_entry->start_addr;
}

static void sectab_init(uint64_t mod_start) {
	PROV_DEBUG("Initializing Section Table ...\n");
	uint32_t* secs_base_addr = (uint32_t*) mod_start;
	uint32_t magic = *secs_base_addr;
	uint32_t offset = *(secs_base_addr+1);
	uint32_t noe = *(secs_base_addr+2);
	PROV_DEBUG("Section Table Magic: %x\tOffset: %x\tNo. of Entries: %d\n", magic, offset, noe);
	PROV_MALLOC_CHECKED(sectab, prov_table, 1);
	PROV_MALLOC_CHECKED(sectab->entries, prov_entry*, noe);
	sectab->count = 0;
	for (int i=0; i<noe; i++) {
		secentry* entry = (secentry*) (((char*)(secs_base_addr)) + offset + (sizeof(secentry)*i));
		char* name = (char*) ((char*)(secs_base_addr+3) + entry->offset);
		uint64_t addr = entry->value;
		uint32_t size = entry->sec_size;
		PROV_MALLOC_CHECKED(sectab->entries[i], prov_entry, 1);
		sectab->entries[i]->name = name;
		sectab->entries[i]->start_addr = addr;
		sectab->entries[i]->end_addr = -1;
		sectab->entries[i]->size = size;
		sectab->count++;
	}
	sectab_resolve_addr_ranges();

	PROV_DEBUG("[Nr]\tStart Address\tEnd Address\tSize\t\tSection Name\n");
	for (int i=0; i<sectab->count; i++) {
		prov_entry* entry = sectab->entries[i];
		PROV_DEBUG("[%02d]\t%08x\t%08x\t%08x\t%s\n", i+1, entry->start_addr, entry->end_addr, entry->size, entry->name);
	}
	prov_sectab_initialized = true;
}

static void symtab_init(uint64_t mod_start) {
	PROV_DEBUG("Initializing Symbol Table ...\n");
	uint32_t* syms_base_addr = (uint32_t*) mod_start;
	uint32_t magic = *syms_base_addr;
	uint32_t offset = *(syms_base_addr+1);
	uint32_t noe = *(syms_base_addr+2);
	PROV_DEBUG("Symbol Table Magic: %x\tOffset: %x\tNo. of Entries: %d\n", magic, offset, noe);
	PROV_MALLOC_CHECKED(symtab, prov_table, 1);
	PROV_MALLOC_CHECKED(symtab->entries, prov_entry*, noe);
	symtab->count = 0;
	for (int i=0; i<noe; i++) {
		symentry* entry = (symentry*) (((char*)(syms_base_addr)) + offset + (sizeof(symentry)*i));
		char* symbol = (char*) ((char*)(syms_base_addr+3) + entry->offset);
		uint64_t addr = entry->value;
		PROV_MALLOC_CHECKED(symtab->entries[i], prov_entry, 1);
		symtab->entries[i]->name = symbol;
		symtab->entries[i]->start_addr = addr;
		symtab->entries[i]->end_addr = -1;
		symtab->count++;
	}
	symtab_resolve_addr_ranges();

	PROV_DEBUG("[Nr]\tStart Address\tEnd Address\tSymbol\n");
	for (int i=0; i<symtab->count; i++) {
		prov_entry* entry = symtab->entries[i];
		PROV_DEBUG("[%08d]\t%08x\t%08x\t%s\n", i+1, entry->start_addr, entry->end_addr, entry->name);
	}
	prov_symtab_initialized = true;
}

static void deinit_prov_table(prov_table* table) {
	if (table && table->entries) {
		for (int i=0; i<table->count; i++) {
			free(table->entries[i]);
		}
		free(table->entries);
	}
	free(table);
}

static void nk_prov_deinit() {
	deinit_prov_table(sectab);
	prov_sectab_initialized = false;

	deinit_prov_table(symtab);
	prov_symtab_initialized = false;

	prov_initialized = false;
}

provenance_info* nk_prov_get_info(uint64_t addr) {
	if (!prov_initialized)
		return NULL;

	provenance_info* prov_info = (provenance_info*) malloc(sizeof(provenance_info));
	prov_info->symbol = prov_get_name(symtab, addr);
	prov_info->section = prov_get_name(sectab, addr);
	prov_info->line_info = NULL; // Placeholder.
	prov_info->file_info = NULL;	// Placeholder.
	return prov_info;
}

void nk_prov_init() {
	if (!prov_initialized) {
		struct list_head *curr = NULL;
		struct multiboot_info* mb_info = nautilus_info.sys.mb_info;

		list_for_each(curr, &(mb_info->mod_list)) {
			struct multiboot_mod* mod = list_entry(curr, struct multiboot_mod, elm);
			uint32_t magic = *((uint32_t*) (mod->start));
			switch (magic) {
				case SYMTAB_MAGIC:
					symtab_init((uint64_t) mod->start);
					break;
				case SECTAB_MAGIC:
					sectab_init((uint64_t) mod->start);
					break;
			}
		}

		if (!prov_sectab_initialized)
			PROV_ERROR("Failed to load Nautilus section table information!\n");
		
		if (!prov_symtab_initialized)
			PROV_ERROR("Failed to load Nautilus symbol table information!\n");

		if (prov_sectab_initialized && prov_symtab_initialized) {
			symtab_resolve_last_entry();
			prov_initialized = true;
			PROV_DEBUG("Initialization successful\n");
		} else {
			PROV_ERROR("Initialization failed\t");
			nk_prov_deinit();
		}
	}
}
