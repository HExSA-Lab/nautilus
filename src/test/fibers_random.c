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
 * Copyright (c) 2019, Michael A. Cuevas <cuevas@u.northwestern.edu>
 * Copyright (c) 2019, Souradip Ghosh  <SouradipGhosh2021@u.northwestern.edu>
 * Copyright (c) 2019, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Michael A. Cuevas <cuevas@u.northwestern.edu>
 *          Souradip Ghosh  <SouradipGhosh2021@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *  
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/fiber.h>
#include <nautilus/scheduler.h>
#include <nautilus/random.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>

#define DO_PRINT       0
#define MAX_FIBERS     5000

#if DO_PRINT
#define PRINT(...) nk_vc_printf(__VA_ARGS__)
#else
#define PRINT(...) 
#endif

extern struct nk_virtual_console *vc;


/******************* Test Routines *******************/

void routine_simple_1(void *i, void **o)
{
  uint64_t *end = (uint64_t *)i;
  nk_fiber_set_vc(vc);
  unsigned int a = 0;
  while(a < *end){
    nk_vc_printf("routine_simple_1() : a = %d\n", a++);
    nk_fiber_yield();
  }
  nk_vc_printf("Fiber routine_simple_1 is finished, a = %d\n", a);
  free(end);
}

void routine_simple_2(void *i, void **o)
{
  uint64_t *end = (uint64_t *)i;
  nk_fiber_set_vc(vc);
  unsigned int a = 0;
  while(a < *end){
    nk_vc_printf("routine_simple_2() : a = %d\n", a++);
    nk_fiber_yield();
  }
  nk_vc_printf("Fiber routine_simple_2 is finished, a = %d\n", a);
  free(end);
}

/******************* Test Wrappers *******************/

int test_simple_routine1()
{
  nk_fiber_t *simple;
  vc = get_cur_thread()->vc;
  uint64_t t;
  nk_get_rand_bytes((uint8_t *)&t, sizeof(t)); 
  uint64_t *f_input = malloc(sizeof(uint64_t *));
  if (!f_input) {
    nk_vc_printf("test_simple_routine() : Malloc failed\n");
    return -1;
  }
  *f_input = t % MAX_FIBERS;
  nk_fiber_start(routine_simple_1, (void *)f_input, 0, 0, 1, &simple);
  return 0;
}

int test_simple_routine2()
{
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  uint64_t t1;
  uint64_t t2;
  nk_get_rand_bytes((uint8_t *)&t1, sizeof(t1)); 
  nk_get_rand_bytes((uint8_t *)&t2, sizeof(t2)); 
  uint64_t *f1_input = malloc(sizeof(uint64_t *));
  uint64_t *f2_input = malloc(sizeof(uint64_t *));
  if (!f1_input || !f2_input) {
    nk_vc_printf("test_simple_routine() : Malloc failed\n");
    return -1;
  }
  *f1_input = t1 % MAX_FIBERS;
  *f2_input = t2 % MAX_FIBERS;
  nk_fiber_start(routine_simple_1, (void *)f1_input, 0, 0, 1, &simple1);
  nk_fiber_start(routine_simple_2, (void *)f2_input, 0, 0, 1, &simple2);
  return 0;
}

/******************* Test Handlers *******************/

static int
handle_fibers1(char * buf, void * priv)
{
  test_simple_routine1();
  return 0;
}

static int
handle_fibers2(char * buf, void * priv)
{
  test_simple_routine2();
  return 0;
}

/******************* Shell Structs ********************/

static struct shell_cmd_impl fibers_impl1 = {
  .cmd      = "instrument1",
  .help_str = "Simple compiler instrumentation test 1",
  .handler  = handle_fibers1,
};

static struct shell_cmd_impl fibers_impl2 = {
  .cmd      = "instrument2",
  .help_str = "Simple compiler instrumentation test 2",
  .handler  = handle_fibers2,
};

/******************* Shell Commands *******************/

nk_register_shell_cmd(fibers_impl1);
nk_register_shell_cmd(fibers_impl2);
