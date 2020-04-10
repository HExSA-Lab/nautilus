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
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, Drew Kersnar <drewkersnar2021@u.northwestern.edu>
 * Copyright (c) 2020, The Interweaving Project <http://interweaving.org>
 *                     The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Peter Dinda <pdinda@northwestern.edu>
 *			Drew Kersnar <drewkersnar2021@u.northwestern.edu>
 *          
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/* General access routines for the debug registers */

// %dr0 through %dr3 are addresses to fault on

static inline uint64_t read_dr0(void)
{
    uint64_t ret;
    asm volatile ("mov %%dr0, %0" : "=r"(ret));
    return ret;
}


static inline void write_dr0(uint64_t data)
{
    asm volatile ("mov %0, %%dr0" : : "r"(data));
}

static inline uint64_t read_dr1(void)
{
    uint64_t ret;
    asm volatile ("mov %%dr1, %0" : "=r"(ret));
    return ret;
}


static inline void write_dr1(uint64_t data)
{
    asm volatile ("mov %0, %%dr1" : : "r"(data));
}

static inline uint64_t read_dr2(void)
{
    uint64_t ret;
    asm volatile ("mov %%dr2, %0" : "=r"(ret));
    return ret;
}


static inline void write_dr2(uint64_t data)
{
    asm volatile ("mov %0, %%dr2" : : "r"(data));
}

static inline uint64_t read_dr3(void)
{
    uint64_t ret;
    asm volatile ("mov %%dr3, %0" : "=r"(ret));
    return ret;
}


static inline void write_dr3(uint64_t data)
{
    asm volatile ("mov %0, %%dr3" : : "r"(data));
}

// there is no %dr4 and %dr5

// %dr6 is the status register

typedef union {
    uint64_t val;
    struct {
	uint8_t  dr0_detect:1;  // we are in bp due to dr0
	uint8_t  dr1_detect:1;
	uint8_t  dr2_detect:1;
	uint8_t  dr3_detect:1;
	uint16_t res1:8;        // reserved, read as 1s
	uint8_t  res2:1;        // reserved, read as 0s
	uint8_t  bp_debug_acc:1;// we are in bp due to touching a dr
	uint8_t  bp_single:1;   // we are in bp due to single-step mode
	uint8_t  bp_ts:1;       // we are in bp due to a task switch
	uint64_t res3:48;       // reserved must be zero
    } __packed;
} __packed dr6_t;


static inline uint64_t read_dr6(void)
{
    uint64_t ret;
    asm volatile ("mov %%dr6, %0" : "=r"(ret));
    return ret;
}


static inline void write_dr6(uint64_t data)
{
    asm volatile ("mov %0, %%dr6" : : "r"(data));
}

// %dr7 is the debug control register

typedef union {
    uint64_t val;
    struct {
	uint8_t local0:1;   // local enable dr0 bp (this task)
	uint8_t global0:1;  // global enable dr0 bp (any task)
	uint8_t local1:1; // enable both local and global or disable both
	uint8_t global1:1;
	uint8_t local2:1; 
	uint8_t global2:1;
	uint8_t local3:1; 
	uint8_t global3:1;
	uint8_t local_exact:1;  // local enable (this task) [ignored] always set to 1
	uint8_t global_exact:1; // global enable (any task) [ignored] always set to 1
	uint8_t res1:1;         // reserved read as 1
	uint8_t res2:2;         // reserved read as 0
	uint8_t general:1;      // breakpoint on accesses to drs // turn this on but probably irrelevant
	uint8_t res3:2;         // reserved read as 0
	uint8_t type0:2;        // type of BP for dr0 // important
	                        //    00 = INST FETCH
	                        //    01 = WRITE
	                        //    10 = IO READ|WRITE if CR4.DE
	                        //    11 = READ|WRITE
	uint8_t len0:2;         // length of BP for dr0 // important
	                        //    00 = 1 byte natural alignment
	                        //    01 = 2 byte natural alignment
	                        //    10 = 8 byte natural alignment
	                        //    11 = 4 byte natural alignment
	uint8_t type1:2;
	uint8_t len1:2;
	uint8_t type2:2;
	uint8_t len2:2;
	uint8_t type3:2;
	uint8_t len3:2;
	uint32_t res4:32;       // reserved, must be zero
    }__packed;
}__packed dr7_t;

	
static inline uint64_t read_dr7(void)
{
    uint64_t ret;
    asm volatile ("mov %%dr7, %0" : "=r"(ret));
    return ret;
}


static inline void write_dr7(uint64_t data)
{
    asm volatile ("mov %0, %%dr7" : : "r"(data));
}


// track a single address (for long word), looking for a read or write to it
static inline void set_watchpoint(uint64_t addr, int dreg_num)
{
	dr7_t t;
	switch (dreg_num) {
		case 0:
			t.val = read_dr7();
			t.local0 = 1;
			t.global0 = 1;
			t.type0 = 3;
			t.len0 = 2;

			write_dr0(addr);
			write_dr7(t.val);
			break;
		case 1:
			
			t.val = read_dr7();
			t.local1 = 1;
			t.global1 = 1;
			t.type1 = 3;
			t.len1 = 2;

			write_dr1(addr);
			write_dr7(t.val);
			break;
		case 2:
			t.val = read_dr7();
			t.local2 = 1;
			t.global2 = 1;
			t.type2 = 3;
			t.len2 = 2;

			write_dr2(addr);
			write_dr7(t.val);
			break;
		case 3:
			t.val = read_dr7();
			t.local3 = 1;
			t.global3 = 1;
			t.type3 = 3;
			t.len3 = 2;

			write_dr3(addr);
			write_dr7(t.val);
			break;
		default: 
			break;
	}
}

// track a single instruction address, looking for an instruction fetch
static inline void set_breakpoint(uint64_t addr, int dreg_num)
{

	dr7_t t;
	switch (dreg_num) {
		case 0:
			t.val = read_dr7();
			t.local0 = 1;
			t.global0 = 1;
			t.type0 = 0;
			t.len0 = 0;

			write_dr0(addr);
			write_dr7(t.val);
			break;
		case 1:
			
			t.val = read_dr7();
			t.local1 = 1;
			t.global1 = 1;
			t.type1 = 0;
			t.len1 = 0;

			write_dr1(addr);
			write_dr7(t.val);
			break;
		case 2:
			t.val = read_dr7();
			t.local2 = 1;
			t.global2 = 1;
			t.type2 = 0;
			t.len2 = 0;

			write_dr2(addr);
			write_dr7(t.val);
			break;
		case 3:
			t.val = read_dr7();
			t.local3 = 1;
			t.global3 = 1;
			t.type3 = 0;
			t.len3 = 0;

			write_dr3(addr);
			write_dr7(t.val);
			break;
		default: 
			break;
	}
}

// removes a breakpoint/watchpoint
static inline void disable(int dreg_num)
{
	dr7_t t;
	switch (dreg_num) {
		case 0:
			t.val = read_dr7();
			t.local0 = 0;
			t.global0 = 0;
			write_dr0(0); // delete address from register
			write_dr7(t.val); // disable register
			break;
		case 1:
			t.val = read_dr7();
			t.local1 = 0;
			t.global1 = 0;
			write_dr1(0); // delete address from register
			write_dr7(t.val); // disable register
			break;
		case 2:
			t.val = read_dr7();
			t.local2 = 0;
			t.global2 = 0;
			write_dr2(0); // delete address from register
			write_dr7(t.val); // disable register
			break;
		case 3:
			t.val = read_dr7();
			t.local3 = 0;
			t.global3 = 0;
			write_dr3(0); // delete address from register
			write_dr7(t.val); // disable register
			break;
		default: 
			break;
	}

}
    
