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
 * Copyright (c) 2019, Peter Dinda
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
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
#include <nautilus/paging.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>
#include <nautilus/idt.h>

#include <nautilus/aspace.h>

#ifndef NAUT_CONFIG_DEBUG_ASPACES
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("aspace: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("aspace: " fmt, ##args)
#define INFO(fmt, args...)   INFO_PRINT("aspace: " fmt, ##args)



static spinlock_t state_lock;
static struct list_head aspace_list;

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);


// this is to be
nk_aspace_t *nk_aspace_register(char *name, uint64_t flags, nk_aspace_interface_t *interface, void *state)
{
    nk_aspace_t *a;

    a = malloc(sizeof(*a));

    if (!a) {
	ERROR("cannot allocate aspace\n");
	return 0;
    }

    memset(a,0,sizeof(*a));

    a->flags = flags;
    strncpy(a->name,name,NK_ASPACE_NAME_LEN); a->name[NK_ASPACE_NAME_LEN-1]=0;
    a->state = state;
    a->interface = interface;

    INIT_LIST_HEAD(&a->aspace_list_node);

    STATE_LOCK_CONF;
    
    STATE_LOCK();
    list_add_tail(&a->aspace_list_node, &aspace_list);
    STATE_UNLOCK();

    return a;
}


int nk_aspace_unregister(nk_aspace_t *a)
{
    STATE_LOCK_CONF;
    
    STATE_LOCK();
    list_del_init(&a->aspace_list_node);
    STATE_UNLOCK();

    free(a);
    
    return 0;
}

static nk_aspace_impl_t *find_impl(char *impl_name)
{
    extern nk_aspace_impl_t *__start_aspace_impls;
    extern nk_aspace_impl_t * __stop_aspace_impls;
    nk_aspace_impl_t **cur;

    for (cur = &__start_aspace_impls;
	 cur && cur!=&__stop_aspace_impls && *cur;
	 cur++) {
	if (!strcmp((*cur)->impl_name,impl_name)) {
	    return *cur;
	}
    }
    return 0;
}


int nk_aspace_query(char *impl_name, nk_aspace_characteristics_t *chars)
{
    nk_aspace_impl_t *impl = find_impl(impl_name);

    if (impl && impl->get_characteristics) {
	return impl->get_characteristics(chars);
    } else {
	return -1;
    }
}



nk_aspace_t *nk_aspace_create(char *impl_name, char *name, nk_aspace_characteristics_t *chars)
{
    nk_aspace_impl_t *impl = find_impl(impl_name);

    if (impl && impl->create) {
	return impl->create(name, chars);
    } else {
	return 0;
    }
}

#define BOILERPLATE_LEAVE(a,f,args...)                 \
    if (a->interface && a->interface->f) {             \
       return a->interface->f(a->state, ##args);       \
    } else {                                           \
       return -1;                                      \
    }

#define BOILERPLATE_DO(a,f,args...)		  \
    if (a->interface && a->interface->f) {        \
       a->interface->f(a->state, ##args);         \
    } 

#define AS_NAME(a) ((a) ? (a)->name : "default")

int  nk_aspace_destroy(nk_aspace_t *aspace)
{
    BOILERPLATE_LEAVE(aspace,destroy)
}
    
nk_aspace_t *nk_aspace_find(char *name)
{
    struct list_head *cur;
    nk_aspace_t  *target=0;
    
    STATE_LOCK_CONF;
    STATE_LOCK();
    list_for_each(cur,&aspace_list) {
	if (!strcmp(list_entry(cur,struct nk_aspace,aspace_list_node)->name,name)) { 
	    target = list_entry(cur,struct nk_aspace, aspace_list_node);
	    break;
	}
    }
    STATE_UNLOCK();
    DEBUG("search for %s finds %p\n",name,target);
    return target;
}

// switch address space assuming we have interrupts off
static int _nk_aspace_switch(nk_aspace_t *next)
{
    int rc=-1;
    struct cpu *cpu = get_cpu();
    nk_aspace_t *cur = cpu->cur_aspace;

    DEBUG("cpu %d switching from address space %s to %s\n",cpu->id,
	  AS_NAME(cur),AS_NAME(next));

    if (next==cur) {
	// do nothing
	DEBUG("aspaces are the same - ignoring switch\n");
	return 0;
    } else {
	if (cur) {
	    DEBUG("switching away from current address space\n");
	    cur->interface->switch_from(cur->state);
	} else {
	    DEBUG("switching away from default address space\n");
	    // switching from default address space does nothing
	}
	if (next) {
	    DEBUG("switching to next address space\n");
	    next->interface->switch_to(next->state);
	    cur = next;
	} else {
	    DEBUG("switching to default address space\n");
	    write_cr3(nk_paging_default_cr3());
	}
	DEBUG("address space switch complete\n");
	cpu->cur_aspace = next;
	return 0;
    }
}

int nk_aspace_switch(nk_aspace_t *next)
{
    uint8_t flags = irq_disable_save();
    int rc;

    rc = _nk_aspace_switch(next);

    irq_enable_restore(flags);

    return rc;
}

int nk_aspace_move_thread(nk_aspace_t *aspace)
{

    nk_thread_t *t = get_cur_thread();
    uint8_t flags;

    // dangerous if interrupts are off
    DEBUG("moving thread %d (%s) from %p (%s) to %p (%s)\n",t->tid,t->name,
	  t->aspace, AS_NAME(t->aspace), aspace, AS_NAME(aspace));
	

    flags = irq_disable_save();

    // we are now going to keep running as long as needed
    // and we are unstealable
    
    // old address space is losing thread
    if (t->aspace) { 
	BOILERPLATE_DO(t->aspace,remove_thread);
    }

    // new address space is gaining it
    BOILERPLATE_DO(aspace,add_thread);

    
    DEBUG("Doing switch to %p\n",aspace);

    // now actually do the switch

    _nk_aspace_switch(aspace);

    t->aspace = aspace;

    DEBUG("thread %d (%s) is now in %p (%s)\n",t->tid,t->name, t->aspace,t->aspace->name);

    irq_enable_restore(flags);
    
    return 0;
}

int  nk_aspace_add_region(nk_aspace_t *aspace, nk_aspace_region_t *region)
{
    BOILERPLATE_LEAVE(aspace,add_region,region);
}

int  nk_aspace_remove_region(nk_aspace_t *aspace, nk_aspace_region_t *region)
{
    BOILERPLATE_LEAVE(aspace,remove_region,region);
}

int  nk_aspace_protect_region(nk_aspace_t *aspace, nk_aspace_region_t *region, nk_aspace_protection_t *prot)
{
    BOILERPLATE_LEAVE(aspace,protect_region,region,prot);
}


int  nk_aspace_move_region(nk_aspace_t *aspace, nk_aspace_region_t *cur_region, nk_aspace_region_t *new_region)
{
    BOILERPLATE_LEAVE(aspace,move_region,cur_region,new_region);
}



int nk_aspace_exception(excp_entry_t *entry, excp_vec_t vec, void *priv_data)
{
    struct cpu *cpu  = get_cpu();
    nk_aspace_t *cur = cpu->cur_aspace;

    if (vec==PF_EXCP) {
	if (cur->flags & NK_ASPACE_HOOK_PF) {
	    return cur->interface->exception(cur->state,entry,vec);
	} else {
	    // cannot handle
	    return -1;
	}
    }

    if (vec==GP_EXCP) {
	if (cur->flags & NK_ASPACE_HOOK_GPF) {
	    return cur->interface->exception(cur->state, entry, vec);
	} else {
	    // cannot handle
	    return -1;
	}
    }

    return -1;
}
    

int nk_aspace_init()
{

    int nk_aspace_base_init();

    INIT_LIST_HEAD(&aspace_list);
    spinlock_init(&state_lock);

    nk_aspace_base_init();

    return 0;
}

int nk_aspace_init_ap()
{
    INFO("inited\n");
    return 0;
}


int nk_aspace_dump_aspaces(int detail)
{
    struct list_head *cur;
    STATE_LOCK_CONF;
    STATE_LOCK();
    list_for_each(cur,&aspace_list) {
	struct nk_aspace *a = list_entry(cur,struct nk_aspace, aspace_list_node);
	BOILERPLATE_DO(a,print,detail);
    }
    STATE_UNLOCK();
    return 0;
}


static int handle_ases (char * buf, void * priv)
{
    int detail = 0;
    if (strstr(buf,"d")) {
	detail = 1;
    }
    nk_aspace_dump_aspaces(detail);
    return 0;
}


static struct shell_cmd_impl ases_impl = {
    .cmd      = "ases",
    .help_str = "ases [detail]",
    .handler  = handle_ases,
};
nk_register_shell_cmd(ases_impl);

int nk_aspace_dump_aspace_impls()
{
    extern nk_aspace_impl_t *__start_aspace_impls;
    extern nk_aspace_impl_t * __stop_aspace_impls;
    nk_aspace_impl_t **cur;

    for (cur = &__start_aspace_impls;
	 cur && cur!=&__stop_aspace_impls && *cur;
	 cur++) {
	nk_vc_printf("%s\n",(*cur)->impl_name);
    }
    return 0;
}


static int handle_asis (char * buf, void * priv)
{
    nk_aspace_dump_aspace_impls();
    return 0;
}


static struct shell_cmd_impl asis_impl = {
    .cmd      = "asis",
    .help_str = "asis",
    .handler  = handle_asis,
};
nk_register_shell_cmd(asis_impl);
