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
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef _NK_OMP_
#define _NK_OMP_

// Run-time startup and shutdown - invoke in init
// after scheduling is active
int nk_openmp_init();
void nk_openmp_deinit();


// Promote the current thread into an OMP "main"
// thread.   You usually only need to do this
// once at the start of an OMP program.  Child threads
// created/used by an OMP program will automatically be
// OMP threads

int nk_openmp_thread_init();
int nk_openmp_thread_deinit();


//
// publicly visible functions in compliance with OMP standard
//
//
// Generated code also calls into RT-specific functions (e.g. GOMP_parallel)
// but these do not have prototypes in the header file
//

int omp_get_active_level();
int omp_get_ancestor_thread_num(int level);
int omp_get_cancellation();
int omp_get_default_device();
int omp_get_dynamic();
int omp_get_level(void);
int omp_get_max_active_levels(void);
int omp_get_max_task_priority(void);
int omp_get_max_threads(void);
int omp_get_nested(void);
int omp_get_num_devices(void);
int omp_get_num_procs(void);
int omp_get_num_teams(void);
int omp_get_num_threads(void);

typedef int omp_proc_bind_t;

omp_proc_bind_t omp_get_proc_bind(void);

typedef int * omp_sched_t;

void omp_get_schedule(omp_sched_t *kind, int *chunk_size);

int omp_get_team_num(void);
int omp_get_team_size(int level);
int omp_get_thread_limit(void);
int omp_get_thread_num(void);
int omp_in_parallel(void);
int omp_in_final(void);
int omp_is_initial_device(void);
void omp_set_default_device(int device_num);
void omp_set_dynamic(int dynamic_threads);
void omp_set_max_active_levels(int max_levels);
void omp_set_nested(int nested);
void omp_set_num_threads(int num_threads);
void omp_set_schedule(omp_sched_t kind, int chunk_size);

typedef void * omp_lock_t;

void omp_init_lock(omp_lock_t *lock);
void omp_set_lock(omp_lock_t *lock);
int  omp_test_lock(omp_lock_t *lock);
void omp_unset_lock(omp_lock_t *lock);
void omp_destroy_lock(omp_lock_t *lock);


typedef omp_lock_t omp_nest_lock_t;

void omp_init_nest_lock(omp_nest_lock_t *lock);
void omp_set_nest_lock(omp_nest_lock_t *lock);
int  omp_test_nest_lock(omp_nest_lock_t *lock);
void omp_unset_nest_lock(omp_nest_lock_t *lock);
void omp_destroy_nest_lock(omp_nest_lock_t *lock);

double omp_get_wtick(void);
double omp_get_wtime(void);

#endif
