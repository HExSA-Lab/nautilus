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
#ifndef __INSTRUMENT_H__
#define __INSTRUMENT_H__


#ifdef __cplusplus 
extern "C" {
#endif

#define INSTR_CAL_LOOPS 1000

#ifdef NAUT_CONFIG_PROFILE
#define NK_PROFILE_ENTRY() nk_profile_func_enter(__func__)
#define NK_PROFILE_ENTRY_NAME(s) nk_profile_func_enter(#s)
#define NK_PROFILE_EXIT_NAME(s) nk_profile_func_exit(#s)
#define NK_PROFILE_EXIT() nk_profile_func_exit(__func__)
#define NK_MALLOC_PROF_ENTRY() nk_malloc_enter()
#define NK_MALLOC_PROF_EXIT() nk_malloc_exit()
#define NK_FREE_PROF_ENTRY() nk_free_enter()
#define NK_FREE_PROF_EXIT() nk_free_exit()
#else
#define NK_PROFILE_ENTRY() 
#define NK_PROFILE_EXIT()
#define NK_PROFILE_ENTRY_NAME(s)
#define NK_PROFILE_EXIT_NAME(s)
#define NK_MALLOC_PROF_ENTRY()
#define NK_MALLOC_PROF_EXIT()
#define NK_FREE_PROF_ENTRY() 
#define NK_FREE_PROF_EXIT() 
#endif

struct nk_hashtable;

struct malloc_data {
    uint64_t count;
    uint64_t start_count;
    uint64_t avg_latency;
    uint64_t max_latency;
    uint64_t min_latency;
};

struct free_data {
    uint64_t count;
    uint64_t start_count;
    uint64_t avg_latency;
    uint64_t max_latency;
    uint64_t min_latency;
};

struct irq_data {
    uint64_t count;
    uint64_t start_count;
    uint64_t avg_latency;
    uint64_t max_latency;
    uint64_t min_latency;
};

struct thread_switch_data {
    uint64_t count;
    uint64_t start_count;
    uint64_t avg_latency;
    uint64_t max_latency;
    uint64_t min_latency;
};

struct nk_instr_data {
    struct nk_hashtable * func_htable;
    struct irq_data irqstat;
    struct malloc_data mallocstat;
    struct free_data freestat;
    struct thread_switch_data thr_switch;
};


void nk_profile_func_enter(const char * func);
void nk_profile_func_exit(const char * func);
void nk_thr_switch_prof_enter(void);
void nk_thr_switch_prof_exit(void);
void nk_irq_prof_enter(void);
void nk_irq_prof_exit(void);

void nk_malloc_enter(void);
void nk_malloc_exit(void);
void nk_free_enter(void);
void nk_free_exit(void);
void nk_instrument_init(void);
void nk_instrument_start(void);
void nk_instrument_end(void);
void nk_instrument_query(void);
void nk_instrument_clear(void);
void nk_instrument_calibrate(unsigned loops);


#ifdef __cplusplus
}
#endif


#endif
