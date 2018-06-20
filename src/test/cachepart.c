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
 * Copyright (c) 2018, Vyas Alwar
 * Copyright (c) 2018, Qingtong Guo
 * Copyright (c) 2018, Peter Dinda
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Vyas Alwar <valwar@math.northwestern.edu>
 *          Qingtong Guo <QingtongGuo2019@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

//
// Created by gqt on 5/22/18.
//
#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/vc.h>
#include <nautilus/cachepart.h>
#include <nautilus/spinlock.h>
#include <test/cachepart.h>

#define LARGE_NUM 10000000

#define MAXITS 20

volatile static int num_ready_threads;
volatile static int num_finished_threads;

struct cache_part_input {
  uint64_t size;
  uint64_t iteration;
  int percent;
  int init;
  int cos;
};

struct cache_part_output {
  uint64_t sum;
  uint64_t timer;
};


#define IA32_L2_QOS_MASK_0            0x00000D10
#define IA32_L3_QOS_MASK_0            0x00000C90
#define IA32_L3_QOS_MASK(i) ((IA32_L3_QOS_MASK_0) + (i))
#define IA32_PQR_ASSOC                0x00000C8F

static void get_msr_info()
{
  int cos_index = 0;

  cos_index = (msr_read(IA32_PQR_ASSOC) >> 32);
  nk_vc_printf("CPU %d Get MSR info:\n", get_cur_thread() -> current_cpu);
  nk_vc_printf("Thread COS Index: %lu\n", get_cur_thread() -> cache_part_state);
  nk_vc_printf("IA32_PQR_ASSOC MSR COS Index: %lu\n", cos_index);
  nk_vc_printf("Bitmask for %lu: %lu\n", cos_index, msr_read(IA32_L3_QOS_MASK_0 + cos_index));
}

uint64_t test_cat_single_thread(uint64_t size, uint64_t iteration, int percent)
{
  volatile uint64_t z;
  volatile uint64_t s = 0;

  for (z = 0; z < MAXITS; z++) { //We run MAXITS times for every experiment and collect the average.
    uint64_t i, j, k;
    uint64_t timer_start, timer_end;
    uint64_t sum = 0;
    volatile int cos = 0;
    volatile char *data;

    data = (volatile char *) malloc(size);

    if (!data) {
      nk_vc_printf("Fail to allocate\n");
      return -1;
    }

    memset((char *) data, 0xf, size);

    nk_vc_printf("Running %lu iterations over %lu elements with CAT cache percent=%d on CPU %d\n",
           iteration, size, percent, get_cur_thread()->current_cpu);

    asm("wbinvd");

    if (percent) {
	if (nk_cache_part_acquire(percent)) {
	    nk_vc_printf("Cannot acquire partition - aborting\n");
	    return -1;
	}
	nk_vc_printf("COS: %d\n", cos);
    }

    timer_start = nk_sched_get_realtime();

    for (i = 0; i < iteration; i++) {
      for (j = 0; j < 64; j += 1) {
        for (k = j; k < size; k += 64) {
          sum += data[k];
        }
      }
    }

    timer_end = nk_sched_get_realtime();

    get_msr_info();

    if (percent && cos > 0) {
      nk_cache_part_release(cos);
    }

    memset((char *) data, 0, size);
    free((void *)data);

    nk_vc_printf("Time to completion: %lu ns\n", timer_end - timer_start);
    s += timer_end - timer_start;
  }

  nk_vc_printf("Average time to completion: %lu ns\n", s / MAXITS);

  return s / MAXITS;
}


uint64_t test_cat_multi_threads_single(uint64_t size, uint64_t iteration, int percent, int num_threads, int shared_cos)
{
  volatile uint64_t z;
  volatile uint64_t s = 0;

  for (z = 0; z < MAXITS; z++) {
    uint64_t i, j, k;
    uint64_t timer_start, timer_end;
    volatile uint64_t sum = 0;
    volatile int cos = 0;
    volatile char *data;

    data = (volatile char *) malloc(size);

    if (!data) {
      nk_vc_printf("Fail to allocate\n");
      return -1;
    }

    memset((char *) data, 0xf, size);

    if (percent) {
      if (shared_cos > 0) {
	if (nk_cache_part_select(shared_cos)) {
	    nk_vc_printf("Cannot select shared cos\n");
	}
        cos = shared_cos;
        nk_vc_printf("COS: %d\n", cos);
      } else {
	if (nk_cache_part_acquire(percent)) {
	    nk_vc_printf("Cannot acquired cos\n");
	}
	cos = nk_cache_part_get_current();
        nk_vc_printf("COS: %d\n", cos);
      }
    }

    nk_vc_printf("Running %lu iterations over %lu elements with CAT cache percent=%d with loop %d on CPU %d\n",
		 iteration, size, percent, MAXITS, get_cur_thread()->current_cpu);

    __sync_fetch_and_add(&num_finished_threads, 1);

    while (num_finished_threads % num_threads != 0) {}

    asm("wbinvd"); //We make sure that all the threads finish the wbinvd instruction so that it will not affect the result during the loop

    __sync_fetch_and_add(&num_ready_threads, 1);

    while (num_ready_threads % num_threads != 0) {}

    timer_start = nk_sched_get_realtime();

    for (i = 0; i < iteration; i++) {
      for (j = 0; j < 64; j += 1) {
        for (k = j; k < size; k += 64) {
          sum += data[k];
        }
      }
    }

    timer_end = nk_sched_get_realtime();

    get_msr_info();

    s += timer_end - timer_start;

    nk_vc_printf("CPU %d Time to completion: %lu ns\n", get_cur_thread() -> current_cpu, timer_end - timer_start);
    if (percent && cos > 0) {
        nk_cache_part_release(cos);
    }
    
    memset((char *) data, 0, size);
    free((void *) data);
  }

  nk_vc_printf("CPU: %d Average time: %lu\n", get_cur_thread() -> current_cpu, s / MAXITS);

  return s / MAXITS;
}

static void test_cat_multi_threads_func(void * in, void ** out)
{
  unsigned i = 0;
  int cos;
  uint64_t times;
  uint64_t s = 0;
  struct cache_part_input* data = (struct cache_part_input *)in;

  get_cur_thread()->vc = get_cur_thread()->parent->vc;

  test_cat_multi_threads_single(data -> size, data -> iteration, data -> percent,data -> init, data -> cos);
}


uint64_t test_cat_multi_threads(uint64_t size, uint64_t iteration, int percent, int num_threads, int shared)
{
  if (num_threads <= 0)
    return 0;

  unsigned i;
  struct cache_part_input in;
  uint64_t out1, out2;
  nk_thread_id_t* ids;
  uint64_t times;

  ids = malloc(num_threads * sizeof(*ids));

  if (!ids) {
      nk_vc_printf("Cannot allocate\n");
      return -1;
  }
  
  num_ready_threads = 0;
  num_finished_threads = 0;

  in.size = size;
  in.iteration = iteration;
  in.percent = percent;
  in.init = num_threads;
  in.cos = 0;

  if (shared) {
    nk_vc_printf("Shared cache: %d%%\n", percent * num_threads);
    if (nk_cache_part_acquire(percent * num_threads)) {
	nk_vc_printf("failed to acquire partition\n");
	return -1;
    }
  }

  in.cos = nk_cache_part_get_current();

  for (i = 0; i < num_threads; i++) {
      nk_thread_start(test_cat_multi_threads_func, &in, 0, 0, PAGE_SIZE_2MB, &ids[i], (4 * i + 3) % nk_get_num_cpus()); //Make sure the thread does not run on the same CPU
  }

  for (i = 0; i < num_threads; i++) {
    nk_join(ids[i], 0);
  }

  if (shared) {
    nk_cache_part_release(in.cos);
  }

  free(ids);

  return 0;
}
