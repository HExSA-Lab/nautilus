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

#include <nautilus/nautilus.h>
#include <nautilus/dev.h>
#include <nautilus/shell.h>
#include <dev/gpio.h>

#ifndef NAUT_CONFIG_DEBUG_GPIO
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("gpio: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("gpio: " fmt, ##args)
#define INFO(fmt, args...)  INFO_PRINT("gpio: " fmt, ##args)

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK(state) _state_lock_flags = spin_lock_irq_save(&((state)->lock))
#define STATE_UNLOCK(state) spin_unlock_irq_restore(&(((state)->lock)), _state_lock_flags)



struct gpio_state {
    struct nk_dev *dev;

    uint16_t port;

    spinlock_t lock;

    uint64_t cur_out;
    uint64_t cur_in;

    uint8_t  cpu_mask[GPIO_CPU_MASK_SIZE];
};

static struct gpio_state state;

// we can be called before per-cpu is available
// hence this wrapper
static inline int gpio_my_cpu_id()
{
    if (__cpu_state_get_cpu()) {
	return my_cpu_id();
    } else {
	return 0;
    }
}

void     nk_gpio_init()
{
    memset(&state,0,sizeof(state));
    state.port = NAUT_CONFIG_GPIO_PORT;
    spinlock_init(&state.lock);

    nk_gpio_cpu_mask_add(gpio_my_cpu_id());
    nk_gpio_output_set(0x0);
    nk_gpio_cpu_mask_remove(gpio_my_cpu_id());

    INFO("inited\n");
}
    
void     nk_gpio_deinit()
{
    if (!nk_gpio_cpu_mask_check(gpio_my_cpu_id())) { 
	nk_gpio_cpu_mask_add(gpio_my_cpu_id());
	nk_gpio_output_set(0x0);
	nk_gpio_cpu_mask_remove(gpio_my_cpu_id());
    } else {
	nk_gpio_output_set(0x0);
    }
    INFO("deinited\n");
}

void     nk_gpio_cpu_mask_set(uint8_t *mask)
{
    STATE_LOCK_CONF;

    STATE_LOCK(&state);

    memcpy(state.cpu_mask,mask,GPIO_CPU_MASK_SIZE);
    
    STATE_UNLOCK(&state);
}

void     nk_gpio_cpu_mask_get(uint8_t *mask)
{
    STATE_LOCK_CONF;

    STATE_LOCK(&state);

    memcpy(mask, state.cpu_mask,GPIO_CPU_MASK_SIZE);

    STATE_UNLOCK(&state);
}

void     nk_gpio_cpu_mask_add(uint64_t cpu_num)
{
    STATE_LOCK_CONF;

    STATE_LOCK(&state);

    state.cpu_mask[cpu_num/8] |= 0x1 << (cpu_num % 8);

    STATE_UNLOCK(&state);
}

void     nk_gpio_cpu_mask_remove(uint64_t cpu_num)
{
    STATE_LOCK_CONF;

    STATE_LOCK(&state);

    state.cpu_mask[cpu_num/8] &= ~(0x1 << (cpu_num % 8));

    STATE_UNLOCK(&state);
}

static int _nk_gpio_cpu_mask_check(uint64_t cpu_num)
{
    return (state.cpu_mask[cpu_num/8] >> (cpu_num % 8)) & 0x1;
}

int nk_gpio_cpu_mask_check(uint64_t cpu_num)
{
    int rc;

    STATE_LOCK_CONF;

    STATE_LOCK(&state);

    rc = _nk_gpio_cpu_mask_check(cpu_num);
    
    STATE_UNLOCK(&state);

    return rc;
}

void     nk_gpio_output_mask(uint64_t mask, gpio_mask_t op)
{
    STATE_LOCK_CONF;

    STATE_LOCK(&state);

    if (_nk_gpio_cpu_mask_check(gpio_my_cpu_id())) { 
	switch (op) { 
	case GPIO_OR:
	    state.cur_out |= mask;
	    break;
	case GPIO_AND:
	    state.cur_out &= mask;
	    break;
	case GPIO_XOR:
	    state.cur_out ^= mask;
	    break;
	case GPIO_SET:
	    state.cur_out = mask;
	    break;
	}
	
	DEBUG("mask %x op %s newval=%x\n", mask, 
	      op==GPIO_AND ? "and" :
	      op==GPIO_OR ? "or" :
	      op==GPIO_XOR ? "xor" :
	      op==GPIO_SET ? "set" : "UNKNOWN",
	      state.cur_out);

	outb(state.cur_out,state.port);
    } else {
	DEBUG("mask op ignored on cpu %d\n",gpio_my_cpu_id());
    }


    STATE_UNLOCK(&state);

}

uint64_t nk_gpio_input_get()
{
    ERROR("unimplemented\n");
    return -1ULL;
}

static int 
handle_gpio (char * buf, void * priv)
{
    char what[80];
    uint64_t val;
    uint64_t i;
    int set = nk_gpio_cpu_mask_check(my_cpu_id());

    if ((sscanf(buf,"gpio %s %lx", what, &val)==2) && what[0]=='s') {
        nk_vc_printf("sweeping gpio output from 0 to %lx with ~10 us delay\n",val);
        if (!set) {
            nk_gpio_cpu_mask_add(my_cpu_id());
        }
        for (i=0;i<val;i++) { 
            nk_gpio_output_set(i);
            udelay(10);
        }
        if (!set) {
            nk_gpio_cpu_mask_remove(my_cpu_id());
        }
        return 0;
    }

    if ((sscanf(buf,"gpio %s %lx", what, &val)==2) && what[0]=='o') {
        nk_vc_printf("setting gpio output to %lx\n",val);
        if (!set) {
            nk_gpio_cpu_mask_add(my_cpu_id());
        }
        nk_gpio_output_set(val);
        if (!set) {
            nk_gpio_cpu_mask_remove(my_cpu_id());
        }
        return 0;
    }

    if ((sscanf(buf,"gpio %s",what)==1) && what[0]=='i') {
        val = nk_gpio_input_get();
        nk_vc_printf("gpio input is %llx\n",val);
        return 0;
    }

    nk_vc_printf("unknown gpio request\n");

    return 0;
}


static struct shell_cmd_impl gpio_impl = {
    .cmd      = "gpio",
    .help_str = "gpio in/out/sweep [val]",
    .handler  = handle_gpio,
};
nk_register_shell_cmd(gpio_impl);
