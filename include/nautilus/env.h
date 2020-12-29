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

#ifndef __NK_ENV
#define __NK_ENV


#define NK_ENV_MAX_NAME 32

struct nk_env *nk_env_create(char *name,
			     uint64_t numkeys,
			     uint64_t maxkeysize,
			     uint64_t maxvaluesize);

struct nk_env *nk_env_find(char *name);

int            nk_env_destroy(struct nk_env *env);

// the following apply to the global environmnet
#define NK_ENV_GLOBAL_NAME         "global"
#define NK_ENV_GLOBAL_MAX_NUM_KEYS     64
#define NK_ENV_GLOBAL_MAX_KEY_SIZE    256
#define NK_ENV_GLOBAL_MAX_VAL_SIZE    256

// the global environment is constructed on first use
extern struct nk_env *nk_env_global;

int   nk_env_search(struct nk_env *env, char *key, char **value);
int   nk_env_insert(struct nk_env *env, char *key, char *val);
int   nk_env_update(struct nk_env *env, char *key, char *val);
int   nk_env_delete(struct nk_env *env, char *key);

// print list of environments, also their contents if indicated
void   nk_env_dump_envs(int contents);
// print contents of an environment
void   nk_env_dump_env(struct nk_env *env);

#endif
