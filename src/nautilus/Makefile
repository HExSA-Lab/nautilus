obj-y += \
	fmtout.o \
	printk.o \
	doprnt.o \
	vc.o \
	idt.o \
	queue.o \
	irq.o \
	mb_utils.o \
	paging.o \
	naut_string.o \
	msr.o \
	cpuid.o \
	fpu.o \
	spinlock.o \
	ticketlock.o \
	rwlock.o \
	condvar.o \
	hashtable.o \
	rbtree.o \
	random.o \
	smp.o \
	idle.o \
	thread.o \
        timer.o \
        scheduler.o \
	barrier.o \
	backtrace.o \
	cpu.o \
	acpi.o \
	numa.o \
	libccompat.o \
	nemo.o \
	pmc.o \
	setjmp.o \
	mm/ \
	dev.o \
	chardev.o \
	blkdev.o \
	netdev.o \
        shell.o

obj-$(NAUT_CONFIG_PROFILE) += instrument.o
obj-$(NAUT_CONFIG_XEON_PHI) += sfi.o

obj-$(NAUT_CONFIG_PALACIOS) += vmm.o


#obj-$(NAT_CONFIG_DEV) += dev.o 
#obj-$(NAT_CONFIG_CHARDEV) += chardev.o 
#obj-$(NAT_CONFIG_BLKDEV) += blkdev.o 
#obj-$(NAT_CONFIG_NETDEV) += netdev.o 

