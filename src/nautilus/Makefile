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
	mtrr.o \
	fpu.o \
	spinlock.o \
	ticketlock.o \
	rwlock.o \
	condvar.o \
	semaphore.o \
	msg_queue.o \
	hashtable.o \
	rbtree.o \
	random.o \
	smp.o \
	idle.o \
	thread.o \
        task.o   \
        future.o  \
	waitqueue.o \
	group.o \
        timer.o \
        scheduler.o \
	group_sched.o \
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
        loader.o \
        shell.o \
	fprintk.o \
        dtoa.o \
	module.o \
	linker.o \
	prog.o \
	getopt.o \
	cmdline.o

obj-$(NAUT_CONFIG_PROFILE) += instrument.o
obj-$(NAUT_CONFIG_XEON_PHI) += sfi.o

obj-$(NAUT_CONFIG_PALACIOS) += vmm.o

obj-$(NAUT_CONFIG_REAL_MODE_INTERFACE) += realmode.o

obj-$(NAUT_CONFIG_ISOCORE) += isocore.o

obj-$(NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING) += gdb-stub.o

obj-$(NAUT_CONFIG_ENABLE_MONITOR) += monitor.o

obj-$(NAUT_CONFIG_WATCHDOG) +=  watchdog.o

obj-$(NAUT_CONFIG_CACHEPART) +=	cachepart.o

obj-$(NAUT_CONFIG_FIBER_ENABLE) += fiber.o

obj-$(NAUT_CONFIG_ASPACES) +=  aspace.o 

obj-$(NAUT_CONFIG_PARTITION_SUPPORT) += partition.o

