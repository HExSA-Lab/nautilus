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
 * Copyright (c) 2018, Peter Dinda
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */


#ifndef __NK_MTRR
#define __NK_MTRR

#define NK_MTRR_HAS_FIXED  1
#define NK_MTRR_HAS_WC     2
#define NK_MTRR_HAS_SMRR   4

// memory types
#define NK_MTRR_TYPE_UC 0x0
#define NK_MTRR_TYPE_WC 0x1
#define NK_MTRR_TYPE_WT 0x4
#define NK_MTRR_TYPE_WP 0x5
#define NK_MTRR_TYPE_WB 0x6


// check if feature exists, and get flags, number of variable mttrs available
int nk_mttr_get_features(int *num_var, int *flags);

int nk_mtrr_get_default(int *enable, int *fixed_enable, uint8_t *type);
int nk_mtrr_set_default(int enable, int fixed_enable, uint8_t type);


int nk_mtrr_addr_to_fixed_num(void *addr);
int nk_mtrr_fixed_num_to_range(int num, void **start, void **end);

int nk_mtrr_get_fixed(int num, uint8_t *type);
int nk_mtrr_set_fixed(int num, uint8_t type);

// num = -1 => SMRR, >=0 => variable region
int nk_mtrr_get_variable(int num, void **start, void **end, uint8_t *type, int *valid); 
int nk_mtrr_set_variable(int num, void *start, void *end, uint8_t type, int valid);

int nk_mtrr_find_type(void *addr, uint8_t *type, char **typestr);

void nk_mtrr_dump(int cpu);

// these will attempt to find and correct MTRR discrepencies at boot
// time.

// called by BSP at boot
int nk_mtrr_init();
// called by every AP at boot
int nk_mtrr_init_ap();

void nk_mtrr_deinit(); 

#endif
