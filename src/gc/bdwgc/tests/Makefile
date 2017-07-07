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
obj-y += test.o \
	     huge_test.o \
         realloc_test.o \
         leak_test.o 
#		 setjmp_t.o
