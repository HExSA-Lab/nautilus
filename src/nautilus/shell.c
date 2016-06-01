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
#include <nautilus/shell.h>
#include <nautilus/vc.h>
#ifdef NAUT_CONFIG_PALACIOS
#include <nautilus/vmm.h>
#endif


static int handle_cmd(char *buf, int n)
{
  char name[n];

  if (*buf==0) { 
    return 0;
  }

  if (!strncasecmp(buf,"exit",4)) { 
    return 1;
  }
  
  if (!strncasecmp(buf,"help",4)) { 
    nk_vc_printf("help\nexit\nvcs\n");
    nk_vc_printf("shell name\n");
    nk_vc_printf("vm name [embedded image]\n");
    return 0;
  }

  if (!strncasecmp(buf,"vcs",3)) {
    nk_switch_to_vc_list();
    return 0;
  }

  if (sscanf(buf,"shell %s", name)==1) { 
    nk_launch_shell(name,-1);
    return 0;
  }

#ifdef NAUT_CONFIG_PALACIOS_EMBEDDED_VM_IMG
  if (sscanf(buf,"vm %s", name)==1) { 
    extern int guest_start;
    nk_vmm_start_vm(name,&guest_start,0xffffffff);
    return 0;
  }
#endif

  nk_vc_printf("Don't understand \"%s\"\n",buf);
  return 0;
}

static void shell(void *in, void **out)
{
  struct nk_virtual_console *vc = nk_create_vc((char*)in,COOKED, 0x9f, 0, 0);
  char buf[80];

  if (!vc) { 
    ERROR_PRINT("Cannot create virtual console for shell\n");
    return;
  }
  
  if (nk_bind_vc(get_cur_thread(), vc)) { 
    ERROR_PRINT("Cannot bind virtual console for shell\n");
    return;
  }
   
  nk_switch_to_vc(vc);
  
  nk_vc_clear(0x9f);
   
  while (1) {  
    nk_vc_printf("%s> ", (char*)in);
    nk_vc_gets(buf,80,1);

    if (handle_cmd(buf,80)) { 
      break;
    }
  }

  nk_vc_printf("Exiting shell %s\n", (char*)in);
  free(in);
  nk_release_vc(get_cur_thread());

  // exit thread
  
}

nk_thread_id_t nk_launch_shell(char *name, int cpu)
{
  nk_thread_id_t tid;
  char *n = malloc(32);

  if (!n) {
    return 0;
  }

  strncpy(n,name,32);
  n[31]=0;
  
  nk_thread_start(shell, (void*)n, 0, 0, PAGE_SIZE, &tid, cpu);
  
  INFO_PRINT("Shell %s launched on cpu %d as %p\n",name,cpu,tid);

  return tid;
}



