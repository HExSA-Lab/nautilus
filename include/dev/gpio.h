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
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Peter Dinda <pdinda@northwestern.edu>
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __GPIO
#define __GPIO

typedef enum {GPIO_OR=0, GPIO_AND, GPIO_XOR, GPIO_SET} gpio_mask_t;

void     nk_gpio_init();
void     nk_gpio_deinit();

#define GPIO_CPU_MASK_SIZE (((NAUT_CONFIG_MAX_CPUS)/(8)) + !!((NAUT_CONFIG_MAX_CPUS)%(8)))

// select which cpus will be able to manipulate the
// output mask - mask is GPIO_CPU_MASK_SIZE bytes long
// bit high => corresponding cpu can manipulate GPIO
// at init, none can
void     nk_gpio_cpu_mask_set(uint8_t *mask);
void     nk_gpio_cpu_mask_get(uint8_t *mask);
void     nk_gpio_cpu_mask_add(uint64_t cpu_num);
void     nk_gpio_cpu_mask_remove(uint64_t cpu_num);
int      nk_gpio_cpu_mask_check(uint64_t cpu_num);

void     nk_gpio_output_mask(uint64_t mask, gpio_mask_t op);

#define nk_gpio_output_set(x) nk_gpio_output_mask(x,GPIO_SET)

uint64_t nk_gpio_input_get();

// Use the following macros to instrument code conditional on
// whether the GPIO device is available - zero overhead if it is not compiled in
#ifdef NAUT_CONFIG_GPIO
#define NK_GPIO_OUTPUT_MASK(m,o) nk_gpio_output_mask(m,o)
#define NK_GPIO_OUTPUT_SET(x) nk_gpio_output_set(x)
#define NK_GPIO_INPUT_GET() nk_gpio_input_get()
#else
#define NK_GPIO_OUTPUT_MASK(m,o) 
#define NK_GPIO_OUTPUT_SET(x) 
#define NK_GPIO_INPUT_GET() (0ULL)
#endif


#endif

