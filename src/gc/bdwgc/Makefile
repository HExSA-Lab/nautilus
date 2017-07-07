CFLAGS += -Isrc/gc/bdwgc/include \
	-DNAUT \
	-Ulinux \
	-U__linux__ \
	-U__GNU__ \
	-U__GLIBC__ \
	-DNO_CLOCK \
	-DSMALL_CONFIG \
	-DGC_DISABLE_INCREMENTAL \
	-DNO_GETCONTEXT \
	-DNO_DEBUGGING

obj-y += allchblk.o \
	alloc.o \
	backgraph.o \
	blacklst.o \
	checksums.o \
	dbg_mlc.o \
	finalize.o \
	gc_dlopen.o \
	gcj_mlc.o \
	headers.o \
	mach_dep.o \
	malloc.o \
	mallocx.o \
	mark.o \
	mark_rts.o \
	misc.o \
	new_hblk.o \
	obj_map.o \
	os_dep.o \
	ptr_chck.o \
	reclaim.o \
	specific.o \
	stubborn.o \
	thread_local_alloc.o \
	typd_mlc.o \
	naut_threads.o \
	naut_stop_world.o \
	libatomic_ops/ \
	bdwgc.o

obj-$(NAUT_CONFIG_TEST_BDWGC) += tests/ 
