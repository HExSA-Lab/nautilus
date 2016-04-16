obj-y += apic.o \
	 ioapic.o \
	 i8254.o \
	 timer.o \
	 kbd.o \
	 pci.o \
         vga.o \

ifndef NAUT_CONFIG_XEON_PHI
obj-y += serial.o
endif

obj-$(NAUT_CONFIG_HPET) += hpet.o

obj-$(NAUT_CONFIG_VIRTIO_PCI) += virtio_pci.o
