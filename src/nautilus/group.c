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
#include <nautilus/thread.h>
#include <nautilus/atomic.h>
#include <nautilus/list.h>

#include <nautilus/group.h>
#include <nautilus/group_sched.h>

#define MAX_CPU_NUM NAUT_CONFIG_MAX_CPUS

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

typedef struct group_member {
  nk_thread_t *thread;
  struct list_head group_member_node;
} group_member_t;

typedef struct nk_thread_group {
  char group_name[MAX_GROUP_NAME];
  uint64_t group_id;
  int group_leader;
  uint64_t group_size;
  uint64_t next_id;

  struct list_head group_member_array[MAX_CPU_NUM];

  nk_barrier_t group_barrier;

  spinlock_t group_lock;

  void *message;
  int   msg_flag;
  uint64_t msg_count;
  int terminate_bcast;

  void *state;

  struct list_head thread_group_node;
} nk_thread_group_t;

typedef struct parallel_thread_group_list {
  spinlock_t group_list_lock;

  uint64_t num_groups;

  struct list_head group_list_node;
} parallel_thread_group_list_t;

static parallel_thread_group_list_t parallel_thread_group_list;

/*****************************************************/
/***************Below are Internal APIs***************/
/*****************************************************/

static inline void
bspin_lock (volatile int * lock) {
  while (__sync_lock_test_and_set(lock, 1));
}

static inline void
bspin_unlock (volatile int * lock) {
  __sync_lock_release(lock);
}

// init the global group list
// should be called in system init
static void
thread_group_list_init(void) {
  parallel_thread_group_list.num_groups = 0;
  INIT_LIST_HEAD(&parallel_thread_group_list.group_list_node);
  spinlock_init(&parallel_thread_group_list.group_list_lock);
}

// deinit the global group list
// hasn't been used so far
static int
thread_group_list_deinit(void) {
  spinlock_t* lock = &parallel_thread_group_list.group_list_lock;
  spin_lock(lock);
  if (!list_empty(&parallel_thread_group_list.group_list_node)) {
    DEBUG("Can't deinit group list!\n");
    spin_unlock(lock);
    return -1;
  } else {
    if (parallel_thread_group_list.num_groups != 0) {
      ERROR("The num_groups isn't 0 when parallel_thread_group_list is empty!\n");
    }

    spinlock_deinit(&parallel_thread_group_list.group_list_lock);
    spin_unlock(lock);
    return 0;
  }
}

// create a group member and init it
static group_member_t*
thread_group_member_create(void) {
  group_member_t *group_member = (group_member_t *)MALLOC(sizeof(group_member_t));

  if (group_member == NULL) {
    ERROR("Fail to malloc space for group member!\n");
    return NULL;
  }

  if (memset(group_member, 0, sizeof(group_member_t)) == NULL) {
    FREE(group_member);
    ERROR("Fail to clear memory for group member!\n");
    return NULL;
  }

  group_member->thread = get_cur_thread();

  INIT_LIST_HEAD(&group_member->group_member_node);

  return group_member;
}

// delete the group member from list and free it
static void
thread_group_member_destroy(group_member_t *group_member) {
  list_del(&group_member->group_member_node);
  FREE(group_member);
}

// assign a new id for a new group
// returns -1ULL on failure, positive number otherwise
static uint64_t
thread_group_get_next_group_id(void) {
  parallel_thread_group_list_t * l = &parallel_thread_group_list;
  if (list_empty(&l->group_list_node)) {
    return 0;
  } else {
    nk_thread_group_t *last_member = list_first_entry(&l->group_list_node, nk_thread_group_t, thread_group_node);
    return last_member->group_id + 1;
  }
}

static void
thread_group_barrier_init (nk_barrier_t *barrier) {
  DEBUG_BARRIER("Initializing group barrier, group barrier at %p, count=%u\n", (void*)barrier, 0);

  if (memset(barrier, 0, sizeof(nk_barrier_t)) == NULL) {
    ERROR("Fail to memset barrier\n");
  }

  barrier->lock = 0;
  barrier->notify = 0;
  barrier->init_count = 0;
  barrier->remaining  = 0;
}

static int
thread_group_barrier_wait (nk_barrier_t *barrier) {
  int res = 0;

  bspin_lock(&barrier->lock);

  DEBUG_BARRIER("Thread (%p) entering barrier (%p)\n", (void*)get_cur_thread(), (void*)barrier);

  if (--barrier->remaining == 0) {
    res = NK_BARRIER_LAST;
    atomic_cmpswap(barrier->notify, 0, 1); // the last thread comes in set notify
    DEBUG_BARRIER("Thread (%p): notify\n", (void*)get_cur_thread());
  } else {
    DEBUG_BARRIER("Thread (%p): remaining count = %d\n", (void*)get_cur_thread(), barrier->remaining);
    bspin_unlock(&barrier->lock);
    BARRIER_WHILE (barrier->notify != 1);
  }

  if (atomic_inc_val(barrier->remaining) == barrier->init_count) {
    atomic_cmpswap(barrier->notify, 1, 0); // the last thread goes out reset notify
    DEBUG_BARRIER("Thread (%p): reset notify\n", (void*)get_cur_thread());
    bspin_unlock(&barrier->lock);
  }

  DEBUG_BARRIER("Thread (%p) exiting barrier (%p)\n", (void*)get_cur_thread(), (void*)barrier);

  return res;
}

static void
thread_group_barrier_join (nk_barrier_t *barrier) {
  bspin_lock(&barrier->lock);
  DEBUG_BARRIER("Thread (%p) joining barrier \n", (void*)get_cur_thread());
  atomic_inc(barrier->init_count);
  atomic_inc(barrier->remaining);
  bspin_unlock(&barrier->lock);
}

static int
thread_group_barrier_leave (nk_barrier_t *barrier) {
  int res = 0;

  DEBUG_BARRIER("Thread (%p) leaving barrier (%p)\n", (void*)get_cur_thread(), (void*)barrier);

  bspin_lock(&barrier->lock);

  atomic_dec(barrier->init_count);

  if (--barrier->remaining == 0) {
    res = NK_BARRIER_LAST;
    atomic_cmpswap(barrier->notify, 0, 1); // if I'm the last one, I should set notify
    DEBUG_BARRIER("Thread (%p): notify\n", (void*)get_cur_thread());
  }

  bspin_unlock(&barrier->lock);

  return res;
}

/*****************************************************/
/***************Below are External APIs***************/
/*****************************************************/

// init of module
int
nk_thread_group_init(void) {
  DEBUG("Inited\n");

  thread_group_list_init();

  return 0;
}

// deinit of module
int
nk_thread_group_deinit(void) {
  DEBUG("Deinited\n");

  return 0;
}

// create a group and init it
nk_thread_group_t *
nk_thread_group_create(char *name) {
  void *ret = NULL;

  nk_thread_group_t* new_group = (nk_thread_group_t *) MALLOC(sizeof(nk_thread_group_t));

  if (new_group == NULL) {
    ERROR("Fail to malloc space for group!\n");
    return NULL;
  }

  if (memset(new_group, 0, sizeof(nk_thread_group_t)) == NULL) {
    FREE(new_group);
    ERROR("Fail to clear memory for group!\n");
    return NULL;
  }

  new_group->group_leader = -1;

  ret = strncpy(new_group->group_name,name,MAX_GROUP_NAME);  new_group->group_name[MAX_GROUP_NAME-1] = 0;

  if (ret == NULL) {
    FREE(new_group);
    ERROR("Fail to copy name for group!\n");
    return NULL;
  }

  for (int i = 0; i < MAX_CPU_NUM; i++) {
    INIT_LIST_HEAD(&new_group->group_member_array[i]);
  }

  INIT_LIST_HEAD(&(new_group->thread_group_node));

  list_add(&(new_group->thread_group_node), &(parallel_thread_group_list.group_list_node));

  spinlock_init(&new_group->group_lock);

  new_group->group_id = thread_group_get_next_group_id();

  if (new_group->group_id == -1ULL) {
    ERROR("Fail to assign group id!\n");
    FREE(new_group);
    return NULL;
  }

  thread_group_barrier_init(&new_group->group_barrier);

  return new_group;
}

// attach a state to the group
int
nk_thread_group_attach_state(nk_thread_group_t *group, void *state) {
  group->state = state;

  return 0;
}

// detach the state of a group
void *
nk_thread_group_detach_state(nk_thread_group_t *group) {
  group->state = NULL;

  return NULL;
}

// get the schedule state of cur group
void *
nk_thread_group_get_state(nk_thread_group_t *group) {
  if (group) {
    return group->state;
  } else {
    return NULL;
  }
}

// search for a thread group by name
nk_thread_group_t *
nk_thread_group_find(char *name) {
  struct list_head *cur = NULL;
  nk_thread_group_t *cur_group = NULL;
  parallel_thread_group_list_t * l = &parallel_thread_group_list;

  spin_lock(&l->group_list_lock);

  list_for_each(cur, (struct list_head *)&parallel_thread_group_list.group_list_node) {
    cur_group = list_entry(cur, nk_thread_group_t, thread_group_node);

    if (! (strcmp(cur_group->group_name, name) ))  {
      spin_unlock(&l->group_list_lock);
      return cur_group;
    }
  }

  spin_unlock(&l->group_list_lock);

  return NULL;
}

// current thread joins a group
int
nk_thread_group_join(nk_thread_group_t *group) {
  group_member_t* group_member = thread_group_member_create();

  if (group_member == NULL) {
    ERROR("Fail to create group member!\n");
    return -1;
  }

  thread_group_barrier_join(&group->group_barrier);

  atomic_inc(group->group_size);
  int id = atomic_inc(group->next_id);

  spin_lock(&group->group_lock);
  list_add(&group_member->group_member_node, &group->group_member_array[my_cpu_id()]);
  spin_unlock(&group->group_lock);

  return id;
}

// current thread leaves a group
int
nk_thread_group_leave(nk_thread_group_t *group) {
  group_member_t *leaving_member = NULL;
  struct nk_thread *cur_thread = get_cur_thread();
  struct list_head *cur;

  spin_lock(&group->group_lock);

  // currently assumes no thread migration TODO:
  list_for_each(cur, &group->group_member_array[my_cpu_id()]) {
    leaving_member = list_entry(cur, group_member_t, group_member_node);
    if (cur_thread == leaving_member->thread) {
      break;
    }
  }

  if (!leaving_member) {
      ERROR("Unable to find self within thread group\n");
      spin_unlock(&group->group_lock);
      thread_group_barrier_leave(&group->group_barrier);
      return -1;
  }

  if (cur == &group->group_member_array[my_cpu_id()]) {
    ERROR("Fail to find leaving member in group_member_array!\n");
    spin_unlock(&group->group_lock);
    thread_group_barrier_leave(&group->group_barrier);
    return -1;
  }

  list_del(&leaving_member->group_member_node);

  spin_unlock(&group->group_lock);

  FREE(leaving_member);

  thread_group_barrier_leave(&group->group_barrier);

  atomic_dec(group->group_size);

  return 0;
}

// delete a group, should fail if the group is unempty
int
nk_thread_group_delete(nk_thread_group_t *group) {
  if (group->group_size != 0) {
    DEBUG("Unable to delete thread group!\n");
    return -1;
  }

  list_del(&group->thread_group_node);

  //All group members should have been freed.
  FREE(group);

  return 0;
}

// all threads in the group call to synchronize
int
nk_thread_group_barrier(nk_thread_group_t *group) {
  return thread_group_barrier_wait(&group->group_barrier);
}

// all threads in the group call to select one thread as leader
int
nk_thread_group_election(nk_thread_group_t *group) {
  nk_thread_t *c = get_cur_thread();

  int leader = atomic_cmpswap(group->group_leader, -1, c->tid);
  if (leader == -1){
    return 1;
  }

  return 0;
}

// reset leader
int
nk_thread_group_reset_leader(nk_thread_group_t *group) {
  group->group_leader = -1;

  return 0;
}

// check if I'm the leader
int
nk_thread_group_check_leader(nk_thread_group_t *group) {
  nk_thread_t *c = get_cur_thread();

  if (group->group_leader == c->tid) {
    return 1;
  }

  return 0;
}

// broadcast a message to all members of the thread group be waiting here
// if some threads don't get this message, they just enter the next round
// need to be further flushed out
int
nk_thread_group_broadcast(nk_thread_group_t *group, void *message, uint64_t tid, uint64_t src) {
  if (tid != src) {
    // receiver
    int ret = atomic_inc_val(group->msg_count);
    DEBUG("msg_count = %d\n", group->msg_count);

    while (group->msg_flag == 0) {
      DEBUG("t%d is waiting\n", tid);
      if (group->terminate_bcast) {
        return 0;
      }
    }

    message = group->message;
    DEBUG("Recv: %s", (char *)message);

    if (atomic_dec_val(group->msg_count) == 0) {
      group->message = NULL;
      group->msg_flag = 0;
      DEBUG("Reset msg\n");
    }

    DEBUG("msg_count = %d\n", group->msg_count);
  } else {
    // sender
    while (group->msg_flag == 1) {
      DEBUG("t%d is sending\n", tid);
      if (group->terminate_bcast) {
        return 0;
      }
    }

    group->message = message;
    group->msg_flag = 1;
    DEBUG("Msg sent\n");
    DEBUG("Send: %s", (char *)message);
  }

  return 0;
}

// terminate the bcast, then nobody will be waiting for sending or recieving
int
nk_thread_group_broadcast_terminate(nk_thread_group_t *group) {
  atomic_cmpswap(group->terminate_bcast, 0, 1);

  return 0;
}

// return the size of a group
uint64_t
nk_thread_group_get_size(nk_thread_group_t *group) {
  return group->group_size;
}
