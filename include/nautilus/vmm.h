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
 * Copyright (c) 2016, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __NK_VMM
#define __NK_VMM

void *nk_vmm_start_vm(char *name, void *image, unsigned int cpu_mask);
int   nk_vmm_stop_vm(void *vm);
int   nk_vmm_free_vm(void *vm);
int   nk_vmm_pause_vm(void *vm);
int   nk_vmm_continue_vm(void *vm);
int   nk_vmm_reset_vm(void *vm);
int   nk_vmm_move_vm_core(void *vm, int vcore, int pcore);
int   nk_vmm_move_vm_mem(void *vm, void *gpa, int pcore);


int nk_vmm_init();
int nk_vmm_deinit();

#endif
  
