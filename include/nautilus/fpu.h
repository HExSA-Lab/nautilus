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
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __FPU_H__
#define __FPU_H__

#ifdef __cplusplus
extern "C" {
#endif

#define FPU_BSP_INIT 0
#define FPU_AP_INIT  1

#define MXCSR_IE 1
#define MXCSR_DE 2
#define MXCSR_ZE (1<<2)
#define MXCSR_OE (1<<3)
#define MXCSR_UE (1<<4)
#define MXCSR_PE (1<<5)
#define MXCSR_DAZ (1<<6)
#define MXCSR_IM (1<<7)
#define MXCSR_DM (1<<8)
#define MXCSR_ZM (1<<9)
#define MXCSR_OM (1<<10)
#define MXCSR_UM (1<<11)
#define MXCSR_PM (1<<12)
#define MXCSR_RC (1<<13)
#define MXCSR_FZ (1<<14)

struct naut_info;

void fpu_init(struct naut_info *, int is_ap);

#ifdef __cplusplus
}
#endif

#endif /* !__FPU_H__! */
