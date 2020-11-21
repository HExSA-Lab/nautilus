#ifndef __IPI_H__
#define __IPI_H__
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
 * Copyright (c) 2016, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/smp.h>
#include <nautilus/fs.h>
#include <nautilus/vc.h>

#define PING_VEC       0x42
#define PONG_VEC       0x32
#define PONG_BCAST_VEC 0x33


#define IPI_MAX_TRIALS 5000
#define IPI_NUM_CORES  64 //TODO: NIX

#define IPI_MAX_FNAME_LEN 128

typedef enum {
		EXP_ONEWAY,
		EXP_ROUNDTRIP,
		EXP_BROADCAST
} ipi_exp_type_t;

typedef enum {
		TYPE_ONE,
		TYPE_ALL,
        TYPE_NUM_TYPES,
} ipi_endpoint_t;


typedef struct ipi_exp_data {
	ipi_exp_type_t type;     // which type of experiment
	ipi_endpoint_t src_type; // is this one src core or all?
	ipi_endpoint_t dst_type; // target one or all cores? (oneway & roundtrip only)
	cpu_id_t       src_core; // which core is the src
	cpu_id_t       dst_core; // destination 
	uint32_t       trials;   // how many experimental trials

	nk_xcall_func_t measure_func; // the measurement function

    uint8_t use_file; // should we output data to a file?
    nk_fs_fd_t fd;    // the FD to use if we should
    char fname[IPI_MAX_FNAME_LEN]; // the filename to use

    struct nk_virtual_console * vc; // the virtual console on the originating core

    unsigned bitmap_len;                // how many longs
    volatile uint64_t * ack_bitmap;     // for bcast, remote cores set their corresponding bit on a recv
    volatile uint64_t * bcast_cntrs; // for bcast, the cycle count for the remote receiving cores

} ipi_exp_data_t;

int ipi_run_exps(ipi_exp_data_t * data);


#endif
