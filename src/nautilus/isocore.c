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
 * Copyright (c) 2017, Peter Dinda <pdinda@northwestern.edu>
 *                     Adel Lahlou <AdelLahlou2017@u.northwestern.edu>
 *                     Fahad Khan <FahadKhan2017@u.northwestern.edu>
 *                     David Samuels <davidSamuels2018@u.northwestern.edu>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Peter Dinda <pdinda@northwestern.edu>
 *	    Adel Lahlou <AdelLahlou2017@u.northwestern.edu>
 *          Fahad Khan <FahadKhan2017@u.northwestern.edu>
 *          David Samuels <davidSamuels2018@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/shell.h>

#ifndef NAUT_CONFIG_DEBUG_ISOCORE
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("isocore: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("isocore: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("isocore: " fmt, ##args)

#define FLOOR_DIV(x,y) ((x)/(y))
#define CEIL_DIV(x,y)  (((x)/(y)) + !!((x)%(y)))
#define DIVIDES(x,y) (((x)%(y))==0)
#define MAX(x,y) ((x)>(y) ? (x) : (y))
#define MIN(x,y) ((x)<(y) ? (x) : (y))

int nk_isolate(void (*code)(void *arg),
	       uint64_t codesize,
	       uint64_t stacksize,
	       void     *arg)
{

    DEBUG("nk_isolate(code=%p, codesize=%lu\n",code, codesize);
    DEBUG("           stacksize=%lu, arg=%p\n", stacksize, arg);

    // build a code+stack segment that looks like this:
    //
    // CODE
    // ----- <- page boundary
    // STACK
    //
    // both CODE and STACK are an integral number of 
    // pages long
    //
    // Note that we don't really need page alignment for
    // this - I'm just doing it for now to make sure
    // we have cache line alignment for everything, regardless of machine

    uint64_t code_pages = CEIL_DIV(codesize,PAGE_SIZE_4KB);
    uint64_t stack_pages = CEIL_DIV(stacksize,PAGE_SIZE_4KB);
    uint64_t total_pages = code_pages+stack_pages;

    DEBUG("Allocating %lu code pages and %lu stack pages\n",
	  code_pages,stack_pages);

    // malloc will align to next power of 2 pages...
    void *capsule = malloc(total_pages*PAGE_SIZE_4KB);
    
    if (!capsule) { 
	ERROR("Unable to allocate capsule\n");
	return -1;
    }

    DEBUG("Capsule allocated at %p\n",capsule);

    // clear code and stack of the capsule
    memset(capsule,0,total_pages*PAGE_SIZE_4KB);

    // copy the code into the capsule
    memcpy(capsule+stack_pages*PAGE_SIZE_4KB,
	   code,
	   codesize);

    //nk_dump_mem(capsule+stack_pages*PAGE_SIZE_4KB, codesize);

    // now transfer to the low-level code to
    // effect isolation

    extern int _nk_isolate_entry(void *,   // where capsule begins
				 uint64_t, // size of capsule
				 void *,   // entry point/stack start
				 void *);  // what goes into rdi

    DEBUG("Launching low-level capsule code, capsule=%p, size=%lu, entry=%p, rdi=%p\n", capsule, total_pages*PAGE_SIZE_4KB, capsule+stack_pages*PAGE_SIZE_4KB, arg);

    _nk_isolate_entry(capsule,
		      total_pages*PAGE_SIZE_4KB, 
		      capsule+stack_pages*PAGE_SIZE_4KB, 
		      arg);

    // this should never return
    
    ERROR("The impossible has happened - _nk_isolate_entry returned!\n");
    return -1;
}
    

static void 
isotest (void *arg)
{
    // note trying to do anything in here with NK
    // features, even a print, is unlikely to work due to
    // relocation, interrupts off, etc.   
    //serial_print("Hello from isocore, my arg is %p\n", arg);
    serial_putchar('H');
    serial_putchar('I');
    serial_putchar('!');
    serial_putchar('\n');
    while (1) { }  // does actually get here in testing
}

    
static int
handle_iso (char * buf, void * priv)
{
    void (*code)(void*) = isotest;
    uint64_t codesize   = PAGE_SIZE_4KB; // we are making pretend here
    uint64_t stacksize  = PAGE_SIZE_4KB;
    void *arg           = (void*)0xdeadbeef;

    nk_vc_printf("Testing isolated core - this will not return!\n");

    return nk_isolate(code, 
                      codesize,
                      stacksize,
                      arg);
    return 0;
}


static struct shell_cmd_impl iso_impl = {
    .cmd      = "iso",
    .help_str = "iso",
    .handler  = handle_iso,
};
nk_register_shell_cmd(iso_impl);
