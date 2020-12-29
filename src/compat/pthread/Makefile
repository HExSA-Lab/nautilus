NK_LIB =\
	nk/

MUTEX_OBJS = \
  pthread_mutex_unlock.o \
  pthread_mutex_init.o \
  pthread_mutex_destroy.o \
  pthread_mutex_lock.o \
  pthread_mutex_timedlock.o \
  pthread_mutex_trylock.o 

MUTEXATTR_OBJS = \
  pthread_mutexattr_destroy.o \
  pthread_mutexattr_getkind_np.o \
  pthread_mutexattr_getpshared.o \
  pthread_mutexattr_gettype.o \
  pthread_mutexattr_init.o \
  pthread_mutexattr_setkind_np.o \
  pthread_mutexattr_setpshared.o \
  pthread_mutexattr_settype.o

SUPPORT_OBJS = \
  pte_relmillisecs.o \
  pte_mutex_check_need_init.o \
  pte_threadDestroy.o \
  pte_new.o \
  pte_threadStart.o \
  global.o \
  pte_reuse.o \
  pthread_init.o \
  pthread_terminate.o

THREAD_OBJS = \
  create.o \
  pthread_self.o \
  pthread_equal.o \
  pthread_join.o \
  pthread_detach.o \
  pte_detach.o \
  pte_callUserDestroyRoutines.o \
  pte_tkAssocDestroy.o \
  pthread_kill.o \
  pthread_attr_destroy.o \
  pthread_attr_getdetachstate.o \
  pthread_attr_getinheritsched.o \
  pthread_attr_getschedparam.o \
  pthread_attr_getschedpolicy.o \
  pthread_attr_getscope.o \
  pthread_attr_getstackaddr.o \
  pthread_attr_getstacksize.o \
  pthread_attr_init.o \
  pthread_attr_setdetachstate.o \
  pthread_attr_setinheritsched.o \
  pthread_attr_setschedparam.o \
  pthread_attr_setschedpolicy.o \
  pthread_attr_setscope.o \
  pthread_attr_setstackaddr.o \
  pthread_attr_setstacksize.o \
  pte_is_attr.o \
  pthread_exit.o \
  pthread_getschedparam.o \
  pthread_setschedparam.o \
  sched_get_priority_max.o \
  sched_get_priority_min.o \
  pthread_attr_getstack.o \
  pthread_getattr_np.o

TLS_OBJS = \
  pthread_key_create.o \
  pthread_key_delete.o \
  pthread_getspecific.o \
  pthread_setspecific.o \
  pte_tkAssocCreate.o

MISC_OBJS = \
  sched_yield.o \
  pthread_delay_np.o \
  pthread_testcancel.o \
  pte_throw.o \
  cleanup.o \
  pthread_once.o \
  pthread_num_processors_np.o \
  pte_getprocessors.o \
  pte_spinlock_check_need_init.o \
  global.o \
  pthread_timechange_handler_np.o \
  pte_cond_check_need_init.o \
	pthread_getconcurrency.o \
	pthread_setconcurrency.o \
  pte_cancellable_wait.o

SEM_OBJS = \
  sem_close.o \
  sem_destroy.o \
  sem_getvalue.o \
  sem_init.o \
  sem_open.o  \
  sem_post.o \
  sem_post_multiple.o \
  sem_timedwait.o \
  sem_trywait.o \
  sem_unlink.o \
  sem_wait.o

BARRIER_OBJS = \
  pthread_barrier_init.o \
  pthread_barrier_destroy.o \
  pthread_barrier_wait.o \
  pthread_barrierattr_init.o \
  pthread_barrierattr_destroy.o \
  pthread_barrierattr_getpshared.o \
  pthread_barrierattr_setpshared.o \
  
SPIN_OBJS = \
  pthread_spin_destroy.o \
  pthread_spin_init.o \
  pthread_spin_lock.o \
  pthread_spin_trylock.o \
  pthread_spin_unlock.o

CONDVAR_OBJS = \
  pthread_cond_destroy.o \
  pthread_cond_init.o \
  pthread_cond_signal.o \
  pthread_cond_wait.o \
  pthread_condattr_destroy.o \
  pthread_condattr_getpshared.o \
  pthread_condattr_init.o \
  pthread_condattr_setpshared.o

RWLOCK_OBJS = \
  pthread_rwlock_init.o \
  pthread_rwlock_destroy.o \
  pthread_rwlock_rdlock.o \
  pthread_rwlock_timedrdlock.o \
  pthread_rwlock_timedwrlock.o \
  pthread_rwlock_tryrdlock.o \
  pthread_rwlock_trywrlock.o \
  pthread_rwlock_unlock.o \
  pthread_rwlock_wrlock.o \
  pthread_rwlockattr_init.o \
  pthread_rwlockattr_destroy.o \
  pthread_rwlockattr_getpshared.o \
  pthread_rwlockattr_setpshared.o \
  pte_rwlock_check_need_init.o \
  pte_rwlock_cancelwrwait.o 

CANCEL_OBJS = \
  pthread_cancel.o \
  pthread_setcanceltype.o \
  pthread_setcancelstate.o



OBJS = $(MUTEX_OBJS) $(MUTEXATTR_OBJS) $(THREAD_OBJS) $(SUPPORT_OBJS) $(TLS_OBJS) $(MISC_OBJS) $(SEM_OBJS) $(BARRIER_OBJS) $(SPIN_OBJS) $(CONDVAR_OBJS) $(RWLOCK_OBJS) $(CANCEL_OBJS)
obj-y +=$(OBJS) \
	$(NK_LIB) \

obj-$(NAUT_CONFIG_BASE_PTHREAD_COMPAT_TESTS) += pthreadtest/


