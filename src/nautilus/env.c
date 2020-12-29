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
 * Copyright (c) 2020, Peter Dinda
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/list.h>
#include <nautilus/env.h>



#if NAUT_CONFIG_DEBUG_ENVS
#define DEBUG(fmt, args...) DEBUG_PRINT("env: " fmt, ##args)
#else
#define DEBUG(fmt, args...)
#endif
#define INFO(fmt, args...) INFO_PRINT("env: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("env: " fmt, ##args)
#define WARN(fmt, args...)  WARN_PRINT("env: " fmt, ##args)


#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);

#define ENV_LOCK_CONF uint8_t _env_lock_flags
#define ENV_LOCK(env) _env_lock_flags = spin_lock_irq_save(&env->lock)
#define ENV_UNLOCK(env) spin_unlock_irq_restore(&env->lock, _env_lock_flags);


// the algorithm is stupid and can be fixed later if this
// is ever a performance issue
struct nk_env {
    spinlock_t lock;
    char       name[NK_ENV_MAX_NAME];
    struct list_head node;
    uint64_t num_keys;
    uint64_t key_size;
    uint64_t val_size;
    char     data[0];
};

static spinlock_t state_lock;
static struct list_head env_list = LIST_HEAD_INIT(env_list);


#define UNINITED_GLOBAL ((struct nk_env*)0xdeadb000b000b000UL)

// char [NUM_KEYS][MAX_KEY_SIZE+MAX_VAL_SIZE];
struct nk_env *nk_env_global = UNINITED_GLOBAL;

#define KEY(e,i) (env->data + ((env->key_size + env->val_size) * (i)))
#define VAL(e,i) (env->data + ((env->key_size + env->val_size) * (i) + env->key_size))

static inline struct nk_env *check_global(struct nk_env *env)
{
    if (env==UNINITED_GLOBAL) {
	nk_env_global = nk_env_create(NK_ENV_GLOBAL_NAME,
				      NK_ENV_GLOBAL_MAX_NUM_KEYS,
				      NK_ENV_GLOBAL_MAX_KEY_SIZE,
				      NK_ENV_GLOBAL_MAX_VAL_SIZE);
	if (!nk_env_global) {
	    ERROR("failed to allocate global environment\n");
	    return 0;
	} else {
	    DEBUG("created global environment\n");
	    return nk_env_global;
	}
    } else {
	return env;
    }
}


struct nk_env *nk_env_create(char *name,
			     uint64_t maxnumkeys,
			     uint64_t maxkeysize,
			     uint64_t maxvaluesize)
{
    struct nk_env *e;

    e = malloc(sizeof(struct nk_env) + maxnumkeys * (maxkeysize + maxvaluesize));

    if (!e) {
	ERROR("failed to allocate environment\n");
	return 0;
    }

    memset(e,0,sizeof(struct nk_env) + maxnumkeys * (maxkeysize + maxvaluesize));

    strncpy(e->name,name,NK_ENV_MAX_NAME); e->name[NK_ENV_MAX_NAME-1]=0;
    e->num_keys = maxnumkeys;
    e->key_size = maxkeysize;
    e->val_size = maxvaluesize;

    spinlock_init(&e->lock);
    INIT_LIST_HEAD(&e->node);

    STATE_LOCK_CONF;

    STATE_LOCK();
    list_add(&e->node,&env_list);
    STATE_UNLOCK();

    DEBUG("created environment %s\n",e->name);
						
    return e;
}


struct nk_env *nk_env_find(char *name)
{
    struct list_head *cur;
    struct nk_env *target=0;
    DEBUG("finding environment %s\n",name);
    STATE_LOCK_CONF;
    STATE_LOCK();
    list_for_each(cur,&env_list) {
	if (!strncasecmp(list_entry(cur,struct nk_env,node)->name,name,NK_ENV_MAX_NAME)) { 
	    target = list_entry(cur,struct nk_env, node);
	    break;
	}
    }
    STATE_UNLOCK();
    return target;
}


int nk_env_destroy(struct nk_env *env)
{
    if (env==nk_env_global) {
	ERROR("Cannot destroy global environment\n");
	return -1;
    }
    DEBUG("destroying environment %s\n",env->name);
    STATE_LOCK_CONF;
    STATE_LOCK();
    list_del_init(&env->node);
    STATE_UNLOCK();
    free(env);
    return 0;
}

// assumes lock is held
static int env_search(struct nk_env *env, char *key, char **keyptr, char **valptr)
{
    uint64_t i;

    for (i=0;i<env->num_keys;i++) {
	if (!strncmp(KEY(env,i),key,env->key_size)) {
	    *keyptr = KEY(env,i);
	    *valptr = VAL(env,i);
	    return 0;
	}
    }

    return -1;
}

// assumes lock is held
static int env_alloc(struct nk_env *env, char *key, char **keyptr, char **valptr)
{
    uint64_t i;

    for (i=0;i<env->num_keys;i++) {
	if (!(KEY(env,i)[0])) {
	    *keyptr = KEY(env,i);
	    *valptr = VAL(env,i);
	    return 0;
	}
    }

    return -1;
}
	    
int nk_env_search(struct nk_env *env, char *key, char **valptr)
{
    env = check_global(env);

    if (!env) {
	return -1;
    }

    char *keyptr;
    int rc;

    ENV_LOCK_CONF;

    ENV_LOCK(env);
    
    rc = env_search(env,key,&keyptr,valptr);

    ENV_UNLOCK(env);

    DEBUG("searched environment %s for %s %s %s\n",env->name,
	  key, !rc ? "found" : "", !rc ? *valptr : "");
    
    return rc;
}

int   nk_env_insert(struct nk_env *env, char *key, char *val)
{
    env = check_global(env);

    if (!env) {
	return -1;
    }

    char *keyptr;
    char *valptr;
    int rc;

    ENV_LOCK_CONF;

    ENV_LOCK(env);
    
    rc = env_search(env,key,&keyptr,&valptr);

    
    if (!rc) {
	ENV_UNLOCK(env);
	ERROR("attempting to overwrite key %s in %s\n",key,env->name);
	return -1;
    }

    if (env_alloc(env,key,&keyptr,&valptr)) {
	ENV_UNLOCK(env);
	ERROR("out of space for inserting key %s in %s\n",key,env->name);
	return -1;
    }

    strncpy(keyptr,key,env->key_size); keyptr[env->key_size-1]=0;
    strncpy(valptr,val,env->val_size); valptr[env->val_size-1]=0;

    ENV_UNLOCK(env);

    DEBUG("inserted (%s,%s) into %s\n", key,val,env->name);
    
    return 0;
}
    
int   nk_env_update(struct nk_env *env, char *key, char *val)
{
    env = check_global(env);

    if (!env) {
	return -1;
    }

    char *keyptr;
    char *valptr;
    int rc;

    ENV_LOCK_CONF;

    ENV_LOCK(env);
    
    rc = env_search(env,key,&keyptr,&valptr);

    
    if (rc) {
	ENV_UNLOCK(env);
	ERROR("attempting to update nonexistent key %s in %s\n",key, env->name);
	return -1;
    }
    
    strncpy(keyptr,key,env->key_size); keyptr[env->key_size-1]=0;
    strncpy(valptr,val,env->val_size); valptr[env->val_size-1]=0;

    ENV_UNLOCK(env);

    DEBUG("updated (%s,%s) on %s\n", key,val,env->name);
    
    return 0;
}
    

int  nk_env_delete(struct nk_env *env, char *key)
{
    env = check_global(env);

    if (!env) {
	return -1;
    }

    char *keyptr;
    char *valptr;
    int rc;

    ENV_LOCK_CONF;

    ENV_LOCK(env);
    
    rc = env_search(env,key,&keyptr,&valptr);

    
    if (rc) {
	ENV_UNLOCK(env);
	ERROR("attempting to delete nonexistent key %s in %s\n",key, env->name);
	return -1;
    }

    keyptr[0]=0;
    valptr[0]=0;

    ENV_UNLOCK(env);

    DEBUG("deleted %s from %s\n", key,env->name);
    
    return 0;
}


void nk_env_dump_env(struct nk_env *env)
{
    uint64_t i;
    ENV_LOCK_CONF;

    ENV_LOCK(env);
    
    for (i=0;i<env->num_keys;i++) {
	if (KEY(env,i)) {
	    nk_vc_printf("  %s = %s\n", KEY(env,i), VAL(env,i));
	}
    }

    ENV_UNLOCK(env);

}


void nk_env_dump_envs(int contents)
{
    struct list_head *cur;
    STATE_LOCK_CONF;
    STATE_LOCK();
    list_for_each(cur,&env_list) {
	struct nk_env *e = list_entry(cur,struct nk_env, node);
	if (contents) {
	    nk_vc_printf("%s contains:\n", e->name);
	    nk_env_dump_env(e);
	} else {
	    nk_vc_printf("%s\n", e->name);
	}
    }
    STATE_UNLOCK();
}


static int handle_envs (char * buf, void * priv)
{
    char temp[256];
    if (sscanf(buf,"envs %s",temp)==1) {
	nk_env_dump_envs(1);
	return 0;
    } else {
	nk_env_dump_envs(0);
	return 0;
    }
}


static struct shell_cmd_impl envs_impl = {
    .cmd      = "envs",
    .help_str = "envs [detail]",
    .handler  = handle_envs,
};
nk_register_shell_cmd(envs_impl);

static int handle_env (char * buf, void * priv)
{
    char temp[256];
    if (sscanf(buf,"env %s",temp)==1) {
	struct nk_env *e = nk_env_find(temp);
	if (!e) {
	    nk_vc_printf("cannot find environment %s\n",temp);
	    return 0;
	} else{
	    nk_env_dump_env(e);
	    return 0;
	}
    } else {
	nk_vc_printf("env name\n");
	return 0;
    }
}


static struct shell_cmd_impl env_impl = {
    .cmd      = "env",
    .help_str = "env name",
    .handler  = handle_env,
};
nk_register_shell_cmd(env_impl);

