#include <nautilus/nautilus.h>
#include <nautilus/scheduler.h>
#include <nautilus/libccompat.h>
#include <nautilus/naut_string.h>
#include <nautilus/spinlock.h>
#include <nautilus/condvar.h>
#include <nautilus/thread.h>
#include <nautilus/barrier.h>
#include <nautilus/scheduler.h>
#include <nautilus/group_sched.h>


#define INFO(fmt, args...) INFO_PRINT("bsp: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("bsp: " fmt, ##args)

#if 0
#define DEBUG(fmt, args...) DEBUG_PRINT("bsp: " fmt, ##args)
#else
#define DEBUG(fmt, args...)
#endif

#define printf(fmt, args...) nk_vc_printf(fmt, ##args);
#define fprintf(foo,fmt, args...) nk_vc_printf(fmt, ##args);

#ifdef ENABLE_PARSEC_HOOKS
#include <hooks.h>
#endif

struct args {
    int numproc;
    int myproc;
    int iters;
    int elements;
    int flops;
    int reads;
    int writes;
    int comms;

    int rc;
    
    int **localmem;

    nk_counting_barrier_t *barrier;
    struct nk_thread_group *group;
    struct nk_sched_constraints *constraints;
    int nobarrier;
};
    

static void do_flops(struct args *a)
{
    int i;
    volatile double sum;
    volatile double x=1995;
    
    double val;
    double mpy;
    
    sum=x;
    for (i=0;i<a->flops;i++) {
	val=x;
	mpy=x;
	sum = sum + mpy*val;
    }
}

static void do_reads(struct args *a)
{
    int i;
    volatile int *mymem = a->localmem[a->myproc];
    volatile double sum;
    volatile double x;
    
    for (i=0;i<a->reads;i++) {
	sum = mymem[i];
    }
}

static void do_writes(struct args *a)
{
    int i;
    volatile int *mymem = a->localmem[a->myproc];
    volatile double sum;
    volatile double x=93;

    sum = x;
    
    for (i=0;i<a->writes;i++) {
	mymem[i] = sum;
    }
}
    

static void do_compute(struct args *a)
{
    int i;

    for (i=0;i<a->elements;i++) { 
	do_flops(a);
	do_reads(a);
	do_writes(a);
    }
}

static void do_comm(struct args *a)
{
    int i;
    // for now, just a simple ring...
    int neighbor = (a->myproc + 1) % a->numproc;
    volatile int *neighbormem = a->localmem[neighbor];

    //DEBUG("Writing neighbor mem %p through %p\n", neighbormem, neighbormem+a->comms);
    
    for (i=0;i<a->comms;i++) {
	neighbormem[i] = a->localmem[a->myproc][i];
    }
   
}

#define MAX(x,y) ((x)>(y) ? (x) : (y))

static void doit(void *arg, void **out)
{
    struct args *a = (struct args *)arg;
    char buf[80];
    int rc = -1;
    int allocsize = MAX(a->reads,MAX(a->writes,a->comms));

    DEBUG("binding vc\n");
    nk_bind_vc(get_cur_thread(),get_cur_thread()->parent->vc);

    sprintf(buf,"bsp-%d",a->myproc);

    DEBUG("naming self\n");
    nk_thread_name(get_cur_thread(),buf);

    DEBUG("allocating local space\n");
    a->localmem[a->myproc] = (int*)malloc_specific(sizeof(int)*allocsize,my_cpu_id());

    if (!a->localmem[a->myproc]) {
	ERROR("local allocation failed\n");
	goto outvc;
    }

    // we need to be sure everyone has allocated their memory before anyone writes
    // we do this by having a barrier in the following before the loop
    
    if (a->group) {
	DEBUG("group join\n");
	if (nk_thread_group_join(a->group)<0) {
	    ERROR("failed to join group\n");
	    goto outmem;
	}

	// wait until everyone has joined the group
	nk_counting_barrier(a->barrier);

	if (a->constraints) {
	    DEBUG("group change constraint\n");
	    if (nk_group_sched_change_constraints(a->group,a->constraints)) {
		ERROR("failed group change constraints\n");
		goto outgroup;
	    }
	}
    }

    // do a barrier sync if change constraints did not affect this already
    if (!a->constraints) {
	nk_counting_barrier(a->barrier);
    }

    int i;

    for (i=0;i<a->iters;i++) {
	DEBUG("Iteration %d\n",i);
	do_compute(a);
	if (!a->nobarrier) {
	    DEBUG("First comm barrier\n");
	    nk_counting_barrier(a->barrier);
	} else {
	    DEBUG("Skipping first comm barrier\n");
	}
	do_comm(a);
	if (!a->nobarrier) {
	    DEBUG("Second Comm barrier\n");
	    nk_counting_barrier(a->barrier);
	} else {
	    DEBUG("Skipping first comm barrier\n");
	}
    }

    // force final barrier since we need to be sure that we are OK to free memory

    nk_counting_barrier(a->barrier);

    DEBUG("Done, cleaning up\n");

    
    rc = 0;

    // now cleanup for either error path or normal path
 outgroup:
    nk_thread_group_leave(a->group);
 outmem:
    free(a->localmem[a->myproc]);
    a->localmem[a->myproc]=0;
 outvc:
    nk_release_vc(get_cur_thread());

    a->rc = rc;
    
}



//
//
//
int test_bsp(int nump, int startp,  // processors (number and start)
	     int elementsperproc,   // size of each local array 
	     int flopsperelement,
	     int readsperelement,
	     int writesperelement,
	     int commsperproc,       // number of elements read/written from neighbors
	     int iters,             // number of iters
	     struct nk_sched_constraints *constraints, // schedule constraint
	     int nobarrier,         // skip barriersync
	     uint64_t *time_ns)
{
    int rc = -1;
    uint64_t start, end;

    
    DEBUG("testbsp(nump=%d, startp=%d, elements=%d, flops=%d, reads=%d, writes=%d, comms=%d, iters=%d, c=%p, nobarrier=%d\n",
	  nump,startp,elementsperproc,flopsperelement, readsperelement, writesperelement, commsperproc,iters,constraints,nobarrier);

    // allocate space for args, localmem array, barrier, group
    // be paranoid about args since it could be huge

    struct args *a = malloc(sizeof(struct args)*nump);
    if (!a) {
	ERROR("Failed to allocate space for args\n");
	goto outfinal;
    }
    int **localmem = (int **)malloc(sizeof(int *)*nump);
    if (!localmem) {
	ERROR("Failed to allocate space for localmem\n");
	goto outargs;
    }

    struct nk_thread_group *group = nk_thread_group_create("Alphaville");

    if (!group) {
	ERROR("Failed to create group\n");
	goto outlocalmem;
    }

    nk_counting_barrier_t barrier;
    nk_counting_barrier_init(&barrier,nump);
    
    int i;

    start = nk_sched_get_realtime();
    
    // launch the actual work
    for (i=0;i<nump;i++) {
	
	a[i].numproc = nump;
	a[i].myproc = i;
	a[i].iters = iters;
	a[i].elements = elementsperproc;
	a[i].flops = flopsperelement;
	a[i].reads = readsperelement;
	a[i].writes = writesperelement;
	a[i].comms = commsperproc;
	a[i].localmem = localmem;        // all threads see each others local memory
	a[i].barrier = &barrier;         // all threads share single barrier
	a[i].group = group;              // all threads share a group
	a[i].constraints = constraints;  // all threads share the scheduling constraints, if any
	a[i].nobarrier = nobarrier;      // all threads either do or do not use barriers

	if (nk_thread_start(doit,
			    (void*)&(a[i]),
			    0,
			    0,
			    TSTACK_DEFAULT,
			    0, // we don't are about the TID
			    startp == -1 ? startp : startp+i)) {
	    ERROR("Failed to launch thread %d\n", i);
	    // we now are going to leak, since we can't stop whatever
	    // threads we have already launched
	    return rc;
	}
	DEBUG("thread %d launched\n",i);
    }

    DEBUG("joining threads\n");

    // now wait for them to all exit
    nk_join_all_children(0);
    
    end = nk_sched_get_realtime();

    DEBUG("all threads joined\n",i);

    DEBUG("done.  cleaning up\n");

    DEBUG("Total time: %lu ns\n", end-start);

    if (time_ns) {
	*time_ns = end-start;
    }

    rc = 0;
    
    //outgroup:
    nk_thread_group_delete(group);
 outlocalmem:
    free(localmem);
 outargs:
    free(a);
 outfinal:
    return rc;

}

    
int test_bsp_sweep(int nump, int startp,  // processors (number and start)
		   struct nk_sched_constraints *constraints, // schedule constraint
		   int nobarrier)
{
    // we are looking for tiny communication
    nk_vc_printf("Sweeping BSP\n");
    int n;
    uint64_t ns;

    for (n=1;n<128;n++) {
	//nk_vc_printf("nump=%d, startp=%p, constraints=%p, nobarrier=%d\n",constraints,nobarrier);
	test_bsp(nump,startp,10000,n,n,n,n,100,constraints,nobarrier,&ns);
	nk_vc_printf("%d %lu ns\n",n,ns);
    }

    return 0;
}


int test_bsp_eval()
{
    struct nk_sched_constraints constraints;
    
    // we are looking for tiny communication
    nk_vc_printf("Sweeping BSP\n");
    int n;
    uint64_t ns;
    int nobarrier;
    int nump;
    int ne;

#define MAX_N  128
#define MAX_NE 128
#define ITERS  100
    
    // sweep numprocs 1..numcpus in powers of 2 plus maximum possible
    // sweep schedule
    // sweep nobarrier on/off
    // sweep n 1..128 in powers of two, skipping 0
    // sweep ne 1..128 in powers of two, skipping 0
    // for ... schedule

#define DOIT \
	for (nobarrier=0;nobarrier<2;nobarrier++) {			\
          for (ne=1;ne<=MAX_NE;ne*=2) {				\
	    for (n=1;n<=MAX_N;n*=2) {					\
		char buf[80];						\
									\
		nk_sched_reap(1);					\
									\
		test_bsp(nump,1,ne,n,n,n,n,ITERS,&constraints,nobarrier, &ns); \
									\
		nk_vc_printf("%d %d %d %d %d %lu",				\
			     nump,ITERS,ne,n,nobarrier,ns);		\
									\
		switch (constraints.type) {				\
		case APERIODIC:						\
		    nk_vc_printf(" a %u %llu 0", constraints.interrupt_priority_class,constraints.aperiodic.priority); \
		    break;						\
		case SPORADIC:						\
		    nk_vc_printf(" s %u %llu 0", constraints.interrupt_priority_class,constraints.sporadic.size); \
		    break;						\
		case PERIODIC:						\
		    nk_vc_printf(" p %u %llu %llu", constraints.interrupt_priority_class,constraints.periodic.period,constraints.periodic.slice); \
		    break;						\
		}							\
		nk_vc_printf("\n");					\
	    }								\
	  }								\
	}

#define PERIOD_MIN   100000    //100 us
#define PERIOD_MAX   10000000  // 10 ms
#define PERIOD_STEP  100000    //100us
// these are in percentages of the period
#define SLICE_MIN  10        // 10/100
#define SLICE_MAX  80        // 80/100
#define SLICE_STEP 10        // 10/100

    uint64_t period, slice;
    
    nk_vc_printf("#numproc iters ne n nobarrier time_ns schedule...\n");
    for (nump=1;
	 nump<nk_get_num_cpus();
	 nump = (nump*2 < nk_get_num_cpus() || nump==nk_get_num_cpus()-1) ? nump*2 : nk_get_num_cpus()-1) {
    //for (nump=nk_get_num_cpus()-1;nump<nk_get_num_cpus();nump++) {
	constraints.type=APERIODIC;
	constraints.interrupt_priority_class = 0xe;
	constraints.aperiodic.priority = 2000000000;

	DOIT ;

	// now again sweeping period/slice
	for (period=PERIOD_MIN;period<=PERIOD_MAX;period+=PERIOD_STEP) {
	    for (slice=SLICE_MIN; slice<=SLICE_MAX; slice+=SLICE_STEP) {
		constraints.type=PERIODIC;
		constraints.interrupt_priority_class = 0xe;
		constraints.periodic.phase = 0;
		constraints.periodic.period = period;
		constraints.periodic.slice = (period * slice)/100;

		DOIT;
	    }
	}
    }
    return 0;
}

	

	
