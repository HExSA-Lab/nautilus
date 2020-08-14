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

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/fiber.h>
#include <nautilus/scheduler.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>
#include <nautilus/backtrace.h>
#include <nautilus/provenance.h>
#include <test/test.h>

#define PRINT(...) nk_vc_printf(__VA_ARGS__)

#define USAGE "provenance [ panic | info addr | bt ]"

struct nk_virtual_console *vc;

int handle_prov_info(uint64_t addr) {
	provenance_info* prov_info = nk_prov_get_info(addr);
	if(prov_info != NULL) {
		PRINT("Symbol: %s\n", 
				(prov_info->symbol != NULL) ? (char*) prov_info->symbol : "???");
		PRINT("Section: %s\n", 
				(prov_info->section != NULL) ? (char*) prov_info->section : "???");
		free(prov_info);
		return 0;
	}
	return -1;
}

/*
 * This function is to test the provenance feature functionality.
 * Invokes panic
 */
__attribute__((section (".prov")))
static int handle_provenance(char* buf, void* priv) {
	uint64_t addr;
	char opt;
	if (((opt = 'p', strcmp(buf, "provenance panic"))==0) ||
		((opt = 'b', strcmp(buf, "provenance bt"))==0) ||
	    ((opt = 'i', sscanf(buf, "provenance info %lx", &addr)) == 1) || 
		((opt = 'q'))) {
		switch (opt) {
			case 'p':
				panic("Provenance: panic invoked!\n");
				break;
			case 'b':
				backtrace_here();
				break;
			case 'i':
				handle_prov_info(addr);
				break;
			case 'q':
			default:
				PRINT("Invalid argument(s)!\n");
				PRINT("Usage: %s\n", USAGE);
				break;
		}
	}
	return 0;
}

/******************* Shell Commands *******************/

static struct shell_cmd_impl provenance = {
	.cmd      = "provenance",
	.help_str = USAGE,
	.handler  = handle_provenance,
};

nk_register_shell_cmd(provenance);

/******************* Test Commands *******************/

static int handle_provtest(int argc, char** argv) {
	PRINT("In provenance test\n");
	PRINT("Argc: %d\n", argc);
	
	int result = 0;
	uint64_t addr;
	char opt;
	char* first_arg = argv[1];

	if (((opt = 'b', strcmp(first_arg, "bt"))==0) ||
		((opt = 'i', strcmp(first_arg, "info"))==0) ||
		((opt = 'q'))) {
		
		switch (opt) {
			case 'b':
				PRINT("Testing backtrace\n");
				backtrace_here();
				break;
			case 'i':
				PRINT("Testing info addr\n");
				if(argc != 3) {
					PRINT("Not enough arguments!\n");
					result = -1;
				} else {
					addr = atol(argv[2]);
					result = handle_prov_info(addr);
				}
				break;
			case 'q':
			default:
				PRINT("Invalid arguments passed!\n");
				result = -1;
				break;
		}
	}

	return result;
}

static struct nk_test_impl my_test_impl = {
	.name = "provtest",
	.handler = handle_provtest,
	.default_args = "bt",
};

nk_register_test(my_test_impl);
