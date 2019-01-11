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
 * Copyright (c) 2016, Peter Dinda
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __NK_REAL_MODE
#define __NK_REAL_MODE


/*
  A real mode interrupt (e.g. bios call, vesa call, etc)
  executes as follows:

  There is a single context for making real mode interrupts 
  across the entire machine.   Only one call is possible at a time.

  That context exists within the segment 
  NAUT_CONFIG_REAL_MODE_INTERFACE_SEGMENT, by default set to 0x8000
  (long mode physical addresses 0x80000 to 0x8ffff)
  
  This 64K chunk is laid out as follows:

  ffff      
  ...     Available for your use
  8000
  ...     Used internally
  7000
  ...     real mode stack
  1000
  ...     thunk code and data
  0000

  A real mode interrupt request structure (see below) therefore
  consists only of the 8 real mode GPRs.  Furthermore, unless you
  know what you are doing, SP and BP should be set to 0x7000.   

 */

struct nk_real_mode_int_args {
    // DO NOT CHANGE OR ADD ANYTHING TO THIS UNLESS YOU
    // MAKE RELEVANT CHANGES TO realmode.S

    // (vector+pad) + 8 GPRS + 6 SRs + 1 Flags = 16 words = 32 bytes = 4 quads
    uint8_t  vector;
    uint8_t  blank;
    uint16_t ax;
    uint16_t bx;
    uint16_t cx;
    uint16_t dx;
    uint16_t si;
    uint16_t di;
    uint16_t sp;    // Accept defaults for these
    uint16_t bp;    // unless you know what you are doing
    uint16_t cs;    //  output only
    uint16_t ds;    //
    uint16_t ss;    // 
    uint16_t fs;    //
    uint16_t gs;    //
    uint16_t es;    //
    uint16_t flags; //  output only
} __packed __attribute__((aligned(4))) ;

int nk_real_mode_init();
int nk_real_mode_deinit();

// Use this to initialize the arguments structure unless
// you know what you are doing
void nk_real_mode_set_arg_defaults(struct nk_real_mode_int_args *args);

// call this when you want to prepare to use RM
// it will spin with interrupts on until this is possible
int nk_real_mode_start();
// call this when you want to invoke the interrupt
// this will also disable interrupts on the core
// until it returns
int nk_real_mode_int(struct nk_real_mode_int_args *args);
// call this when after it returns and when you are done accessing
// any memory in the context
// this will release any other real_mode_starts()
int nk_real_mode_finish();

#endif
