obj-y += smpboot.o \
		 excp_early.o \
		 thread_lowlevel.o 

ifdef NAUT_CONFIG_XEON_PHI
obj-y += xeon_phi_boot.o
else
ifdef NAUT_CONFIG_HVM_HRT
obj-y += hrtboot.o
else
obj-y += boot.o
endif
endif
