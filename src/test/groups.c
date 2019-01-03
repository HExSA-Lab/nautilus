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
 * Copyright (c) 2017, Xiaoyang Wang <xiaoyangwang2018@u.northwestern.edu>
 *                     Jinghang Wang`<jinghangwang2018@u.northwestern.edu>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Xiaoyang Wang <xiaoyangwang2018@u.northwestern.edu>
 *           Jinghang Wang <jinghangwang2018@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/atomic.h>
#include <nautilus/group.h>
#include <nautilus/group_sched.h>
#include <test/groups.h>

#define CPU_OFFSET 0 // skip CPU0 in tests
#define TESTER_TOTAL 2
#define BARRIER_TEST_LOOPS 1

// TODO: inport priority from scheduler
#define DEFAULT_PRIORITY (1000000000ULL/NAUT_CONFIG_HZ)

#define SANITY_CHECKS 0

#if SANITY_CHECKS
#define PAD 0
#define MALLOC(x) ({ void *p  = malloc((x)+2*PAD); if (!p) { panic("Failed to Allocate %d bytes\n",x); } memset(p,0,(x)+2*PAD); p+PAD; })
#define FREE(x) do {if (x) { free(x-PAD); x=0;} } while (0)
#else // no sanity checks
#define MALLOC(x) malloc(x)
#define FREE(x) free(x)
#endif // sanity checks

#ifdef NAUT_CONFIG_DEBUG_GROUP
#define DEBUG(fmt, args...)  DEBUG_PRINT("group: " fmt, ##args)
#else
#define DEBUG(fmt, args...)
#endif

#ifdef NAUT_CONFIG_DEBUG_GROUP_BARRIER
#define DEBUG_BARRIER(fmt, args...)  DEBUG_PRINT("group: " fmt, ##args)
#else
#define DEBUG_BARRIER(fmt, args...)
#endif

#define ERROR(fmt, args...) ERROR_PRINT("group: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("group: " fmt, ##args)

static int tester_num; // the number of tester in one round
uint64_t dur_array[TESTER_TOTAL][5];

// int tester_total;
// uint64_t *dur_array = malloc(sizeof(uint64_t)*tester_total*5);
// #define DEREF(i, j) *(dur_array + 5*i + j)

static void
thread_group_dur_dump(void) {
  for (int i = 0; i < tester_num; i++) {
    nk_vc_log_wrap("%llu,%llu,%llu,%llu,%llu,%llu\n",
                  i, dur_array[i][0], dur_array[i][1], dur_array[i][2], dur_array[i][3], dur_array[i][4]);
  }

  for (int i = 0; i < tester_num; i++) {
    nk_vc_printf("index: %llu join: %llu election: %llu group_change: %llu local_change: %llu barrier: %llu cycles\n",
                  i, dur_array[i][0], dur_array[i][1], dur_array[i][2], dur_array[i][3], dur_array[i][4]);
  }
}

static void
thread_group_tester(void *in, void **out) {
  uint64_t dur[5] = {0, 0, 0, 0, 0};

  uint64_t start, end;

  static struct nk_sched_constraints *constraints;

  nk_thread_group_t *dst = nk_thread_group_find((char*) in);

  if (!dst) {
    DEBUG("group_find failed\n");
    return;
  }

  start = rdtsc();
  int tid = nk_thread_group_join(dst);
  end = rdtsc();

  if (tid < 0) {
    DEBUG("group join failed\n");
    return;
  }

  dur_array[tid][0] = end - start;

  char *name = (char *)MALLOC(MAX_GROUP_NAME*sizeof(char));
  if (name == NULL) {
    DEBUG("Fail to malloc space for tester name!\n");
    return;
  }

  sprintf(name, "tester %d", tid);
  nk_thread_name(get_cur_thread(), name);

  int i = 0;
  while (nk_thread_group_get_size(dst) != tester_num) {
#ifdef NAUT_CONFIG_DEBUG_GROUP
    i += 1;
    if (i == 0xffffff) {
      DEBUG("group_size = %d\n", nk_thread_group_get_size(dst));
      i = 0;
    }
#endif
  }

#ifdef NAUT_CONFIG_DEBUG_GROUP
  if (tid == 0) {
    DEBUG("All joined!\n");
  }
#endif

  start = rdtsc();
  nk_thread_group_election(dst);
  end = rdtsc();

  dur_array[tid][1] = end - start;

  if (nk_thread_group_check_leader(dst)) {
    constraints = MALLOC(sizeof(struct nk_sched_constraints));
    constraints->type = APERIODIC;
    constraints->interrupt_priority_class = 0x01;
    constraints->aperiodic.priority = DEFAULT_PRIORITY;
  }

  start = rdtsc();
  if (nk_group_sched_change_constraints(dst, constraints)) {
    end = rdtsc();
    DEBUG("t%d change constraint failed!\n", tid);
  } else {
    end = rdtsc();
    DEBUG("t%d #\n", tid);
  }

  dur_array[tid][2] = end - start;

  //change_constraint measure
  start = rdtsc();
  if (nk_sched_thread_change_constraints(constraints)) {
      DEBUG("Failed to change thread constraints\n");
  }
  end = rdtsc();

  dur_array[tid][3] = end - start;

  //barrier test
  int ret;
  for (i = 0; i < BARRIER_TEST_LOOPS; ++i) {
    start = rdtsc();
    ret = nk_thread_group_barrier(dst);
    end = rdtsc();
    if (ret) {
      DEBUG("t%d &\n", tid);
    }
  }

  dur_array[tid][4] = end - start;

  nk_thread_group_barrier(dst);

  nk_thread_group_leave(dst);

  if (nk_thread_group_delete(dst) != -1) {
    FREE(in);
    return;
  }

  return;
}

static int
thread_group_test_launcher() {
  uint64_t i = 0;

  if (memset(dur_array, 0, TESTER_TOTAL*5*sizeof(uint64_t)) == NULL) {
    DEBUG("memset dur_array failed\n");
    return -1;
  }

  char* group_name = (char *)MALLOC(MAX_GROUP_NAME*sizeof(char));
  if (group_name == NULL) {
    DEBUG("malloc group name failed\n");
    FREE(group_name);
    return -1;
  }

  if (memset(group_name, 0, MAX_GROUP_NAME*sizeof(char)) == NULL) {
    DEBUG("memset group name failed\n");
    FREE(group_name);
    return -1;
  }

  nk_thread_group_t *new_group = NULL;
  nk_thread_group_t *ret = NULL;
  nk_thread_id_t *tids = (nk_thread_id_t *)MALLOC(tester_num*sizeof(nk_thread_id_t));

  if (tids == NULL) {
    DEBUG("malloc tids failed\n");
    FREE(tids);
    return -1;
  }

  if (memset(tids, 0, tester_num*sizeof(nk_thread_id_t)) == NULL) {
    DEBUG("memset tids failed\n");
    FREE(tids);
    return -1;
  }

  sprintf(group_name, "Group Alpha");

  new_group = nk_thread_group_create(group_name);

  if (new_group == NULL) {
    DEBUG("group_create failed\n");
    return -1;
  }

  ret = nk_thread_group_find(group_name);

  if (ret != new_group) {
    DEBUG("result from group_create does not match group_find!\n");
  }

  // launch a few aperiodic threads (testers), i.e. regular threads
  // each join the group
  for (i = 0; i < tester_num; i++) {
    if (nk_thread_start(thread_group_tester, (void*)group_name , NULL, 0, PAGE_SIZE_4KB, &tids[i], i + CPU_OFFSET)) {
      DEBUG("Fail to start thread_group_tester %d\n", i);
    }
  }

  for (i = 0; i < tester_num; i++) {
    if (nk_join(tids[i], NULL)) {
      DEBUG("Fail to join thread_group_tester %d\n", i);
    }
  }

  FREE(tids);

  return 0;
}

int
nk_thread_group_test() {
  // warm up round is to get rid of cold-start effect
  nk_vc_printf("Warm Up\n");
  tester_num = TESTER_TOTAL;
  thread_group_test_launcher();

  for (int i = 1; i < TESTER_TOTAL + 1; i = i * 2) {
    nk_vc_printf("Round: %d\n", i);
    tester_num = i;
    thread_group_test_launcher();
  }

  nk_vc_printf("Test Finished\n");

  thread_group_dur_dump();

  return 0;
}

static int
handle_groups (char * buf, void * priv)
{
    return nk_thread_group_test();
}

static struct shell_cmd_impl groups_impl = {
    .cmd      = "grouptest",
    .help_str = "grouptest",
    .handler  = handle_groups,
};
nk_register_shell_cmd(groups_impl);
