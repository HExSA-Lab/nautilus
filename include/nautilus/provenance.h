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


#ifndef PROVENANCE_H
#define PROVENANCE_H

#define SYMTAB_MAGIC 0x00952700
#define SECTAB_MAGIC 0x00952800

/* Serialized structure of nautilus symbol table (natuilus.syms) */
typedef struct {
	uint64_t value;
	uint64_t offset;
} symentry;

/* Serialized structure of nautilus section table (natuilus.secs) */
typedef struct {
	uint64_t value;
	uint32_t sec_size;
	uint64_t offset;
} secentry;

/* Structure of each entry in the Section Table */
typedef struct {
	char* name;
	uint64_t start_addr;
	uint64_t end_addr;
	uint32_t size;
} prov_sectab_entry;

/* Structure of each entry in the Symbol Table */
typedef struct {
	char* symbol;
	uint64_t start_addr;
	uint64_t end_addr;
} prov_symtab_entry;

/* Symbol Table */
typedef struct {
	prov_sectab_entry** entries;
	int count;
} prov_sectab;

/* Symbol Table */
typedef struct {
	prov_symtab_entry** entries;
	int count;
} prov_symtab;

typedef struct {
	char* file_name;
	char* file_dir;
} prov_file_info;

typedef struct {
	int line_number;
	char* line_string;
} prov_line_info;

typedef struct {
	char* symbol;
	char* section;
	prov_line_info* line_info;
	prov_file_info* file_info;
} provenance_info;

void prov_init();
provenance_info prov_get_info(uint64_t addr);

#endif
