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

#include <nautilus/nautilus.h>
#include <nautilus/vmm.h>
#include <nautilus/shell.h>
#include "palacios.h"

struct v3_vm_info;

extern struct v3_vm_info * v3_create_vm(void * cfg, void * priv_data, char * name, unsigned int cpu_mask);
extern int v3_start_vm(struct v3_vm_info * vm, unsigned int cpu_mask);
extern int v3_stop_vm(struct v3_vm_info * vm);
extern int v3_pause_vm(struct v3_vm_info * vm);
extern int v3_continue_vm(struct v3_vm_info * vm);
extern int v3_reset_vm(struct v3_vm_info *vm);
typedef enum {V3_VM_RESET_ALL,V3_VM_RESET_HRT,V3_VM_RESET_ROS,V3_VM_RESET_CORE_RANGE} v3_vm_reset_type;
extern int v3_reset_vm_extended(struct v3_vm_info *vm, v3_vm_reset_type t, void *data);

extern int v3_save_vm(struct v3_vm_info * vm, char * store, char * url, unsigned long long opts);
extern int v3_load_vm(struct v3_vm_info * vm, char * store, char * url, unsigned long long opts);

extern int v3_send_vm(struct v3_vm_info * vm, char * store, char * url, unsigned long long opts);
extern int v3_receive_vm(struct v3_vm_info * vm, char * store, char * url, unsigned long long opts);

extern int v3_move_vm_core(struct v3_vm_info * vm, int vcore_id, int target_cpu);
extern int v3_move_vm_mem(struct v3_vm_info * vm, void *gpa, int target_cpu);

extern int v3_free_vm(struct v3_vm_info * vm);

extern int v3_get_state_sizes_vm(struct v3_vm_info        *vm, 
				 unsigned long long       *num_vcores, 
				 unsigned long long       *num_regions);

struct v3_vm_base_state;
struct v3_vm_core_state;
struct v3_vm_mem_state;

extern int v3_get_state_vm(struct v3_vm_info        *vm, 
			   struct v3_vm_base_state  *base,
			   struct v3_vm_core_state  *core,
			   struct v3_vm_mem_state   *mem);


void *nk_vmm_start_vm(char *name, void *image, unsigned int cpu_mask) 
{
 struct v3_vm_info *vm;

  palacios_inform_new_vm_pre(name);

  INFO_PRINT("Starting \"%s\" VM at %p\n",image);
  vm = v3_create_vm(image, 0, name, cpu_mask);
  
  if (vm) {
    palacios_inform_new_vm_post(name,vm);
    if (v3_start_vm(vm, cpu_mask)) {
      ERROR_PRINT("failed to start VM \"%s\"\n",name);
    } else {
      INFO_PRINT("successfully started VM \"%s\"\n",name);
    }
  } else {
    ERROR_PRINT("failed to create VM \"%s\"\n",name);
  }

  return vm;
}


int nk_vmm_stop_vm(void *vm)
{
  return v3_stop_vm((struct v3_vm_info*)vm);
}

int nk_vmm_free_vm(void *vm)
{
  return v3_free_vm((struct v3_vm_info*)vm);
}

int nk_vmm_pause_vm(void *vm)
{
  return v3_pause_vm((struct v3_vm_info*)vm);
}

int nk_vmm_continue_vm(void *vm)
{
  return v3_continue_vm((struct v3_vm_info*)vm);
}

int nk_vmm_reset_vm(void *vm)
{
  return v3_reset_vm((struct v3_vm_info*)vm);
}

int nk_vmm_move_vm_core(void *vm, int vcore, int pcore)
{
  return v3_move_vm_core((struct v3_vm_info*)vm,vcore,pcore);  
}

int nk_vmm_move_vm_mem(void *vm, void *gpa, int pcore)
{
  return v3_move_vm_mem((struct v3_vm_info*)vm,gpa,pcore);  
}


int nk_vmm_init()
{
  INFO_PRINT("VMM init\n");

  return palacios_vmm_init("none");
}

int nk_vmm_deinit()
{
  INFO_PRINT("VMM deinit\n");

  return palacios_vmm_exit();
}


static int
handle_vm (char * buf, void * priv)
{
    if (sscanf(buf,"vm %s", name)==1) { 
        extern int guest_start;
        nk_vmm_start_vm(name, &guest_start, 0xffffffff);
        return 0;
    }

    nk_vc_printf("Unknown VMM command\n");

    return 0;
}


static struct shell_cmd_impl vm_impl = {
    .cmd      = "vm",
    .help_str = "vm name [embedded image]",
    .handler  = handle_vm,
};
nk_register_shell_cmd(vm_impl);
  
