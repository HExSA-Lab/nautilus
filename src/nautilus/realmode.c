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

#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/irq.h>
#include <nautilus/realmode.h>
#include <nautilus/shell.h>

#ifndef NAUT_CONFIG_DEBUG_REAL_MODE_INTERFACE
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("realmode: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("realmode: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("realmode: " fmt, ##args)

static spinlock_t state_lock;
static int in_progress = 0;


extern void _nk_real_mode_int(struct nk_real_mode_int_args *);
extern int _nk_real_mode_int_END;

void nk_real_mode_set_arg_defaults(struct nk_real_mode_int_args *args)
{
    memset(args,0,sizeof(*args));

    args->sp = 0x7000;
    args->bp = 0x7000;
    
    args->cs = NAUT_CONFIG_REAL_MODE_INTERFACE_SEGMENT;
    args->ds = NAUT_CONFIG_REAL_MODE_INTERFACE_SEGMENT;
    args->ss = NAUT_CONFIG_REAL_MODE_INTERFACE_SEGMENT;
    args->fs = NAUT_CONFIG_REAL_MODE_INTERFACE_SEGMENT;
    args->gs = NAUT_CONFIG_REAL_MODE_INTERFACE_SEGMENT;
    args->es = NAUT_CONFIG_REAL_MODE_INTERFACE_SEGMENT;
    
}

	   

int nk_real_mode_init()
{
    uint64_t size = (uint64_t)(&_nk_real_mode_int_END) -
	(uint64_t)&_nk_real_mode_int + 1;
    void *target = (void*)(NAUT_CONFIG_REAL_MODE_INTERFACE_SEGMENT * 16);

    spinlock_init(&state_lock);
    DEBUG("Copying thunk from %p to target address %p (size=0x%lx)\n",
	  &_nk_real_mode_int, target, size);
    memcpy(target,_nk_real_mode_int,size);
    INFO("real mode inited\n");
    return 0;
}

int nk_real_mode_deinit()
{
    INFO("real mode deinited\n");
    return 0;
}
int nk_real_mode_start()
{
    // spin until we get the lock, but leave interrupts on
    DEBUG("wait for start\n");
    spin_lock(&state_lock);
    in_progress = 1;
    DEBUG("started\n");
    return 0;
}

int nk_real_mode_int(struct nk_real_mode_int_args *args)
{
    if (!in_progress) {
	ERROR("Invocation of nk_real_mode_int before nk_real_mode_start\n");
	return -1;
    }

    // now disable interrupts
    uint8_t flags = irq_disable_save();
    
    DEBUG("Interrupts off, thunk about to be called\n");

    // invoke the thunk
    void (*thunk)(struct nk_real_mode_int_args *) = 
	(void (*)(struct nk_real_mode_int_args *))(NAUT_CONFIG_REAL_MODE_INTERFACE_SEGMENT*16);

    thunk(args);

    DEBUG("Thunk has returned\n");

    // reenable interrupts

    irq_enable_restore(flags);

    return 0;

}


int nk_real_mode_finish()
{
    DEBUG("finish called\n");

    if (!in_progress) { 
	ERROR("Calling nk_real_mode_finish when no invocation is in progress\n");
	return -1;
    }

    in_progress = 0;

    spin_unlock(&state_lock);

    return 0;
}


static int 
handle_real (char * cmd, void * priv)
{
    struct nk_real_mode_int_args test;

    if ((nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx %hx %hx %hx:%hx", 
		&test.vector, &test.ax, &test.bx, &test.cx, &test.cx, &test.es, &test.di)==7) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx %hx %hx:%hx", 
		&test.vector, &test.ax, &test.bx, &test.cx, &test.es, &test.di)==6) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx %hx:%hx", 
		&test.vector, &test.ax, &test.bx, &test.es, &test.di)==5) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx:%hx", 
		&test.vector, &test.ax, &test.es, &test.di)==4) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx:%hx", 
		&test.vector, &test.ax, &test.es, &test.di)==3) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx %hx %hx", 
		&test.vector, &test.ax, &test.bx, &test.cx, &test.dx)==5) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx %hx", 
		&test.vector, &test.ax, &test.bx, &test.cx)==4) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx", 
		&test.vector, &test.ax, &test.bx)==3) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx", 
		&test.vector, &test.ax)==2) ||	
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx",
		&test.vector)==1)) {

            nk_vc_printf("Req: int %hx ax=%04hx bx=%04hx cx=%04hx dx=%04hx es:di=%04hx:%04hx\n",
                    test.vector, test.ax, test.bx, test.cx, test.dx, test.es, test.di);

            if (nk_real_mode_start()) { 
                nk_vc_printf("start failed\n");
                return -1;
            } else {
                if (nk_real_mode_int(&test)) { 
                    nk_vc_printf("int failed\n");
                    nk_real_mode_finish();
                    return -1;
                } else {
                    nk_vc_printf("Res: ax=%04hx bx=%04hx cx=%04hx dx=%04hx si=%04hx di=%04hx\n"
                            "     flags=%04hx cs=%04hx ds=%04hx ss=%04hx fs=%04hx gs=%04hx es=%04hx\n",
                            test.ax, test.bx, test.cx, test.dx, test.si, test.di,
                            test.flags, test.cs, test.ds, test.ss, test.fs, test.gs, test.es);
                    nk_real_mode_finish();
                    return 0;
                }
            }

    } else {
        nk_vc_printf("Don't understand %s\n",cmd);
        return -1;
    }
}


static struct shell_cmd_impl real_impl = {
    .cmd      = "real",
    .help_str = "real int [ax [bx [cx [dx]]]] [es:di]",
    .handler  = handle_real,
};
nk_register_shell_cmd(real_impl);
