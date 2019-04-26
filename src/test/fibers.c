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
 * Copyright (c) 2019, Enrico Deiana <ead@u.northwestern.edu>
 * Copyright (c) 2019, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author:  Michael A. Cuevas <cuevas@u.northwestern.edu>
 *          Enrico Deiana <ead@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *  
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/fiber.h>
#include <nautilus/scheduler.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>

#define DO_PRINT       0

#if DO_PRINT
#define PRINT(...) nk_vc_printf(__VA_ARGS__)
#else
#define PRINT(...) 
#endif

struct nk_virtual_console *vc;

void fiber_inner(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  while(a < 10){
    nk_vc_printf("Fiber inner a = %d\n", a++);
    nk_fiber_yield();
  }
  nk_vc_printf("Fiber inner is finished, a = %d\n", a);
}

void fiber_outer(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  while(a < 3){
    nk_vc_printf("Fiber outer a = %d\n", a++);
    nk_fiber_t *f_inner;
    nk_fiber_start(fiber_inner, 0, 0, 0, 1, &f_inner);
    nk_fiber_yield();
  }
  nk_vc_printf("Fiber outer is finished, a = %d\n", a);
}

void fiber_inner_join(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  while(a < 10){
    nk_vc_printf("Fiber inner a = %d\n", a++);
    nk_fiber_yield();
  }
  nk_vc_printf("Fiber inner is finished, a = %d\n", a);
}

void fiber_outer_join(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  while(a < 3){
    nk_vc_printf("Fiber outer a = %d\n", a++);
    nk_fiber_t *f_inner_join;
    nk_fiber_start(fiber_inner_join, 0, 0, 0, 1, &f_inner_join);
    nk_fiber_join(f_inner_join);
    nk_fiber_yield();
  }
  nk_vc_printf("Fiber outer is finished, a = %d\n", a);
}



void fiber4(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  while(a < 10){
    nk_vc_printf("Fiber 4 a = %d\n", a++);
    nk_fiber_yield();
  }
  nk_vc_printf("Fiber 4 is finished, a = %d\n", a);
}


void fiber3(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  while(a < 2){
    nk_vc_printf("Fiber 3 a = %d\n", a++);
    nk_fiber_yield();
  }
  nk_vc_printf("Fiber 3 is finished, a = %d\n", a);
  nk_fiber_t *f4;
  nk_fiber_start(fiber4, 0, 0, 0, 1, &f4);
}

void fiber1(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  while(a < 5){
    nk_vc_printf("Fiber 1 a = %d\n", a++);
    nk_fiber_yield();
  }
  nk_vc_printf("Fiber 1 is finished, a = %d\n", a);
  nk_fiber_t *f3;
  nk_fiber_start(fiber3, 0, 0, 0, 1, &f3);
}


void fiber2(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  while(a < 10){
    nk_vc_printf("Fiber 2 a = %d\n", a++);
    nk_fiber_yield();
  }
  nk_vc_printf("Fiber 2 is finished, a = %d\n", a);
  nk_fiber_t *f4;
  nk_fiber_start(fiber4, 0, 0, 0, 1, &f4);
}

void print_even(void *i, void **o){
  nk_fiber_set_vc(vc);
  for (int i = 0; i < 10; ++i){
    if ((i % 2) == 0){
      nk_vc_printf("Fiber even, counter = %d\n", i);
      nk_fiber_yield();
    }
  }
  nk_vc_printf("Fiber even is finished\n");

  return;
}

void print_odd(void *i, void **o){
  nk_fiber_set_vc(vc);
  for (int i = 0; i < 10; ++i){
    if ((i % 2) != 0){
      nk_vc_printf("Fiber odd, counter = %d\n", i);
      nk_fiber_yield();
    }
  }
  nk_vc_printf("Fiber odd is finished\n");

  return;
}

void fiber_first(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  nk_fiber_t *f_to = (nk_fiber_t*)i;
  while(a < 5){
    nk_vc_printf("Fiber_first() : a = %d and yielding to fiber_second = %p\n", a++, f_to);
    nk_fiber_yield_to(f_to);
  }
  nk_vc_printf("Fiber 1 is finished, a = %d\n", a);
}


void fiber_second(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  nk_fiber_t *f_to = (nk_fiber_t*)i;
  while(a < 5){
    nk_vc_printf("Fiber_second() : a = %d and yielding to fiber_third = %p\n", a++, f_to);
    nk_fiber_yield_to(f_to);
  }
  nk_vc_printf("Fiber 2 is finished, a = %d\n", a);
}

void fiber_third(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  //nk_fiber_t **temp = (nk_fiber_t**)i;
  nk_fiber_t *f_to = (nk_fiber_t*)i;
  while(a < 5){
    nk_vc_printf("fiber_third() : a = %d and yielding to fiber_fourth = %p\n", a++, f_to);
    nk_fiber_yield_to(f_to);
  }
  nk_vc_printf("fiber 3 is finished, a = %d\n", a);
}

void fiber_fourth(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  //nk_fiber_t **temp = (nk_fiber_t**)i;
  nk_fiber_t *f_to = (nk_fiber_t*)i;
  while(a < 5){
    nk_vc_printf("fiber_fourth() : a = %d and yielding to fiber_first = %p\n", a++, f_to);
    nk_fiber_yield_to(f_to);
  }
  nk_vc_printf("fiber 4 is finished, a = %d\n", a);
}

void fiber_fork(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  nk_fiber_t *f_new;
  f_new = nk_fiber_fork();
  while(a < 5){
    //nk_fiber_t *f_curr = _nk_fiber_current();
    nk_vc_printf("fiber_fork() : This is the %dth iteration of fiber %p\n"/* and curr_f is %p\n"*/, a++, f_new/*, f_curr*/);
    nk_fiber_yield();
  }
  nk_vc_printf("fiber 4 is finished, a = %d\n", a);
}

void fiber_fork_join(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  nk_fiber_t *f_new;
  f_new = nk_fiber_fork();
  if(f_new){
    nk_fiber_join(f_new);
  }

  //nk_fiber_t *f_curr = _nk_fiber_current();
  nk_fiber_t *f_curr = NULL;
  while(a < 5){
    nk_vc_printf("fiber_fork_join() : This is the %d iteration of fiber %p\n", a++, f_curr);
    nk_fiber_yield();
  }
  nk_vc_printf("fiber %p is finished, a = %d\n", f_curr, a);
}

void fiber_routine2(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  //nk_vc_printf("fiber_routine2() : Fiber %p created\n", _nk_fiber_current());
  nk_vc_printf("fiber_routine2() : Fiber created\n");
}


void fiber_routine1(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  nk_fiber_t *f_new;
  while(a < 5){
    f_new = nk_fiber_fork();
    //nk_fiber_t *curr = _nk_fiber_current();
    nk_fiber_t *curr = NULL;
    if(f_new){
      nk_fiber_t *new;
      nk_fiber_start(fiber_routine2, 0, 0, 0, 0, &new);
      break;
    }
    nk_vc_printf("fiber_routine1() : This is the %d iteration of fiber %p\n", a++, curr);
  }
  //nk_vc_printf("fiber_routine1() : fiber %p is finished, a = %d\n", _nk_fiber_current(), a);
  nk_vc_printf("fiber_routine1() : fiber is finished, a = %d\n", a);
}

void fiber_routine3(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  nk_fiber_t *f_new;
  while(a < 5){
    f_new = nk_fiber_fork();
    //nk_fiber_t *curr = _nk_fiber_current();
    nk_fiber_t *curr = NULL;
    nk_vc_printf("fiber_routine3() : This is the %d iteration of fiber %p\n", a++, curr);
  }
  //nk_vc_printf("fiber_routine3() : fiber %p is finished, a = %d\n", _nk_fiber_current(), a);
  nk_vc_printf("fiber_routine3() : fiber is finished, a = %d\n",  a);
}

#define N 100000
void first_timer(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  uint64_t start = 0;
  uint64_t end = 0;
  while(a < N){
    if (a == 2) {
        start = rdtsc();
    }
    nk_fiber_yield();
    a++;
  }
  end = rdtsc();
  nk_vc_printf("First Timer is finished, a = %d, cycle count = %d, cycles per iteration = %d\n", a, end-start, (end-start)/N);
}

void second_timer(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  while(a < N){
    nk_fiber_yield();
    a++;
  }
  nk_vc_printf("Second Timer is finished, a = %d\n", a);
}

int test_fibers_counter(){
  nk_fiber_t *even;
  nk_fiber_t *odd;
  vc = get_cur_thread()->vc;
  nk_fiber_start(print_even, 0, 0, 0, 1, &even);
  nk_fiber_start(print_odd, 0, 0, 0, 1, &odd);

  return 0;
}

int test_nested_fibers()
{
  nk_fiber_t *f_outer;
  vc = get_cur_thread()->vc;
  nk_vc_printf("test_nested_fibers() : virtual console %p\n", vc);
  nk_fiber_start(fiber_outer, 0, 0, 0, 1, &f_outer);
  return 0;
}

int test_fibers()
{
  nk_fiber_t *f1;
  nk_fiber_t *f2;
  nk_fiber_t *f3;
  vc = get_cur_thread()->vc;
  nk_vc_printf("test_fibers() : virtual console %p\n", vc);
  nk_fiber_start(fiber1, 0, 0, 0, 1, &f1);
  nk_fiber_start(fiber2, 0, 0, 0, 1, &f2);
  nk_fiber_start(fiber3, 0, 0, 0, 1, &f3);
  return 0;
}

int test_yield_to()
{
  nk_fiber_t *f_first;
  nk_fiber_t *f_second;
  nk_fiber_t *f_third;
  nk_fiber_t *f_fourth;
  vc = get_cur_thread()->vc;
  nk_vc_printf("test_yield_to() : virtual console %p\n", vc);
  nk_fiber_create(fiber_fourth, 0, 0, 0, &f_fourth);
  nk_fiber_create(fiber_first, 0, 0, 0, &f_first);
  nk_fiber_create(fiber_third, 0, 0, 0, &f_third);
  nk_fiber_create(fiber_second, 0, 0, 0, &f_second);
  //void *input[4] = {&f_first, &f_second, &f_third, &f_fourth};
  f_first->input = f_second;
  f_second->input = f_third;
  f_third->input = f_fourth;
  f_fourth->input = f_first;
  nk_fiber_run(f_fourth, 1);
  nk_fiber_run(f_first, 1);
  nk_fiber_run(f_third, 1);
  nk_fiber_run(f_second, 1);
  return 0;
}

int test_fiber_join()
{
  nk_fiber_t *f_outer_join;
  vc = get_cur_thread()->vc;
  nk_vc_printf("test_fiber_join() : virtual console %p\n", vc);
  nk_fiber_start(fiber_outer_join, 0, 0, 0, 1, &f_outer_join);
  return 0;
}

int test_fiber_fork()
{
  nk_fiber_t *f_orig;
  vc = get_cur_thread()->vc;
  nk_vc_printf("test_fiber_fork() : virtual console %p\n", vc);
  nk_fiber_start(fiber_fork, 0, 0, 0, 1, &f_orig);
  return 0;
}

int test_fiber_fork_join()
{
  nk_fiber_t *f_orig;
  vc = get_cur_thread()->vc;
  nk_vc_printf("test_fiber_fork_join() : virtual console %p\n", vc);
  nk_fiber_start(fiber_fork_join, 0, 0, 0, 1, &f_orig);
  return 0;
}

int test_fiber_routine()
{
  nk_fiber_t *f_orig;
  vc = get_cur_thread()->vc;
  nk_vc_printf("test_fiber_routine() : virtual console %p\n", vc);
  nk_fiber_start(fiber_routine1, 0, 0, 0, 1, &f_orig);
  return 0;
}

int test_fiber_routine_2()
{
  nk_fiber_t *f_orig;
  vc = get_cur_thread()->vc;
  nk_vc_printf("test_fiber_routine_2() : virtual console %p\n", vc);
  nk_fiber_start(fiber_routine3, 0, 0, 0, 1, &f_orig);
  return 0;
}

int test_fiber_timing(){
  nk_fiber_t *first;
  nk_fiber_t *second;
  vc = get_cur_thread()->vc;
  
  nk_fiber_start(first_timer, 0, 0, 0, 1, &first);
  nk_fiber_start(second_timer, 0, 0, 0, 1, &second);

  return 0;
}


static int
handle_fibers (char * buf, void * priv)
{
  test_fibers();

  return 0;
}

static int
handle_fibers2 (char * buf, void * priv)
{
  test_nested_fibers();

  return 0;
}

static int
handle_fibers3 (char * buf, void * priv)
{
  test_fibers_counter();

  return 0;
}

static int
handle_fibers4 (char * buf, void * priv)
{
  test_yield_to();

  return 0;
}

static int
handle_fibers5 (char * buf, void * priv)
{
  test_fiber_join();
  return 0;
}

static int
handle_fibers6 (char * buf, void * priv)
{
  test_fiber_fork();
  return 0;
}

static int
handle_fibers7 (char * buf, void * priv)
{
  test_fiber_fork_join();
  return 0;
}

static int
handle_fibers8 (char * buf, void * priv)
{
  test_fiber_routine();
  return 0;
}

static int
handle_fibers9 (char * buf, void * priv)
{
  test_fiber_routine_2();
  return 0;
}

static int
handle_fibers10 (char * buf, void * priv)
{
  test_fiber_timing();
  return 0;
}


static struct shell_cmd_impl fibers_impl = {
  .cmd      = "fibertest",
  .help_str = "fibertest",
  .handler  = handle_fibers,
};

static struct shell_cmd_impl fibers_impl2 = {
  .cmd      = "fibertest2",
  .help_str = "fibertest2",
  .handler  = handle_fibers2,
};

static struct shell_cmd_impl fibers_impl3 = {
  .cmd      = "fibertest3",
  .help_str = "fibertest3",
  .handler  = handle_fibers3,
};

static struct shell_cmd_impl fibers_impl4 = {
  .cmd      = "fibertest4",
  .help_str = "fibertest4",
  .handler  = handle_fibers4,
};

static struct shell_cmd_impl fibers_impl5 = {
  .cmd      = "fibertest5",
  .help_str = "fibertest5",
  .handler  = handle_fibers5,
};

static struct shell_cmd_impl fibers_impl6 = {
  .cmd      = "fibertest6",
  .help_str = "fibertest6",
  .handler  = handle_fibers6,
};

static struct shell_cmd_impl fibers_impl7 = {
  .cmd      = "fibertest7",
  .help_str = "fibertest7",
  .handler  = handle_fibers7,
};

static struct shell_cmd_impl fibers_impl8 = {
  .cmd      = "fibertest8",
  .help_str = "fibertest8",
  .handler  = handle_fibers8,
};

static struct shell_cmd_impl fibers_impl9 = {
  .cmd      = "fibertest9",
  .help_str = "fibertest9",
  .handler  = handle_fibers9,
};

static struct shell_cmd_impl fibers_impl10 = {
  .cmd      = "fibertime1",
  .help_str = "fibertime1",
  .handler  = handle_fibers10,
};


nk_register_shell_cmd(fibers_impl);
nk_register_shell_cmd(fibers_impl2);
nk_register_shell_cmd(fibers_impl3);
nk_register_shell_cmd(fibers_impl4);
nk_register_shell_cmd(fibers_impl5);
nk_register_shell_cmd(fibers_impl6);
nk_register_shell_cmd(fibers_impl7);
nk_register_shell_cmd(fibers_impl8);
nk_register_shell_cmd(fibers_impl9);
nk_register_shell_cmd(fibers_impl10);
