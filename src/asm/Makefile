obj-y += smpboot.o \
		 excp_early.o \
		 thread_lowlevel.o \
		 setjmp_low.o \
		 timing_loops.o \
		 string_lowlevel.o

obj-$(NAUT_CONFIG_FIBER_ENABLE) += fiber_lowlevel.o

ifdef NAUT_CONFIG_PALACIOS_EMBED_VM_IMG
	obj-y += guest.o
endif

ifdef NAUT_CONFIG_RAMDISK_EMBED
	obj-y += ramdisk.o
endif

ifdef NAUT_CONFIG_REAL_MODE_INTERFACE
	obj-y += realmode.o
endif
ifdef NAUT_CONFIG_LOAD_LUA
	obj-y +=lua_script.o
endif


ifdef NAUT_CONFIG_ISOCORE
	obj-y += isocore_lowlevel.o
endif
