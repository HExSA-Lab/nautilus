obj-y += apic.o \
	 ioapic.o \
	 i8254.o \
	 kbd.o \
	 pci.o \
         vga.o \

ifndef NAUT_CONFIG_XEON_PHI
obj-y += serial.o
endif

obj-$(NAUT_CONFIG_HPET) += hpet.o

obj-$(NAUT_CONFIG_VIRTIO_PCI) += virtio_pci.o

obj-$(NAUT_CONFIG_RAMDISK) += ramdisk.o

obj-$(NAUT_CONFIG_VESA) += vesa.o

