/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2016, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>
#include <nautilus/dev.h>
#include <nautilus/blkdev.h>
#include <nautilus/netdev.h>
#include <nautilus/chardev.h>
#include <nautilus/fs.h>
#include <nautilus/loader.h>
#include <nautilus/cpuid.h>
#include <nautilus/msr.h>
#include <nautilus/backtrace.h>
#include <test/ipi.h>
#include <test/threads.h>

#ifdef NAUT_CONFIG_PALACIOS
#include <nautilus/vmm.h>
#endif

#ifdef NAUT_CONFIG_REAL_MODE_INTERFACE 
#include <nautilus/realmode.h>
#endif

#ifdef NAUT_CONFIG_ISOCORE
#include <nautilus/isocore.h>
#endif

#define MAX_CMD 80

struct burner_args {
    struct nk_virtual_console *vc;
    char     name[MAX_CMD];
    uint64_t size_ns; 
    struct nk_sched_constraints constraints;
} ;

// enable this to flip a GPIO periodically within
// the main loop of test thread
#define GPIO_OUTPUT 0

#if GPIO_OUPUT
#define GET_OUT() inb(0xe010)
#define SET_OUT(x) outb(x,0xe010)
#else
#define GET_OUT() 
#define SET_OUT(x) 
#endif

#define SWITCH() SET_OUT(~GET_OUT())
#define LOOP() {SWITCH(); udelay(1000); }

static void burner(void *in, void **out)
{
    uint64_t start, end, dur;
    struct burner_args *a = (struct burner_args *)in;

    nk_thread_name(get_cur_thread(),a->name);

    if (nk_bind_vc(get_cur_thread(), a->vc)) { 
	ERROR_PRINT("Cannot bind virtual console for burner %s\n",a->name);
	return;
    }

    nk_vc_printf("%s (tid %llu) attempting to promote itself\n", a->name, get_cur_thread()->tid);
#if 1
    if (nk_sched_thread_change_constraints(&a->constraints)) { 
	nk_vc_printf("%s (tid %llu) rejected - exiting\n", a->name, get_cur_thread()->tid);
	return;
    }
#endif

    nk_vc_printf("%s (tid %llu) promotion complete - spinning for %lu ns\n", a->name, get_cur_thread()->tid, a->size_ns);

    while(1) {
	start = nk_sched_get_realtime();
	LOOP();
	end = nk_sched_get_realtime();
	dur = end - start;
	//	nk_vc_printf("%s (tid %llu) start=%llu, end=%llu left=%llu\n",a->name,get_cur_thread()->tid, start, end,a->size_ns);
	if (dur >= a->size_ns) { 
	    nk_vc_printf("%s (tid %llu) done - exiting\n",a->name,get_cur_thread()->tid);
	    free(in);
	    return;
	} else {
	    a->size_ns -= dur;
	}
    }
}

static int launch_aperiodic_burner(char *name, uint64_t size_ns, uint32_t tpr, uint64_t priority)
{
    nk_thread_id_t tid;
    struct burner_args *a;

    a = malloc(sizeof(struct burner_args));
    if (!a) { 
	return -1;
    }
    
    strncpy(a->name,name,MAX_CMD); a->name[MAX_CMD-1]=0;
    a->vc = get_cur_thread()->vc;
    a->size_ns = size_ns;
    a->constraints.type=APERIODIC;
    a->constraints.interrupt_priority_class = (uint8_t) tpr;
    a->constraints.aperiodic.priority=priority;

    if (nk_thread_start(burner, (void*)a , NULL, 1, PAGE_SIZE_4KB, &tid, -1)) { 
	free(a);
	return -1;
    } else {
	return 0;
    }
}

static int launch_sporadic_burner(char *name, uint64_t size_ns, uint32_t tpr, uint64_t phase, uint64_t size, uint64_t deadline, uint64_t aperiodic_priority)
{
    nk_thread_id_t tid;
    struct burner_args *a;

    a = malloc(sizeof(struct burner_args));
    if (!a) { 
	return -1;
    }
    
    strncpy(a->name,name,MAX_CMD); a->name[MAX_CMD-1]=0;
    a->vc = get_cur_thread()->vc;
    a->size_ns = size_ns;
    a->constraints.type=SPORADIC;
    a->constraints.interrupt_priority_class = (uint8_t) tpr;
    a->constraints.sporadic.phase = phase;
    a->constraints.sporadic.size = size;
    a->constraints.sporadic.deadline = deadline;
    a->constraints.sporadic.aperiodic_priority = aperiodic_priority;

    if (nk_thread_start(burner, (void*)a , NULL, 1, PAGE_SIZE_4KB, &tid, -1)) {
	free(a);
	return -1;
    } else {
	return 0;
    }
}

static int launch_periodic_burner(char *name, uint64_t size_ns, uint32_t tpr, uint64_t phase, uint64_t period, uint64_t slice)
{
    nk_thread_id_t tid;
    struct burner_args *a;

    a = malloc(sizeof(struct burner_args));
    if (!a) { 
	return -1;
    }
    
    strncpy(a->name,name,MAX_CMD); a->name[MAX_CMD-1]=0;
    a->vc = get_cur_thread()->vc;
    a->size_ns = size_ns;
    a->constraints.type=PERIODIC;
    a->constraints.interrupt_priority_class = (uint8_t) tpr;
    a->constraints.periodic.phase = phase;
    a->constraints.periodic.period = period;
    a->constraints.periodic.slice = slice;

    if (nk_thread_start(burner, (void*)a , NULL, 1, PAGE_SIZE_4KB, &tid, -1)) {
	free(a);
	return -1;
    } else {
	return 0;
    }
}


static int handle_cat(char *buf)
{
    char data[MAX_CMD];
    ssize_t ct, i;

    buf+=3;
    
    while (*buf && *buf==' ') { buf++;}
    
    if (!*buf) { 
	nk_vc_printf("No file requested\n");
	return 0;
    }

    nk_fs_fd_t fd = nk_fs_open(buf,O_RDONLY,0);

    if (FS_FD_ERR(fd)) { 
	nk_vc_printf("Cannot open \"%s\"\n",buf);
	return 0;
    }

    do {
	ct = nk_fs_read(fd, data, MAX_CMD);
	if (ct<0) {
	    nk_vc_printf("Error reading file\n");
	    nk_fs_close(fd);
	    return 0;
	}
	for (i=0;i<ct;i++) {
	    nk_vc_printf("%c",data[i]);
	}
    } while (ct>0);

    //    nk_fs_close(fd);
    
    return 0;
}

#ifdef NAUT_CONFIG_REAL_MODE_INTERFACE 
static int handle_real(char *cmd)
{
    struct nk_real_mode_int_args test;


    if ((nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx %hx %hx %hx:%hx", 
		&test.vector, &test.ax, &test.bx, &test.cx, &test.cx, &test.es, &test.di)==7) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx %hx %hx:%hx", 
		&test.vector, &test.ax, &test.bx, &test.cx, &test.es, &test.di)==6) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx %hx:%hx", 
		&test.vector, &test.ax, &test.bx, &test.es, &test.di)==5) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx:%hx", 
		&test.vector, &test.ax, &test.es, &test.di)==4) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx:%hx", 
		&test.vector, &test.ax, &test.es, &test.di)==3) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx %hx %hx", 
		&test.vector, &test.ax, &test.bx, &test.cx, &test.dx)==5) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx %hx", 
		&test.vector, &test.ax, &test.bx, &test.cx)==4) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx %hx", 
		&test.vector, &test.ax, &test.bx)==3) ||
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx %hx", 
		&test.vector, &test.ax)==2) ||	
	(nk_real_mode_set_arg_defaults(&test),
	 sscanf(cmd,"real %hx",
		&test.vector)==1)) {

	nk_vc_printf("Req: int %hx ax=%04hx bx=%04hx cx=%04hx dx=%04hx es:di=%04hx:%04hx\n",
		     test.vector, test.ax, test.bx, test.cx, test.dx, test.es, test.di);
    
	if (nk_real_mode_start()) { 
	    nk_vc_printf("start failed\n");
	    return -1;
	} else {
	    if (nk_real_mode_int(&test)) { 
		nk_vc_printf("int failed\n");
		nk_real_mode_finish();
		return -1;
	    } else {
		nk_vc_printf("Res: ax=%04hx bx=%04hx cx=%04hx dx=%04hx si=%04hx di=%04hx\n"
			     "     flags=%04hx cs=%04hx ds=%04hx ss=%04hx fs=%04hx gs=%04hx es=%04hx\n",
			     test.ax, test.bx, test.cx, test.dx, test.si, test.di,
			     test.flags, test.cs, test.ds, test.ss, test.fs, test.gs, test.es);
		nk_real_mode_finish();
		return 0;
	    }
	}
    } else {
	nk_vc_printf("Don't understand %s\n",cmd);
	return -1;
    }
}
#endif


static int handle_ipitest(char * buf)
{
	uint32_t trials, sid, did;

	ipi_exp_data_t * data = malloc(sizeof(ipi_exp_data_t));
	if (!data) {
		nk_vc_printf("ERROR: could not allocate IPI experiment data\n");
		return -1;
	}
	memset(data, 0, sizeof(ipi_exp_data_t));
	
	buf += 7;
	while (*buf && *buf==' ') { buf++;}

	if (!*buf) {
		nk_vc_printf("No test type given\n");
		return 0;
	}

	// get the experiment type (oneway, roundtrip, or broadcast)
	if (sscanf(buf, "oneway %u", &trials)==1) {
		data->type = EXP_ONEWAY;
		buf += 6;
	} else if (sscanf(buf, "roundtrip %u", &trials)==1) {
		data->type = EXP_ROUNDTRIP;
		buf += 9;
	} else if (sscanf(buf, "broadcast %u", &trials)==1) {
		data->type = EXP_BROADCAST;
		buf += 9;
	} else {
		nk_vc_printf("Unknown IPI test type\n");
		return 0;
	}

	data->trials = (trials > IPI_MAX_TRIALS) ? IPI_MAX_TRIALS : trials;

    buf++;

    // skip over trial count
    while (*buf && *buf!=' ') { buf++;}

    // find next arg
	while (*buf && *buf==' ') { buf++;}

    if (!strncasecmp(buf, "-f", 2)) {

#ifndef NAUT_CONFIG_EXT2_FILESYSTEM_DRIVER 
        nk_vc_printf("Not compiled with FS support, cannot use -f\n");
        return 0;
    }
#else
        char fbuf[IPI_MAX_FNAME_LEN];
        data->use_file = 1;
        buf += 2;

        // find next arg
        while (*buf && *buf==' ') { buf++;}

        if (sscanf(buf, "%s", fbuf)==1) {
            if (!strncasecmp(buf, "-", 1)) {
                nk_vc_printf("No filename given\n");
                return 0;
            }
            strncpy(data->fname, fbuf, IPI_MAX_FNAME_LEN);

            // skip over the filename
            while(*buf && *buf!=' ') {buf++;}

            // find next arg
            while (*buf && *buf==' ') {buf++;}

        } else {
            nk_vc_printf("No filename given\n");
            return 0;
        }
    }
#endif

    // which source type is it 
	if (sscanf(buf, "-s %u", &sid)==1) {
        data->src_type = SRC_ONE;
        data->src_core = sid; 
        buf += 3;

        // skip over src core
        while (*buf && *buf!=' ') { buf++;}

        // find next arg
        while (*buf && *buf==' ') { buf++;}

	} else { 
        data->src_type = SRC_ALL;
    }

        
    if (sscanf(buf, "-d %u", &did)==1) {
        data->dst_type = DST_ONE;
        data->dst_core = did;
    } else {
        data->dst_type = DST_ALL;
    }
		
	if (ipi_run_exps(data) != 0) {
        nk_vc_printf("Could not run ipi experiment\n");
        return 0;
    }

    free(data);

	return 0;
}


static int handle_blktest(char * buf)
{
    char name[32], rw[16];
    uint64_t start, count;
    struct nk_block_dev *d;
    struct nk_block_dev_characteristics c;

    if ((sscanf(buf,"blktest %s %s %lu %lu",name,rw,&start,&count)!=4)
	|| (*rw!='r' && *rw!='w') ) { 
	nk_vc_printf("Don't understand %s\n",buf);
	return -1;
    }

    if (!(d=nk_block_dev_find(name))) { 
	nk_vc_printf("Can't find %s\n",name);
	return -1;
    }

    if (nk_block_dev_get_characteristics(d,&c)) { 
	nk_vc_printf("Can't get characteristics of %s\n",name);
	return -1;
    }

    char data[c.block_size+1];
    uint64_t i,j;


    for (i=start;i<start+count;i++) { 
	if (*rw == 'w') { 
	    for (j=0;j<c.block_size;j++) { 
		data[j] = "abcdefghijklmnopqrstuvwxyz0123456789"[j%36];
	    }
	    if (nk_block_dev_write(d,i,1,data,NK_DEV_REQ_BLOCKING,0,0)) {
		nk_vc_printf("Failed to write block %lu\n",i);
		return -1;
	    }
	} else if (*rw == 'r') {
	    if (nk_block_dev_read(d,i,1,data,NK_DEV_REQ_BLOCKING,0,0)) {
		nk_vc_printf("Failed to read block %lu\n",i);
		return -1;
	    }
	    data[c.block_size] = 0;
	    nk_vc_printf("%s\n",data);
	}
    }
    return 0;
}

static int handle_test(char *buf)
{
    char what[80];
    
    if (sscanf(buf,"test %s",what)!=1) { 
	goto dunno;
    }
    
    if (!strncasecmp(what,"thread",6)) { 
	return test_threads();
    }

 dunno:
    nk_vc_printf("Unknown test request\n");
    return -1;
}
	
static int handle_attach(char * buf)
{
    char type[32], devname[32], fsname[32]; 
    uint64_t start, count;
    struct nk_block_dev *d;
    struct nk_block_dev_characteristics c;
    int rc;

    if (sscanf(buf,"attach %s %s %s",devname, type, fsname)!=3) {
	nk_vc_printf("Don't understand %s\n",buf);
	return -1;
    }

    if (!strcmp(type,"ext2")) { 
#ifndef NAUT_CONFIG_EXT2_FILESYSTEM_DRIVER 
        nk_vc_printf("Not compiled with EXT2 support, cannot attach\n");
        return -1;
#else
	if (nk_fs_ext2_attach(devname,fsname,0)) {
	    nk_vc_printf("Failed to attach %s as ext2 volume with name %s\n", devname,fsname);
	    return -1;
	} else {
	    nk_vc_printf("Device %s attached as ext2 volume with name %s\n", devname,fsname);
	    return 0;
	}
#endif
    } else {
	nk_vc_printf("FS type %s is not supported\n", type);
	return -1;
    }
}

static int handle_benchmarks(char * buf)
{
    extern void run_benchmarks();
    
    run_benchmarks();

    return 0;
}

#ifdef NAUT_CONFIG_ISOCORE

static void isotest(void *arg)
{
    // note trying to do anything in here with NK
    // features, even a print, is unlikely to work due to
    // relocation, interrupts off, etc.   
    //serial_print("Hello from isocore, my arg is %p\n", arg);
    serial_putchar('H');
    serial_putchar('I');
    serial_putchar('!');
    serial_putchar('\n');
    while (1) { }  // does actually get here in testing
}

static int handle_isotest(char *buf)
{
    void (*code)(void*) = isotest;
    uint64_t codesize = PAGE_SIZE_4KB; // we are making pretend here
    uint64_t stacksize = PAGE_SIZE_4KB;
    void *arg = (void*)0xdeadbeef;

    return nk_isolate(code, 
		      codesize,
		      stacksize,
		      arg);
}


#endif

static int handle_meminfo(char *buf)
{
    uint64_t num = kmem_num_pools();

    // nk_vc_printf("Number of pools=%lu\n",num);
    
    struct kmem_stats *s = malloc(sizeof(struct kmem_stats)+num*sizeof(struct buddy_pool_stats));

    if (!s) { 
	nk_vc_printf("Failed to allocate space for mem info\n");
	return 0;
    }

    s->max_pools = num;
    
    kmem_stats(s);

    
    uint64_t i;

    for (i=0;i<s->num_pools;i++) { 
	nk_vc_printf("pool %lu %p-%p %lu blks free %lu bytes free\n  %lu bytes min %lu bytes max\n", 
		     i,
		     s->pool_stats[i].start_addr,
		     s->pool_stats[i].end_addr,
		     s->pool_stats[i].total_blocks_free,
		     s->pool_stats[i].total_bytes_free,
		     s->pool_stats[i].min_alloc_size,
		     s->pool_stats[i].max_alloc_size);
    }

    nk_vc_printf("%lu pools %lu blks free %lu bytes free\n", s->total_num_pools, s->total_blocks_free, s->total_bytes_free);
    nk_vc_printf("  %lu bytes min %lu bytes max\n", s->min_alloc_size, s->max_alloc_size);

    free(s);
    
    return 0;
}

int handle_run(char *buf)
{
    char path[80];

    if (sscanf(buf,"run %s", path)!=1) { 
	nk_vc_printf("Can't determine what to run\n");
	return 0;
    }

    struct nk_exec *e = nk_load_exec(path);

    if (!e) { 
	nk_vc_printf("Can't load %s\n", path);
	return 0;
    }

    nk_vc_printf("Loaded executable, now running\n");
    
    if (nk_start_exec(e,0,0)) { 
	nk_vc_printf("Failed to run %s\n", path);
    }

    nk_vc_printf("Unloading executable\n");
    
    if (nk_unload_exec(e)) { 
	nk_vc_printf("Failed to unload %s\n",path);
    }

    return 0;
}    
	

static int handle_cmd(char *buf, int n)
{
  char name[MAX_CMD];
  uint64_t size_ns;
  uint32_t tpr;
  uint64_t priority, phase;
  uint64_t period, slice;
  uint64_t size, deadline;
  uint64_t addr, data, len;
  uint64_t tid;
  uint32_t id, idsub, sub;
  uint32_t msr;
  int cpu;
  char bwdq;

  if (*buf==0) { 
    return 0;
  }

  if (!strncasecmp(buf,"exit",4)) { 
    return 1;
  }

#ifdef NAUT_CONFIG_REAL_MODE_INTERFACE 
  if (!strncasecmp(buf,"real",4)) { 
    handle_real(buf);
    return 0;
  }
#endif

#ifdef NAUT_CONFIG_ISOCORE
  if (!strncasecmp(buf,"isotest",4)) { 
    handle_isotest(buf);
    return 0;
  }
#endif
  
  if (!strncasecmp(buf,"help",4)) { 
    nk_vc_printf("help\nexit\nvcs\ncores [n]\ntime [n]\nthreads [n]\n");
    nk_vc_printf("devs | fses | ofs | cat [path]\n");
    nk_vc_printf("shell name\n");
    nk_vc_printf("regs [t]\npeek [bwdq] x | mem x n [s] | poke [bwdq] x y\nin [bwd] addr | out [bwd] addr data\nrdmsr x [n] | wrmsr x y\ncpuid f [n] | cpuidsub f s\n");
    nk_vc_printf("meminfo [detail]\n");
    nk_vc_printf("reap\n");
    nk_vc_printf("burn a name size_ms tpr priority\n");
    nk_vc_printf("burn s name size_ms tpr phase size deadline priority\n");
    nk_vc_printf("burn p name size_ms tpr phase period slice\n");
    nk_vc_printf("real int [ax [bx [cx [dx]]]] [es:di]\n");
    nk_vc_printf("ipitest type (oneway | roundtrip | broadcast) trials [-f <filename>] [-s <src_id> | all] [-d <dst_id> | all]\n");
    nk_vc_printf("bench\n");
    nk_vc_printf("blktest dev r|w start count\n");
    nk_vc_printf("blktest dev r|w start count\n");
    nk_vc_printf("isotest\n");
    nk_vc_printf("test threads|...\n");
    nk_vc_printf("vm name [embedded image]\n");
    nk_vc_printf("run path\n");
    return 0;
  }

  if (!strncasecmp(buf,"vcs",3)) {
    nk_switch_to_vc_list();
    return 0;
  }

  if (!strncasecmp(buf,"devs",4)) {
    nk_dev_dump_devices();
    return 0;
  }

  if (!strncasecmp(buf,"fses",4)) {
    nk_fs_dump_filesystems();
    return 0;
  }

  if (!strncasecmp(buf,"ofs",3)) {
    nk_fs_dump_files();
    return 0;
  }

  if (!strncasecmp(buf,"cat",3)) {
    handle_cat(buf);
    return 0;
  }

  if (!strncasecmp(buf,"ipitest",7)) {
	handle_ipitest(buf);
	return 0;
  }

  if (!strncasecmp(buf,"bench",5)) {
	handle_benchmarks(buf);
	return 0;
  }

  if (!strncasecmp(buf,"blktest",7)) {
	handle_blktest(buf);
	return 0;
  }

  if (!strncasecmp(buf,"test",4)) {
      handle_test(buf);
      return 0;
  }


  if (!strncasecmp(buf,"attach",6)) {
	handle_attach(buf);
	return 0;
  }

  if (sscanf(buf,"shell %s", name)==1) { 
    nk_launch_shell(name,-1);
    return 0;
  }

  if (!strncasecmp(buf,"reap",4)) { 
      nk_sched_reap(1); // unconditional reap
    return 0;
  }

  if (sscanf(buf,"regs %lu",&tid)==1) { 
      nk_thread_t *t = nk_find_thread_by_tid(tid);
      if (!t) {
	  nk_vc_printf("No such thread\n");
      } else {
	  nk_print_regs((struct nk_regs *) t->rsp);
      }
      return 0;
  }

  if (!strncasecmp(buf,"regs",4)) {
      extern int nk_interrupt_like_trampoline(void (*)(struct nk_regs *));
      nk_interrupt_like_trampoline(nk_print_regs);
      return 0;
  }

  if (((bwdq='b', sscanf(buf,"peek b %lx", &addr))==1) ||
      ((bwdq='w', sscanf(buf,"peek w %lx", &addr))==1) ||
      ((bwdq='d', sscanf(buf,"peek d %lx", &addr))==1) ||
      ((bwdq='q', sscanf(buf,"peek q %lx", &addr))==1) ||
      ((bwdq='q', sscanf(buf,"peek %lx", &addr))==1)) {
      switch (bwdq) { 
      case 'b': 
	  data = *(uint8_t*)addr;       
	  nk_vc_printf("Mem[0x%016lx] = 0x%02lx\n",addr,data);
	  break;
      case 'w': 
	  data = *(uint16_t*)addr;       
	  nk_vc_printf("Mem[0x%016lx] = 0x%04lx\n",addr,data);
	  break;
      case 'd': 
	  data = *(uint32_t*)addr;       
	  nk_vc_printf("Mem[0x%016lx] = 0x%08lx\n",addr,data);
	  break;
      case 'q': 
	  data = *(uint64_t*)addr;       
	  nk_vc_printf("Mem[0x%016lx] = 0x%016lx\n",addr,data);
	  break;
      default:
	  nk_vc_printf("Unknown size requested\n",bwdq);
	  break;
      }
      return 0;
  }

  if (((bwdq='b', sscanf(buf,"in b %lx", &addr))==1) ||
      ((bwdq='w', sscanf(buf,"in w %lx", &addr))==1) ||
      ((bwdq='d', sscanf(buf,"in d %lx", &addr))==1) ||
      ((bwdq='b', sscanf(buf,"in %lx", &addr))==1)) {
      addr &= 0xffff; // 16 bit address space
      switch (bwdq) { 
      case 'b': 
	  data = (uint64_t) inb(addr);
	  nk_vc_printf("IO[0x%04lx] = 0x%02lx\n",addr,data);
	  break;
      case 'w': 
	  data = (uint64_t) inw(addr);
	  nk_vc_printf("IO[0x%04lx] = 0x%04lx\n",addr,data);
	  break;
      case 'd': 
	  data = (uint64_t) inl(addr);
	  nk_vc_printf("IO[0x%04lx] = 0x%08lx\n",addr,data);
	  break;
      default:
	  nk_vc_printf("Unknown size requested\n",bwdq);
	  break;
      }
      return 0;
  }

#define BYTES_PER_LINE 16

  if ((sscanf(buf, "mem %lx %lu %lu",&addr,&len,&size)==3) ||
      (size=8, sscanf(buf, "mem %lx %lu", &addr, &len)==2)) { 
      uint64_t i,j,k;
      for (i=0;i<len;i+=BYTES_PER_LINE) {
	  nk_vc_printf("%016lx :",addr+i);
	  for (j=0;j<BYTES_PER_LINE && (i+j)<len; j+=size) {
	      nk_vc_printf(" ");
	      for (k=0;k<size;k++) { 
		  nk_vc_printf("%02x", *(uint8_t*)(addr+i+j+k));
	      }
	  }
	  nk_vc_printf(" ");
	  for (j=0;j<BYTES_PER_LINE && (i+j)<len; j+=size) {
	      for (k=0;k<size;k++) { 
		  nk_vc_printf("%c", isalnum(*(uint8_t*)(addr+i+j+k)) ? 
			       *(uint8_t*)(addr+i+j+k) : '.');
	      }
	  }
	  nk_vc_printf("\n");
      }	      

      return 0;
  }

  if (((bwdq='b', sscanf(buf,"poke b %lx %lx", &addr,&data))==2) ||
      ((bwdq='w', sscanf(buf,"poke w %lx %lx", &addr,&data))==2) ||
      ((bwdq='d', sscanf(buf,"poke d %lx %lx", &addr,&data))==2) ||
      ((bwdq='q', sscanf(buf,"poke q %lx %lx", &addr,&data))==2) ||
      ((bwdq='q', sscanf(buf,"poke %lx %lx", &addr, &data))==2)) {
      switch (bwdq) { 
      case 'b': 
	  *(uint8_t*)addr = data;
	  nk_vc_printf("Mem[0x%016lx] = 0x%02lx\n",addr,data);
	  break;
      case 'w': 
	  *(uint16_t*)addr = data;
	  nk_vc_printf("Mem[0x%016lx] = 0x%04lx\n",addr,data);
	  break;
      case 'd': 
	  *(uint32_t*)addr = data;
	  nk_vc_printf("Mem[0x%016lx] = 0x%08lx\n",addr,data);
	  break;
      case 'q': 
	  *(uint64_t*)addr = data;
	  nk_vc_printf("Mem[0x%016lx] = 0x%016lx\n",addr,data);
	  break;
      default:
	  nk_vc_printf("Unknown size requested\n");
	  break;
      }
      return 0;
  }

  if (((bwdq='b', sscanf(buf,"out b %lx %lx", &addr,&data))==2) ||
      ((bwdq='w', sscanf(buf,"out w %lx %lx", &addr,&data))==2) ||
      ((bwdq='d', sscanf(buf,"out d %lx %lx", &addr,&data))==2) ||
      ((bwdq='q', sscanf(buf,"out %lx %lx", &addr, &data))==2)) {
      addr &= 0xffff;
      switch (bwdq) { 
      case 'b': 
	  data &= 0xff;
	  outb((uint8_t) data, (uint16_t)addr);
	  nk_vc_printf("IO[0x%04lx] = 0x%02lx\n",addr,data);
	  break;
      case 'w': 
	  data &= 0xffff;
	  outw((uint16_t) data, (uint16_t)addr);
	  nk_vc_printf("IO[0x%04lx] = 0x%04lx\n",addr,data);
	  break;
      case 'd': 
	  data &= 0xffffffff;
	  outl((uint32_t) data, (uint16_t)addr);
	  nk_vc_printf("IO[0x%04lx] = 0x%08lx\n",addr,data);
      default:
	  nk_vc_printf("Unknown size requested\n");
	  break;
      }
      return 0;
  }


  if ((sscanf(buf,"rdmsr %x %lu", &msr, &size)==2) ||
      (size=1, sscanf(buf,"rdmsr %x", &msr)==1)) {
      uint64_t i,k;
      for (i=0;i<size;i++) { 
	  data = msr_read(msr+i);
	  nk_vc_printf("MSR[0x%08x] = 0x%016lx ",msr+i,data);
	  for (k=0;k<8;k++) { 
	      nk_vc_printf("%02x",*(k + (uint8_t*)&data));
	  }
	  nk_vc_printf(" ");
	  for (k=0;k<8;k++) { 
	      nk_vc_printf("%c",isalnum(*(k + (uint8_t*)&data)) ?
			   (*(k + (uint8_t*)&data)) : '.');
	  }
	  nk_vc_printf("\n");
      }
      return 0;
  }

  if (sscanf(buf, "wrmsr %x %lx",&msr,&data)==2) { 
      msr_write(msr,data);
      nk_vc_printf("MSR[0x%08x] = 0x%016lx\n",msr,data);
      return 0;
  }

  if ((sub=0, sscanf(buf,"cpuid %x %lu", &id, &size)==2) ||
      (size=1, sub=0, sscanf(buf,"cpuid %x",&id)==1) ||
      (size=1, sub=1, sscanf(buf,"cpuidsub %x %x",&id,&idsub)==2)) {
      uint64_t i,j,k;
      cpuid_ret_t r;
      uint32_t val[4];
      
      for (i=0;i<size;i++) {
	  if (sub) { 
	      cpuid_sub(id,idsub,&r);
	      nk_vc_printf("CPUID[0x%08x, 0x%08x] =",id+i,idsub);
	  } else {
	      cpuid(id+i,&r);
	      nk_vc_printf("CPUID[0x%08x] =",id+i);
	  }
	  val[0]=r.a; val[1]=r.b; val[2]=r.c; val[3]=r.d;
	  for (j=0;j<4;j++) {
	      nk_vc_printf(" ");
	      for (k=0;k<4;k++) { 
		  nk_vc_printf("%02x",*(k + (uint8_t*)&(val[j])));
	      }
	  }
	  for (j=0;j<4;j++) {
	      nk_vc_printf(" ");
	      for (k=0;k<4;k++) { 
		  nk_vc_printf("%c",isalnum(*(k + (uint8_t*)&(val[j]))) ?
			       (*(k + (uint8_t*)&(val[j]))) : '.');
	      }
	  }
	  nk_vc_printf("\n");
      }
      return 0;
  }

  if (sscanf(buf,"burn a %s %llu %u %llu", name, &size_ns, &tpr, &priority)==4) { 
    nk_vc_printf("Starting aperiodic burner %s with tpr %u, size %llu ms.and priority %llu\n",name,size_ns,priority);
    size_ns *= 1000000;
    launch_aperiodic_burner(name,size_ns,tpr,priority);
    return 0;
  }

  if (sscanf(buf,"burn s %s %llu %u %llu %llu %llu %llu", name, &size_ns, &tpr, &phase, &size, &deadline, &priority)==7) { 
    nk_vc_printf("Starting sporadic burner %s with size %llu ms tpr %u phase %llu from now size %llu ms deadline %llu ms from now and priority %lu\n",name,size_ns,tpr,phase,size,deadline,priority);
    size_ns *= 1000000;
    phase   *= 1000000; 
    size    *= 1000000;
    deadline*= 1000000; deadline+= nk_sched_get_realtime();
    launch_sporadic_burner(name,size_ns,tpr,phase,size,deadline,priority);
    return 0;
  }

  if (sscanf(buf,"burn p %s %llu %u %llu %llu %llu", name, &size_ns, &tpr, &phase, &period, &slice)==6) { 
    nk_vc_printf("Starting periodic burner %s with size %llu ms tpr %u phase %llu from now period %llu ms slice %llu ms\n",name,size_ns,tpr,phase,period,slice);
    size_ns *= 1000000;
    phase   *= 1000000; 
    period  *= 1000000;
    slice   *= 1000000;
    launch_periodic_burner(name,size_ns,tpr,phase,period,slice);
    return 0;
  }

#ifdef NAUT_CONFIG_PALACIOS_EMBED
  if (sscanf(buf,"vm %s", name)==1) { 
    extern int guest_start;
    nk_vmm_start_vm(name,&guest_start,0xffffffff);
    return 0;
  }
#endif

  if (!strncasecmp(buf,"run",3)) {
    handle_run(buf);
    return 0;
  }

  if (!strncasecmp(buf,"threads",7)) { 
    if (sscanf(buf,"threads %d",&cpu)!=1) {
      cpu=-1; 
    }
    nk_sched_dump_threads(cpu);
    return 0;
  }

  if (!strncasecmp(buf,"cores",5)) { 
    if (sscanf(buf,"cores %d",&cpu)!=1) {
      cpu=-1; 
    }
    nk_sched_dump_cores(cpu);
    return 0;
  }

  if (!strncasecmp(buf,"time",4)) { 
    if (sscanf(buf,"time %d",&cpu)!=1) {
      cpu=-1; 
    }
    nk_sched_dump_time(cpu);
    return 0;
  }

  if (!strncasecmp(buf,"memi",4)) {
      handle_meminfo(buf);
      return 0;
  }

  nk_vc_printf("Don't understand \"%s\"\n",buf);
  return 0;
}

static void shell(void *in, void **out)
{
  struct nk_virtual_console *vc = nk_create_vc((char*)in,COOKED, 0x9f, 0, 0);
  char buf[MAX_CMD];
  char lastbuf[MAX_CMD];
  int first=1;

  if (!vc) { 
    ERROR_PRINT("Cannot create virtual console for shell\n");
    return;
  }

  if (nk_thread_name(get_cur_thread(),(char*)in)) {   
    ERROR_PRINT("Cannot name shell's thread\n");
    return;
  }

  if (nk_bind_vc(get_cur_thread(), vc)) { 
    ERROR_PRINT("Cannot bind virtual console for shell\n");
    return;
  }
   
  nk_switch_to_vc(vc);
  
#define PROMPT 0xcf 
#define INPUT  0x3f
#define OUTPUT 0x9f

  nk_vc_clear(OUTPUT);
  nk_vc_setattr(OUTPUT);
   
  while (1) {  
    nk_vc_setattr(PROMPT);
    nk_vc_printf("%s> ", (char*)in);
    nk_vc_setattr(INPUT);
    nk_vc_gets(buf,MAX_CMD,1);
    nk_vc_setattr(OUTPUT);

    if (buf[0]==0 && !first) { 
	// continue; // turn off autorepeat for now
	if (handle_cmd(lastbuf,MAX_CMD)) { 
	    break;
	}
    } else {
	if (handle_cmd(buf,MAX_CMD)) {
	    break;
	}
	memcpy(lastbuf,buf,MAX_CMD);
	first=0;

    }
	       
  }

  nk_vc_printf("Exiting shell %s\n", (char*)in);
  free(in);
  nk_release_vc(get_cur_thread());

  // exit thread
  
}

nk_thread_id_t nk_launch_shell(char *name, int cpu)
{
  nk_thread_id_t tid;
  char *n = malloc(32);

  if (!n) {
    return 0;
  }

  strncpy(n,name,32);
  n[31]=0;
  
  if (nk_thread_start(shell, (void*)n, 0, 1, PAGE_SIZE_4KB, &tid, cpu)) { 
      free(n);
      return 0;
  } else {
      INFO_PRINT("Shell %s launched on cpu %d as %p\n",name,cpu,tid);
      return tid;
  }
}



