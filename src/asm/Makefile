obj-y += smpboot.o \
		 excp_early.o \
		 thread_lowlevel.o 

ifdef NAUT_CONFIG_XEON_PHI
obj-y += xeon_phi_boot.o
else
obj-y += boot.o
endif
