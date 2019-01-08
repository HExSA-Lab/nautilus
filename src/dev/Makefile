obj-y += apic.o \
	 ioapic.o \
	 i8254.o \
	 ps2.o \
	 pci.o \
         vga.o \

ifndef NAUT_CONFIG_XEON_PHI
obj-y += serial.o
endif

obj-$(NAUT_CONFIG_HPET) += hpet.o

obj-$(NAUT_CONFIG_VIRTIO_PCI) += virtio_pci.o
obj-$(NAUT_CONFIG_VIRTIO_NET) += virtio_net.o
obj-$(NAUT_CONFIG_VIRTIO_BLK) += virtio_blk.o

obj-$(NAUT_CONFIG_E1000_PCI) += e1000_pci.o
obj-$(NAUT_CONFIG_E1000E_PCI) += e1000e_pci.o
obj-$(NAUT_CONFIG_MLX3_PCI) += mlx3_ib.o

obj-$(NAUT_CONFIG_RAMDISK) += ramdisk.o

obj-$(NAUT_CONFIG_ATA) += ata.o

obj-$(NAUT_CONFIG_VESA) += vesa.o

ifdef NAUT_CONFIG_LOAD_LUA
obj-y +=lua_script.o
endif

obj-$(NAUT_CONFIG_GPIO) += gpio.o

