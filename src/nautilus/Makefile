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
        fs.o \
        shell.o \
		fprintk.o 

obj-$(NAUT_CONFIG_PROFILE) += instrument.o
obj-$(NAUT_CONFIG_XEON_PHI) += sfi.o

obj-$(NAUT_CONFIG_PALACIOS) += vmm.o

obj-$(NAUT_CONFIG_REAL_MODE_INTERFACE) += realmode.o


