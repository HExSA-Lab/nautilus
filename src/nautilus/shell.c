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
#include <nautilus/libccompat.h>
#include <nautilus/vc.h>
#include <nautilus/dev.h>
#include <nautilus/blkdev.h>
#include <nautilus/netdev.h>
#include <nautilus/chardev.h>
#include <nautilus/semaphore.h>
#include <nautilus/msg_queue.h>
#include <nautilus/timer.h>
#include <nautilus/fs.h>
#include <nautilus/loader.h>
#include <nautilus/cpuid.h>
#include <nautilus/msr.h>
#include <nautilus/mtrr.h>
#include <nautilus/backtrace.h>
#include <nautilus/pmc.h>
#include <nautilus/linker.h>
#include <nautilus/prog.h>
#include <dev/gpio.h>
#include <dev/pci.h>
#include <dev/apic.h>
#include <test/ipi.h>
#include <test/threads.h>
#include <test/groups.h>
#include <test/tasks.h>
#include <test/net_udp_echo.h>

#ifdef NAUT_CONFIG_PALACIOS
#include <nautilus/vmm.h>
#endif

#ifdef NAUT_CONFIG_REAL_MODE_INTERFACE 
#include <nautilus/realmode.h>
#endif

#ifdef NAUT_CONFIG_ISOCORE
#include <nautilus/isocore.h>
#endif

#ifdef NAUT_CONFIG_CACHEPART
#include <nautilus/cachepart.h>
#ifdef NAUT_CONFIG_TEST_CACHEPART
#include <test/cachepart.h>
#endif
#endif

#ifdef NAUT_CONFIG_ENABLE_BDWGC
#include <gc/bdwgc/bdwgc.h>
#endif

#ifdef NAUT_CONFIG_ENABLE_PDSGC
#include <gc/pdsgc/pdsgc.h>
#endif

#ifdef NAUT_CONFIG_LOAD_LUA
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
#include <dev/lua_script.h>
#endif

#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING
#include <nautilus/gdb-stub.h>
#endif

#ifdef NAUT_CONFIG_PROFILE
#include <nautilus/instrument.h>
#endif


#ifdef NAUT_CONFIG_OMP_RT_TESTS
#include <test/test_omp.h>
#endif

#ifdef NAUT_CONFIG_NDPC_RT_TESTS
#include <test/test_ndpc.h>
#endif

#ifdef NAUT_CONFIG_NESL_RT
#include <rt/nesl/nesl.h>
#endif

#ifdef NAUT_CONFIG_NESL_RT_TESTS
#include <test/test_nesl.h>
#endif


#ifdef NAUT_CONFIG_NET_ETHERNET
#include <net/ethernet/ethernet_agent.h>
#include <net/ethernet/ethernet_arp.h>
#endif

#ifdef NAUT_CONFIG_NET_LWIP
#include <net/lwip/lwip.h>
#endif


#define MAX_CMD 80

#define SHELL_STACK_SIZE (PAGE_SIZE_2MB) 

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
	NK_GPIO_OUTPUT_MASK(0x1, GPIO_XOR);
	start = nk_sched_get_realtime();
	//LOOP();
	udelay(10);
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

    if (nk_thread_start(burner, (void*)a , NULL, 1, PAGE_SIZE_4KB, &tid, 1)) { 
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

    if (nk_thread_start(burner, (void*)a , NULL, 1, PAGE_SIZE_4KB, &tid, 1)) {
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

    if (nk_thread_start(burner, (void*)a , NULL, 1, PAGE_SIZE_4KB, &tid, 1)) {
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

static int test_stop()
{
    int i;

#define N 16
    
    for (i=0;i<N;i++) { 
	nk_vc_printf("Stopping world iteration %d\n",i);
	nk_sched_stop_world();
	nk_vc_printf("Executing during stopped world iteration %d\n",i);
	nk_sched_start_world();
    }

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

static int test_iso()
{
    void (*code)(void*) = isotest;
    uint64_t codesize = PAGE_SIZE_4KB; // we are making pretend here
    uint64_t stacksize = PAGE_SIZE_4KB;
    void *arg = (void*)0xdeadbeef;

    nk_vc_printf("Testing isolated core - this will not return!\n");

    return nk_isolate(code, 
		      codesize,
		      stacksize,
		      arg);
}


#endif

#ifdef NAUT_CONFIG_GARBAGE_COLLECTION
static int handle_collect(char *buf)
{
#ifdef NAUT_CONFIG_ENABLE_BDWGC
    nk_vc_printf("Doing BDWGC global garbage collection\n");
    int rc = nk_gc_bdwgc_collect();
    nk_vc_printf("BDWGC global garbage collection done result: %d\n",rc);
    return 0;
#else
#ifdef NAUT_CONFIG_ENABLE_PDSGC
    nk_vc_printf("Doing PDSGC global garbage collection\n");
    struct nk_gc_pdsgc_stats s;
    int rc = nk_gc_pdsgc_collect(&s);
    nk_vc_printf("PDSGC global garbage collection done result: %d\n",rc);
    nk_vc_printf("%lu blocks / %lu bytes freed\n",
		 s.num_blocks, s.total_bytes);
    nk_vc_printf("smallest freed block: %lu bytes, largest freed block: %lu bytes\n",
		 s.min_block, s.max_block);
    return 0;
#else 
    nk_vc_printf("No garbage collector is enabled...\n");
    return 0;
#endif
#endif
}

static int handle_leaks(char *buf)
{
#ifdef NAUT_CONFIG_ENABLE_BDWGC
    nk_vc_printf("Leak detection not available for BDWGC\n");
    return 0;
#else
#ifdef NAUT_CONFIG_ENABLE_PDSGC
    nk_vc_printf("Doing PDSGC global leak detection\n");
    struct nk_gc_pdsgc_stats s;
    int rc = nk_gc_pdsgc_leak_detect(&s);
    nk_vc_printf("PDSGC global leak detection done result: %d\n",rc);
    nk_vc_printf("%lu blocks / %lu bytes have leaked\n",
		 s.num_blocks, s.total_bytes);
    nk_vc_printf("smallest leaked block: %lu bytes, largest leaked block: %lu bytes\n",
		 s.min_block, s.max_block);
    return 0;
#else 
    nk_vc_printf("No garbage collector is enabled...\n");
    return 0;
#endif
#endif
}
#endif

static int handle_test(char *buf)
{
    char what[80];
    
    if (sscanf(buf,"test %s",what)!=1) { 
	goto dunno;
    }

#ifdef NAUT_CONFIG_OMP_RT_TESTS
    if (!strncasecmp(what,"ompb",4)) { 
	return test_ompbench();
    }

    if (!strncasecmp(what,"omp",3)) { 
	return test_omp();
    }
#endif

#ifdef NAUT_CONFIG_NDPC_RT_TESTS
    if (!strncasecmp(what,"ndpc",4)) { 
	return test_ndpc();
    }
#endif

#ifdef NAUT_CONFIG_NESL_RT_TESTS
    if (!strncasecmp(what,"nesl",4)) {
	return test_nesl();
    }
#endif

    if (!strncasecmp(what,"thread",6)) { 
	return test_threads();
    }

    if (!strncasecmp(what,"group",5)) {
        return nk_thread_group_test();
    }

    if (!strncasecmp(what,"task",4)) {
        return test_tasks();
    }

    if (!strncasecmp(what,"stop",4)) { 
	return test_stop();
    }

#ifdef NAUT_CONFIG_ISOCORE
    if (!strncasecmp(what,"iso",3)) { 
	test_iso();
	return 0;
    }
#endif

    int pmc_id;
    if (sscanf(buf, "test pmc %d", &pmc_id) == 1) {
        test_pmc(pmc_id);
        return 0;
    }

#ifdef NAUT_CONFIG_TEST_BDWGC
    if (!strncasecmp(what,"bdwgc",5)) { 
	nk_vc_printf("Testing BDWGC garbage collector\n");
	return nk_gc_bdwgc_test();
    }
#endif

#ifdef NAUT_CONFIG_ENABLE_PDSGC
    if (!strncasecmp(what,"pdsgc",5)) { 
	nk_vc_printf("Testing PDSGC garbage collector\n");
	return nk_gc_pdsgc_test();
    }
#endif

#ifdef NAUT_CONFIG_TEST_CACHEPART
    uint64_t size, iteration;
    int percent, flag, num_threads, shared;

    if (sscanf(buf, "test cachepart %lu %lu %d %d %d", &size, &iteration, &percent, &num_threads, &shared) == 5) {
      nk_vc_printf("Testing cache partitioning with multiple threads\n");
      test_cat_multi_threads(size, iteration, percent, num_threads, shared);
      return 0;
    }

    if (sscanf(buf, "test cachepart %lu %lu %d", &size, &iteration, &percent) == 3) {
      nk_vc_printf("Testing cache partitioning with a single thread\n");
      test_cat_single_thread(size, iteration, percent);
      return 0;
    }

#endif

    char nic[80];
    char ip[80];
    uint32_t port, num;
    
    if (sscanf(buf,"test udp_echo %s %s %u %u",nic,ip,&port,&num)==4) { 
	nk_vc_printf("Testing udp echo server\n");
	test_net_udp_echo(nic,ip,port,num);
	return 0;
    } 

    if (sscanf(buf,"test udp_echo %s",nic)==1) { 
	nk_vc_printf("Testing udp echo server\n");
	test_net_udp_echo(nic,"10.10.10.10",5000,20);
	return 0;
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

int handle_ioapic(char *buf)
{
    uint32_t num, pin;
    struct sys_info *sys = &nk_get_nautilus_info()->sys;
    struct ioapic *io;
    char all[80];
    char what[80];
    int mask=0;

    if (sscanf(buf,"ioapic %u %s %s",&num,all,what)==3) { 
	if (num>=sys->num_ioapics) { 
	    nk_vc_printf("unknown ioapic\n");
	    return 0;
	}
	if (what[0]!='m' && what[0]!='u') { 
	    nk_vc_printf("unknown ioapic request (mask|unmask)\n");
	    return 0;
	}
	mask = what[0]=='m';
	
	if (all[0]!='a') { 
	    if (sscanf(all,"%u",&pin)!=1) { 
		nk_vc_printf("unknown ioapic request (pin|all)\n");
		return 0;
	    }
	    if (mask) { 
		nk_vc_printf("masking ioapic %u pin %u\n",num,pin);
		ioapic_mask_irq(sys->ioapics[num], pin);
	    } else {
		nk_vc_printf("unmasking ioapic %u pin %u\n",num,pin);
		ioapic_unmask_irq(sys->ioapics[num], pin);
	    }
	} else {
	    for (pin=0;pin<sys->ioapics[num]->num_entries;pin++) { 
		if (mask) { 
		    nk_vc_printf("masking ioapic %u pin %u\n",num,pin);
		    ioapic_mask_irq(sys->ioapics[num], pin);
		} else {
		    nk_vc_printf("unmasking ioapic %u pin %u\n",num,pin);
		    ioapic_unmask_irq(sys->ioapics[num], pin);
		}
	    }
	}
	return 0;
    }

    if (sscanf(buf,"ioapic %s",what)==1) { 
	if (what[0]=='l' || what[0]=='d') { 
	    for (num=0;num<sys->num_ioapics;num++) {
		io = sys->ioapics[num];
		nk_vc_printf("ioapic %u: id=%u %u pins address=%p\n",
			     num, io->id, io->num_entries, io->base);
		if (what[0]=='d') { 
		    uint64_t entry;
		    for (pin=0;pin<io->num_entries;pin++) { 
			entry = (uint64_t) ioapic_read_reg(io, 0x10 + 2*pin);
			entry |= ((uint64_t) ioapic_read_reg(io, 0x10 + 2*pin+1));

			nk_vc_printf("  pin %2u -> %016lx (dest 0x%lx mask %lu vec 0x%lx%s)\n", 
				     pin, entry, (entry>>56)&0xffLU, (entry>>16)&0x1LU,
				     entry&0xffLU, (entry&0xffLU) == 0xf7 ? " panic" : "");
		    }
		}
	    }
	    return 0;
	} else {
	    nk_vc_printf("Unknown ioapic request\n");
	    return 0;
	}				       
    }
    
    nk_vc_printf("unknown ioapic request\n");
    return 0;
}

	


int handle_pci(char *buf)
{
  int bus, slot, func, off;
  uint64_t data;
  char bwdq; 

  if (strncmp(buf,"pci l",5)==0) { 
    pci_dump_device_list();
    return 0;
  }

  if (sscanf(buf,"pci raw %x %x %x", &bus, &slot, &func)==3) { 
    int i,j;
    uint32_t v;
    for (i=0;i<256;i+=32) {
      nk_vc_printf("%02x:", i);
      for (j=0;j<8;j++) {
	v = pci_cfg_readl(bus,slot,func,i+j*4);
	nk_vc_printf(" %08x",v);
      } 
      nk_vc_printf("\n");
    }
    return 0;
  }

  if (sscanf(buf,"pci dev %x %x %x", &bus, &slot, &func)==3) { 
    pci_dump_device(pci_find_device(bus,slot,func));
    return 0;
  }



  if (!strncmp(buf,"pci dev",7)) {
    pci_dump_devices();
    return 0;
  }

  if (((bwdq='w', sscanf(buf,"pci poke w %x %x %x %x %x", &bus, &slot, &func, &off,&data))==5) ||
      ((bwdq='d', sscanf(buf,"pci poke d %x %x %x %x %x", &bus, &slot, &func, &off,&data))==5) ||
      ((bwdq='d', sscanf(buf,"pci poke %x %x %x %x %x", &bus, &slot, &func, &off,&data))==5)) {
      if (bwdq=='w') { 
	  pci_cfg_writew(bus,slot,func,off,(uint16_t)data);
	  nk_vc_printf("PCI[%x:%x.%x:%x] = 0x%04lx\n",bus,slot,func,off,(uint16_t)data);
      } else {
	  pci_cfg_writel(bus,slot,func,off,(uint32_t)data);
	  nk_vc_printf("PCI[%x:%x.%x:%x] = 0x%08lx\n",bus,slot,func,off,(uint32_t)data);
      }
      return 0;
  }

  if (((bwdq='w', sscanf(buf,"pci peek w %x %x %x %x", &bus, &slot, &func, &off))==4) ||
      ((bwdq='d', sscanf(buf,"pci peek d %x %x %x %x", &bus, &slot, &func, &off))==4) ||
      ((bwdq='d', sscanf(buf,"pci peek %x %x %x %x", &bus, &slot, &func, &off))==4)) {
      if (bwdq=='w') { 
	  data = pci_cfg_readw(bus,slot,func,off);
	  nk_vc_printf("PCI[%x:%x.%x:%x] = 0x%04lx\n",bus,slot,func,off,(uint16_t)data);
      } else {
	  data = pci_cfg_readl(bus,slot,func,off);
	  nk_vc_printf("PCI[%x:%x.%x:%x] = 0x%08lx\n",bus,slot,func,off,(uint32_t)data);
      }
      return 0;
  }

  if (sscanf(buf,"pci cfg %x %x %x", &bus, &slot, &func)) {
      int i,j;
      uint8_t data[256];
      for (i=0;i<256;i+=4) {
	  *(uint32_t*)(&data[i]) = pci_cfg_readl(bus,slot,func,i);
      }
      for (i=0;i<256;i+=16) {
	  nk_vc_printf("PCI[%x:%x.%x].cfg[%02x] = ",bus,slot,func,i);
	  for (j=i;j<(i+16);j++) {
	      nk_vc_printf(" %02x",data[j]);
	  }
	  nk_vc_printf("\n");
      }
      return 0;
  }

  nk_vc_printf("unknown pci command\n");

  return -1;
}    
	
#ifdef NAUT_CONFIG_PROFILE
int handle_instrument(char *buf)
{
    char what[80];

    if (sscanf(buf,"inst%s* %s", what)==1) { 
	if (!strncasecmp(what,"sta",3)) {
	    nk_vc_printf("starting instrumentation\n");
	    nk_instrument_start();
	    return 0;
	} else if (!strncasecmp(what,"e",1) || !strncasecmp(what,"sto",3)) {
	    nk_vc_printf("ending instrumentation\n");
	    nk_instrument_end();
	    return 0;
	} else if (!strncasecmp(what,"c",1)) {
	    nk_vc_printf("clearing instrumentation\n");
	    nk_instrument_clear();
	    return 0;
	} else if (!strncasecmp(what,"q",1)) {
	    nk_vc_printf("querying instrumentation\n");
	    nk_instrument_query();
	    return 0;
	} 
    }
    nk_vc_printf("unknown instrumentation request\n");
    return 0;
}
#endif

	
#ifdef NAUT_CONFIG_GPIO
int handle_gpio(char *buf)
{
    char what[80];
    uint64_t val;
    uint64_t i;
    int set = nk_gpio_cpu_mask_check(my_cpu_id());

    if ((sscanf(buf,"gpio %s %lx", what, &val)==2) && what[0]=='s') {
	nk_vc_printf("sweeping gpio output from 0 to %lx with ~10 us delay\n",val);
	if (!set) {
	    nk_gpio_cpu_mask_add(my_cpu_id());
	}
	for (i=0;i<val;i++) { 
	    nk_gpio_output_set(i);
	    udelay(10);
	}
	if (!set) {
	    nk_gpio_cpu_mask_remove(my_cpu_id());
	}
	return 0;
    }

    if ((sscanf(buf,"gpio %s %lx", what, &val)==2) && what[0]=='o') {
	nk_vc_printf("setting gpio output to %lx\n",val);
	if (!set) {
	    nk_gpio_cpu_mask_add(my_cpu_id());
	}
	nk_gpio_output_set(val);
	if (!set) {
	    nk_gpio_cpu_mask_remove(my_cpu_id());
	}
	return 0;
    }

    if ((sscanf(buf,"gpio %s",what)==1) && what[0]=='i') {
	val = nk_gpio_input_get();
	nk_vc_printf("gpio input is %llx\n",val);
	return 0;
    }

    nk_vc_printf("unknown gpio request\n");

    return 0;
}
#endif    

static int handle_net(char *buf)
{
    char agentname[100], intname[100];
    uint32_t type;
    uint32_t ip[4];
    uint32_t gw[4];
    uint32_t netmask[4];
    uint32_t dns[4];
    int defaults=0;

#ifdef NAUT_CONFIG_NET_ETHERNET
    if (sscanf(buf,"net agent create %s %s",agentname, intname)==2) {
	nk_vc_printf("Attempt to create ethernet agent %s for device %s\n",agentname, intname);
	struct nk_net_dev *netdev = nk_net_dev_find(intname);
	struct nk_net_ethernet_agent *agent;
      
	if (!netdev) {
	    nk_vc_printf("Cannot find device %s\n",intname);
	    return 0;
	}

	agent = nk_net_ethernet_agent_create(netdev, agentname, 64, 64);

	if (!agent) {
	    nk_vc_printf("Cannot create agent %s\n",agentname);
	    return 0;
	}

	nk_vc_printf("agent created\n");
	return 0;
    }
      

    if (sscanf(buf,"net agent add %s %x",agentname, &type)==2) {
	nk_vc_printf("Attempt to create interface for agent %s type %us\n", agentname,type);
	struct nk_net_ethernet_agent *agent = nk_net_ethernet_agent_find(agentname);
	struct nk_net_dev *netdev;
	if (!agent) {
	    nk_vc_printf("Cannot find agent %s\n", agentname);
	    return 0;
	}
	netdev = nk_net_ethernet_agent_register_type(agent,type);
	if (!netdev) {
	    nk_vc_printf("Failed to register for type\n");
	    return 0;
	}
	nk_vc_printf("New netdev is named: %s\n", netdev->dev.name);
	return 0;
    }
  
    if (sscanf(buf,"net agent s%s %s",buf,agentname)==2) {
	struct nk_net_ethernet_agent *agent = nk_net_ethernet_agent_find(agentname);
	if (!agent) {
	    nk_vc_printf("Cannot find agent %s\n", agentname);
	    return 0;
	}
	if (buf[0]=='t' && buf[1]=='o') {
	    if (nk_net_ethernet_agent_stop(agent)) {
		nk_vc_printf("Failed to to stop agent %s\n", agentname);
	    } else {
		nk_vc_printf("Stopped agent %s\n", agentname);
	    }
	} else if (buf[0]=='t' && buf[1]=='a') {
	    if (nk_net_ethernet_agent_start(agent)) {
		nk_vc_printf("Failed to to start agent %s\n", agentname);
	    } else {
		nk_vc_printf("Started agent %s\n", agentname);
	    }
	} else {
	    nk_vc_printf("Unknown agent command\n");
	}
	return 0;
    }
	  

  
    if (sscanf(buf,"net arp create %s %u.%u.%u.%u",agentname,&ip[0],&ip[1],&ip[2],&ip[3])==5) {
	ipv4_addr_t ipv4 = ip[0]<<24 | ip[1]<<16 | ip[2]<<8 | ip[3];
	struct nk_net_ethernet_agent *agent = nk_net_ethernet_agent_find(agentname);
	if (!agent) {
	    nk_vc_printf("Cannot find agent %s\n", agentname);
	    return 0;
	}
	struct nk_net_dev *netdev = nk_net_ethernet_agent_get_underlying_device(agent);
	struct nk_net_dev_characteristics c;
	struct nk_net_ethernet_arper *arper = nk_net_ethernet_arper_create(agent);
	if (!arper) {
	    nk_vc_printf("Failed to create arper\n");
	    return 0;
	}

	nk_net_dev_get_characteristics(netdev,&c);
      
	if (nk_net_ethernet_arper_add(arper,ipv4,c.mac)) {
	    nk_vc_printf("Failed to add ip->mac mapping\n");
	    return 0;
	}
	nk_vc_printf("Added ip->mac mapping\n");
	return 0;
    }      

    if (sscanf(buf,"net arp ask %u.%u.%u.%u",&ip[0],&ip[1],&ip[2],&ip[3])==4) {
	ipv4_addr_t ipv4 = ip[0]<<24 | ip[1]<<16 | ip[2]<<8 | ip[3];
	ethernet_mac_addr_t mac;
	int rc = nk_net_ethernet_arp_lookup(0,ipv4,mac);
	if (rc<0) {
	    nk_vc_printf("Lookup failed\n");
	} else if (rc==0) {
	    nk_vc_printf("Lookup of %u.%u.%u.%u is %02x:%02x:%02x:%02x:%02x:%02x\n",
			 ip[0],ip[1],ip[2],ip[3], mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	} else {
	    nk_vc_printf("Lookup is in progress\n");
	}
	return 0;
    }

#endif

#ifdef NAUT_CONFIG_NET_LWIP
    if (sscanf(buf,"net lwip start %s", intname)==1 && !strncasecmp(intname,"defaults",8)) {
	
	struct nk_net_lwip_config conf = { .dns_ip = 0x08080808 } ;

	if (nk_net_lwip_start(&conf)) {
	    nk_vc_printf("Failed to start lwip\n");
	    return 0;
	}

	struct nk_net_lwip_interface inter = { .name = "virtio-net0",
					       .ipaddr = 0x0a0a0a0a,
					       .netmask = 0xffffff00,
					       .gateway = 0x0a0a0a01 };
	

	if (nk_net_lwip_add_interface(&inter)) {
	    nk_vc_printf("Failed to add interface\n");
	    return 0;
	}

	nk_vc_printf("LWIP started with defaults and default interface added\n");
	return 0;
    }

    if (sscanf(buf,"net lwip start %u.%u.%u.%u", &dns[0], &dns[1], &dns[2], &dns[3])==4) {
	
	struct nk_net_lwip_config conf = { .dns_ip = dns[0]<<24 | dns[1]<<16 | dns[2]<<8 | dns[3] } ;

	if (nk_net_lwip_start(&conf)) {
	    nk_vc_printf("Failed to start lwip\n");
	    return 0;
	}

	nk_vc_printf("LWIP started\n");
	return 0;
	
    }

	
    if (sscanf(buf,"net lwip add %s %u.%u.%u.%u %u.%u.%u.%u %u.%u.%u.%u\n",
	       &intname,
	       &ip[0], &ip[1], &ip[2], &ip[3],
	       &netmask[0], &netmask[1], &netmask[2], &netmask[3],
	       &gw[0], &gw[1], &gw[2], &gw[3])==13) {

	struct nk_net_lwip_interface inter;

	strncpy(inter.name,intname,DEV_NAME_LEN); inter.name[DEV_NAME_LEN-1]=0;
	inter.ipaddr = ip[0]<<24 | ip[1]<<16 | ip[2]<<8 | ip[3];
	inter.netmask = netmask[0]<<24 | netmask[1]<<16 | netmask[2]<<8 | netmask[3];
	inter.gateway = gw[0]<<24 | gw[1]<<16 | gw[2]<<8 | gw[3];
	
	
	if (nk_net_lwip_add_interface(&inter)) {
	    nk_vc_printf("Failed to add interface\n");
	    return 0;
	}

	nk_vc_printf("added interface %s ip=%u.%u.%u.%u netmask=%u.%u.%u.%u gw=%u.%u.%u.%u\n",
		     intname,ip[0],ip[1],ip[2],ip[3],netmask[0],netmask[1],netmask[2],netmask[3],
		     gw[0],gw[1],gw[2],gw[3]);
	
	return 0;
    }

#ifdef NAUT_CONFIG_NET_LWIP_APP_ECHO
    if (!strcasecmp(buf,"net lwip echo")) {
	void echo_init();
	nk_vc_printf("Starting echo server (port 7)\n");
	echo_init();
	return 0;
    }
#endif

#ifdef NAUT_CONFIG_NET_LWIP_APP_HTTPD
    if (!strcasecmp(buf,"net lwip httpd")) {
	void httpd_init();
	nk_vc_printf("Starting httpd (port 80)\n");
	httpd_init();
	return 0;
    }
#endif

#ifdef NAUT_CONFIG_NET_LWIP_APP_LWIP_SHELL
    if (!strcasecmp(buf,"net lwip shell")) {
	void shell_init();
	nk_vc_printf("Starting shell server (port 23)\n");
	shell_init();
	return 0;
    }
#endif

#ifdef NAUT_CONFIG_NET_LWIP_APP_LWIP_SOCKET_ECHO
    if (!strcasecmp(buf,"net lwip socket_echo")) {
	void socket_echo_init();
	nk_vc_printf("Starting socket_echo (port 7)\n");
	socket_echo_init();
	return 0;
    }
#endif

#ifdef NAUT_CONFIG_NET_LWIP_APP_SOCKET_EXAMPLES
    if (!strcasecmp(buf,"net lwip socket_examples")) {
	void socket_examples_init();
	nk_vc_printf("Starting socket_examples (outgoing)\n");
	socket_examples_init();
	return 0;
    }
#endif

#ifdef NAUT_CONFIG_NET_LWIP_APP_LWIP_IPVCD
    if (!strcasecmp(buf,"net lwip ipvcd")) {
	void ipvcd_init();
	nk_vc_printf("Starting Virtual Console Daemon (port 23)\n");
	ipvcd_init();
	return 0;
    }
#endif
    
#endif
    
    nk_vc_printf("No relevant network functionality is configured or bad command\n");
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
  int intr;
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

 
  if (!strncasecmp(buf,"help",4)) { 
    nk_vc_printf("help\nexit\nvcs\ncores [n]\ntime [n]\nthreads [n]\n");
    nk_vc_printf("devs | sems | mqs | fses | ofs | cat [path]\n");
#ifdef NAUT_CONFIG_PROFILE
    nk_vc_printf("instrument start|end/stop|clear|query\n");
#endif
#ifdef NAUT_CONFIG_GPIO
    nk_vc_printf("gpio in/out/sweep [val]\n");
#endif
    nk_vc_printf("delay us | sleep us\n");
    nk_vc_printf("ioapic <list|dump> | ioapic num <pin|all> <mask|unmask>\n");
    nk_vc_printf("pci list | pci raw/dev bus slot func | pci dev [bus slot func]\n");
    nk_vc_printf("pci peek|poke bus slot func off [val] | pci cfg bus slot func\n");
    nk_vc_printf("shell name\n");
    nk_vc_printf("regs [t]\npeek [bwdq] x | mem x n [s] | mt x | poke [bwdq] x y\nin [bwd] addr | out [bwd] addr data\nrdmsr x [n] | wrmsr x y\ncpuid f [n] | cpuidsub f s | mtrrs [cpu] | int [cpu] v\n");
    nk_vc_printf("meminfo [detail]\n");
    nk_vc_printf("prog run | prog info\n");
    nk_vc_printf("reap | net ...\n");
#ifdef NAUT_CONFIG_CACHEPART
    nk_vc_printf("cachepart\n");
#endif
#ifdef NAUT_CONFIG_GARBAGE_COLLECTION
    nk_vc_printf("collect | leaks\n");
#endif
#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING
    nk_vc_printf("break\n");
#endif
    
    nk_vc_printf("burn a name size_ms tpr priority\n");
    nk_vc_printf("burn s name size_ms tpr phase size deadline priority\n");
    nk_vc_printf("burn p name size_ms tpr phase period slice\n");
    nk_vc_printf("real int [ax [bx [cx [dx]]]] [es:di]\n");
    nk_vc_printf("ipitest type (oneway | roundtrip | broadcast) trials [-f <filename>] [-s <src_id> | all] [-d <dst_id> | all]\n");
    nk_vc_printf("bench\n");
    nk_vc_printf("blktest dev r|w start count\n");
    nk_vc_printf("blktest dev r|w start count\n");
    nk_vc_printf("test threads|groups|tasks|stop|iso|bdwgc|pdsgc|omp|ompbench|ndpc|nesl|\n");
    nk_vc_printf("     udp_echo nic ip port num|cachepart ... |pmc ... | ...\n"); 
    nk_vc_printf("vm name [embedded image]\n");
    nk_vc_printf("run path\n");
    return 0;
  }


#ifdef NAUT_CONFIG_PROFILE
  if (!strncasecmp(buf,"inst",4)) {
      handle_instrument(buf);
      return 0;
  }
#endif

#ifdef NAUT_CONFIG_GPIO
  if (!strncasecmp(buf,"gpio",4)) {
      handle_gpio(buf);
      return 0;
  }
#endif

  if (!strncasecmp(buf,"net",3)) {
      handle_net(buf);
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
  
  if (!strncasecmp(buf,"sems",4)) {
    nk_semaphore_dump_semaphores();
    return 0;
  }

  if (!strncasecmp(buf,"mqs",3)) {
    nk_msg_queue_dump_queues();
    return 0;
  }

  if (!strncasecmp(buf,"ioapic",6)) {
    handle_ioapic(buf);
    return 0;
  }

  if (!strncasecmp(buf,"pci",3)) {
    handle_pci(buf);
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


#ifdef NAUT_CONFIG_CACHEPART
  if (!strncasecmp(buf,"cachepart",9)) {
      nk_cache_part_dump();
      return 0;
  }
#endif
  
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
    nk_launch_shell(name,-1,0,0); // simple interactive shell
    return 0;
  }

  if (!strncasecmp(buf,"reap",4)) { 
    nk_sched_reap(1); // unconditional reap
    return 0;
  }

#ifdef NAUT_CONFIG_GARBAGE_COLLECTION
  if (!strncasecmp(buf,"collect",7)) { 
      handle_collect(buf); 
      return 0;
  }
#endif

#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING
  if (!strncasecmp(buf,"break",5)) {
      nk_gdb_breakpoint_here();
      return 0;
  }
#endif

#ifdef NAUT_CONFIG_GARBAGE_COLLECTION
  if (!strncasecmp(buf,"leak",4)) { 
      handle_leaks(buf); 
      return 0;
  }
#endif

  uint64_t time_us;
  
  if (sscanf(buf,"delay %lu",&time_us)==1) {
    nk_vc_printf("Delaying for %lu us\n", time_us);
    nk_delay(time_us*1000UL);
    return 0;
  }

  if (sscanf(buf,"sleep %lu",&time_us)==1) {
    nk_vc_printf("Sleeping for %lu us\n", time_us);
    nk_sleep(time_us*1000UL);
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
	  *(uint8_t*)addr = data; clflush_unaligned((void*)addr,1);
	  nk_vc_printf("Mem[0x%016lx] = 0x%02lx\n",addr,data);
	  break;
      case 'w': 
	  *(uint16_t*)addr = data; clflush_unaligned((void*)addr,2);
	  nk_vc_printf("Mem[0x%016lx] = 0x%04lx\n",addr,data);
	  break;
      case 'd': 
	  *(uint32_t*)addr = data; clflush_unaligned((void*)addr,4);
	  nk_vc_printf("Mem[0x%016lx] = 0x%08lx\n",addr,data);
	  break;
      case 'q': 
	  *(uint64_t*)addr = data; clflush_unaligned((void*)addr,8);
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
	  break;
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

  if ((sscanf(buf,"mtrrs %d", &cpu)==1) ||
      (cpu=-1, !strcmp(buf,"mtrrs"))) {
      nk_mtrr_dump(cpu);
      return 0;
  }

  if (sscanf(buf,"mt %lx", &addr)==1) {
      uint8_t type;
      char *typestr;
	  
      if (nk_mtrr_find_type((void*)addr,&type,&typestr)) {
	  nk_vc_printf("Cannot find memory type for %p\n",addr);
      } else {
	  nk_vc_printf("Mem[0x%016lx] has type 0x%02x %s\n", addr, type, typestr);
      }
      return 0;
  }
  
  if ((sscanf(buf,"int %d %d", &cpu, &intr)==2) ||
      (cpu=my_cpu_id(), sscanf(buf,"int %d",&intr)==1)) {
      if (cpu==my_cpu_id()) {
	  apic_self_ipi(per_cpu_get(apic), intr);
      } else if (cpu>=0) {
	  apic_ipi(per_cpu_get(apic),cpu,intr);
      } else {
	  apic_bcast_ipi(per_cpu_get(apic),intr);
	  apic_self_ipi(per_cpu_get(apic), intr);
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

  if (sscanf(buf,"burnu p %s %llu %u %llu %llu %llu", name, &size_ns, &tpr, &phase, &period, &slice)==6) { 
    nk_vc_printf("Starting periodic burner %s with size %llu ms tpr %u phase %llu from now period %llu us slice %llu us\n",name,size_ns,tpr,phase,period,slice);
    size_ns *= 1000000;
    phase   *= 1000; 
    period  *= 1000;
    slice   *= 1000;
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

#ifdef NAUT_CONFIG_LOAD_LUA
	if (!strncasecmp(buf, "lua", 3)) {
		handle_lua_cmd(buf);
		return 0;
	}
#endif

  if (!strncasecmp(buf,"prog",4)) {
      handle_prog_cmd(buf);
      return 0;
  }

  nk_vc_printf("Don't understand \"%s\"\n",buf);
  return 0;
}

struct shell_op {
  char name[32];
  char **script;
  uint32_t flags;
};

static void shell(void *in, void **out)
{
  struct shell_op *op = (struct shell_op *)in;
  struct nk_virtual_console *vc = nk_create_vc(op->name,COOKED, 0x9f, 0, 0);
  char buf[MAX_CMD];
  char lastbuf[MAX_CMD];
  int first=1;

  if (!vc) { 
    ERROR_PRINT("Cannot create virtual console for shell\n");
    return;
  }

  if (nk_thread_name(get_cur_thread(),op->name)) {   
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

  if (op->script) {
    int i;
    for (i=0; *op->script[i]; i++) {
      nk_vc_printf("***exec: %s\n",op->script[i]);
      handle_cmd(op->script[i],MAX_CMD);
    }
    if (op->flags & NK_SHELL_SCRIPT_ONLY) {
      goto out;
    }
  }
  
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

 out:
  free(in);
  nk_release_vc(get_cur_thread());

  // exit thread
  
}

nk_thread_id_t nk_launch_shell(char *name, int cpu, char **script, uint32_t flags)
{
  nk_thread_id_t tid;
  struct shell_op *op = (struct shell_op *)malloc(sizeof(struct shell_op));

  if (!op) {
    return 0;
  }

  memset(op,0,sizeof(*op));
  
  strncpy(op->name,name,32);
  op->name[31]=0;
  op->script = script;
  op->flags = flags;
  
  if (nk_thread_start(shell, (void*)op, 0, 1, SHELL_STACK_SIZE, &tid, cpu)) { 
      free(op);
      return 0;
  } else {
      INFO_PRINT("Shell %s launched on cpu %d as %p\n",name,cpu,tid);
      return tid;
  }
}



