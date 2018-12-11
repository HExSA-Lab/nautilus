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
 * Copyright (c) 2018, Vyas Alwar
 * Copyright (c) 2018, Qingtong Guo
 * Copyright (c) 2018, Peter Dinda
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Vyas Alwar <valwar@math.northwestern.edu>
 *          Qingtong Guo <QingtongGuo2019@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/cachepart.h>
#include <nautilus/spinlock.h>
#include <nautilus/cpuid.h> 
#include <nautilus/mm.h> 
#include <nautilus/naut_string.h> 
#include <nautilus/printk.h> 
#include <nautilus/msr.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>
#include <test/cachepart.h>


/*
  Implementation of cache partitioning for:

  - Intel CAT only
  - Simple partitioning of L3 only
  - No code/data specialization
  - No special support for NUMA (cache partitioning applies to entire system)
*/

#if NAUT_CONFIG_DEBUG_CACHEPART
#define DEBUG(fmt, args...)  DEBUG_PRINT("cachepart: " fmt, ##args)
#else
#define DEBUG(fmt, args...)
#endif

#define ERROR(fmt, args...) ERROR_PRINT("cachepart: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("cachepart: " fmt, ##args)



#define S_BIT(i) (1u << i)

#define HAVE_FULL_BITMASK 1

#define FULL_BITMASK (cachetable.cos_size - 1)


#define PART_INCREASE 0
#define PART_DECREASE -1


// Intel CAT MSRs
#define IA32_L2_QOS_MASK_0            0x00000D10
#define IA32_L3_QOS_MASK_0            0x00000C90
#define IA32_L3_QOS_MASK(i) ((IA32_L3_QOS_MASK_0) + (i))
#define IA32_PQR_ASSOC                0x00000C8F

typedef struct _cos {
    uint32_t refcount;
    uint32_t bitmask;
} cos;

typedef struct _cos_update {
  uint64_t new_cos;
  uint64_t new_bitmask;
} cos_update;


// One shared table across system tracks all
// current partitions and their refcounts
static struct {
    spinlock_t lock;

    // are we functonal?
    uint8_t    usable;

    // system-wide settings, determined at init time
    uint32_t   thread_default_percent;
    uint32_t   interrupt_percent;

    // machine capabilities - bitmap length and number of COSes
    uint32_t   cbm_length;
    uint32_t   cos_size;


    // refcounts for each of the partitions (each bit in the CBM)
    uint32_t   *part_refcount;

    // refcounts and bitmasks for each cos
    cos        *coses;

} cachetable;

static int cat_started = 0;

// globally visible for use in the thread low-level code
int _nk_cache_part_has_cat = 0;

// used for group cache changes
static spinlock_t    group_state_lock;


#define GLOBAL_LOCK_CONF uint8_t _global_flags=0
#define GLOBAL_LOCK() _global_flags = spin_lock_irq_save(&cachetable.lock)
#define GLOBAL_UNLOCK() spin_unlock_irq_restore(&cachetable.lock,_global_flags)

#define CEIL_DIV(x,y)  (((x)/(y)) + !!((x)%(y)))

static int get_vendor_string(char []);
static int is_intel(void);
static int build_bitmask(uint8_t, uint8_t, uint32_t *);
static int find_contiguous_bitmask(uint8_t, uint32_t *);
static int is_part_usable_no_shared(int, uint8_t);
static int update_part_refcounts(uint32_t, int);
static int find_idle_cos();
static int setup_cos_with_length(uint8_t);
static int set_bitmask(nk_cache_part_t, uint64_t);
static int set_thread(nk_cache_part_t);


static int cachetable_init(cpuid_ret_t* id, uint32_t thread_default_percent, uint32_t interrupt_percent)
{
    cachetable.cbm_length = (id -> a & 0x1f) + 1;
    cachetable.cos_size = (id -> d & 0xffff) + 1;
    cachetable.thread_default_percent = thread_default_percent;
    cachetable.interrupt_percent = interrupt_percent;

    cachetable.part_refcount = malloc(cachetable.cbm_length * sizeof(uint32_t));

    if (!cachetable.part_refcount) {
	ERROR("Failed to allocate refcounts\n");
	return -1;
    }

    memset(cachetable.part_refcount, 0, cachetable.cbm_length * sizeof(uint32_t));

    cachetable.coses = malloc(cachetable.cos_size * sizeof(cos));

    if (!cachetable.coses) {
	ERROR("Failed to allocated coses\n");
	free(cachetable.part_refcount);
	return -1;
    }
    
    memset(cachetable.coses, 0, cachetable.cos_size * sizeof(cos));

    cachetable.usable = 1;

    return 0;
}

int nk_cache_part_init(uint32_t thread_default_percent, uint32_t interrupt_percent)
{
    cpuid_ret_t id;
    int maximum_leaf;

    DEBUG("init\n");
    
    /* Initialize Cachetable */
    memset(&cachetable, 0, sizeof(cachetable));

    spinlock_init(&cachetable.lock);

    spinlock_init(&group_state_lock);
    
    if (!is_intel()) {
        DEBUG("Only works for intel processor\n");
        return -1;
    }
    
    /* Check for CAT using CPUID */
    maximum_leaf = cpuid_leaf_max();
    DEBUG("Maximum leaf is %d\n", maximum_leaf);

    if (maximum_leaf < 0x7) {
        DEBUG("CPU cannot have RDT\n");
        return -1;
    }

    if (maximum_leaf < 0x10) {
        DEBUG("Weird. CPU has RDT, but cannot have L2/L3 info...\n");
    }

    cpuid_sub(0x7, 0x0, &id);

    if (id.b & 0x8000) {
        DEBUG("CPU has RDT (PQE = 1) - checking for CAT\n");
    } else {
        DEBUG("CPU does not have RDT (PQE = 0)\n");
        return -1;
    }

    /* Cache-specific details */
    cpuid_sub(0x10, 0, &id);

    int l2 = !!(id.b & 0x4);
    int l3 = !!(id.b & 0x2);

    DEBUG("CAT check returns 0x%x (L2 = %d L3 = %d)\n", id.b, l2, l3);

    if (l3) {
        cpuid_sub(0x10, 1, &id);
        DEBUG("L3 info\n");
    } else {
	DEBUG("CPU does not have L3 info - failing\n");
	return -1;
    }

    DEBUG(" CBM_LEN = %d\n", id.a & 0x1f);
    DEBUG(" Sharable Resource Bitmap = 0x%x\n", id.b);
    DEBUG(" CDP = %d\n", (id.c & 0x4)>>2);
    DEBUG(" COS_MAX = %d\n",id.d & 0xffff);
    DEBUG(" L3: %d  L2: %d %s\n", l3, l2, l2 ? "(ignored)" : "");

    if (cachetable_init(&id, thread_default_percent, interrupt_percent)) {
	ERROR("Failed to initialize cache table...\n");
	return -1;
    } else {
	_nk_cache_part_has_cat = 1;

	INFO("initialized with %u coses and bitmap length %u\n", cachetable.cos_size, cachetable.cbm_length);
	INFO("thread default %u%%   interrupt %u%% %s\n",
	     cachetable.thread_default_percent, cachetable.interrupt_percent,
#ifdef NAUT_CONFIG_CACHEPART_INTERRUPT
	     "");
#else
	     "(disabled)");
#endif
	return 0;
    }
}

// currently does nothing
int nk_cache_part_init_ap()
{
   DEBUG("init ap\n");
   return 0; // always succeeds
}


static int cache_part_shared_start()
{
    DEBUG("Update bitmask\n");
    DEBUG("Bitmask:      %08x\n", cachetable.coses[NK_CACHE_PART_THREAD_DEFAULT].bitmask);
    DEBUG("CPU:          %d \n", my_cpu_id());

    set_bitmask(NK_CACHE_PART_THREAD_DEFAULT, (uint64_t)cachetable.coses[NK_CACHE_PART_THREAD_DEFAULT].bitmask);

#ifdef NAUT_CONFIG_CACHEPART_INTERRUPT
    set_bitmask(NK_CACHE_PART_INTERRUPT_DEFAULT, (uint64_t)cachetable.coses[NK_CACHE_PART_INTERRUPT_DEFAULT].bitmask);
#endif

#if HAVE_FULL_BITMASK
    set_bitmask(FULL_BITMASK, (uint64_t)cachetable.coses[FULL_BITMASK].bitmask);
#endif
    
    DEBUG("Set cos\n");
    set_thread(NK_CACHE_PART_THREAD_DEFAULT);
    DEBUG("Set up thread\n");
    
    return 0;
}

// call on cpu 0 after scheduler start but before interrupts on
int nk_cache_part_start()
{
    uint8_t thread_num_bits, intr_num_bits;
    
    DEBUG("Start\n");
    
    // If there is no cat
    if (!cachetable.usable) {
        return 0;

    }

    // Set up the default bitmask  (will be last k bits)
    thread_num_bits =  (uint8_t) (CEIL_DIV((cachetable.cbm_length * cachetable.thread_default_percent),100));
    intr_num_bits =  (uint8_t) (CEIL_DIV((cachetable.cbm_length * cachetable.interrupt_percent),100));

    DEBUG("Default setup:  default thread bits = %u (%u%%), interrupt bits = %u (%u%%), available bits = %u\n",
	  thread_num_bits, 100*thread_num_bits/cachetable.cbm_length,
	  intr_num_bits, 100*thread_num_bits/cachetable.cbm_length, cachetable.cbm_length);
    
    build_bitmask(thread_num_bits - 1, 0, &cachetable.coses[NK_CACHE_PART_THREAD_DEFAULT].bitmask);
    cachetable.coses[NK_CACHE_PART_THREAD_DEFAULT].refcount++;
    update_part_refcounts(cachetable.coses[NK_CACHE_PART_THREAD_DEFAULT].bitmask, PART_INCREASE);

#ifdef NAUT_CONFIG_CACHEPART_INTERRUPT
    build_bitmask(intr_num_bits + thread_num_bits - 1, thread_num_bits, &cachetable.coses[NK_CACHE_PART_INTERRUPT_DEFAULT].bitmask);
    cachetable.coses[NK_CACHE_PART_INTERRUPT_DEFAULT].refcount++;
    update_part_refcounts(cachetable.coses[NK_CACHE_PART_INTERRUPT_DEFAULT].bitmask, PART_INCREASE);
#endif
    
#if HAVE_FULL_BITMASK
    build_bitmask(cachetable.cbm_length - 1, 0, &cachetable.coses[FULL_BITMASK].bitmask);
    cachetable.coses[FULL_BITMASK].refcount++;
    // the full bitmask is not included in allocation and will not be deleted
#endif

    if (cache_part_shared_start()) {
	ERROR("cache shared startup failed\n");
	return -1;
    }
    
    __sync_fetch_and_or(&cat_started, 1);

    DEBUG("Active\n");
    
    return 0;

}

// call on each AP after scheduler start but before interrupts on
int nk_cache_part_start_ap()
{
    DEBUG("start ap\n");

    if (!cachetable.usable) {
	return 0;
    }
    
    DEBUG("Cache part start (waiting)\n");

    while (!__sync_fetch_and_and(&cat_started,1)) {}
      
    return cache_part_shared_start();
}


// Used to write the bitmask on the target CPU
static void cos_update_xcall(void *arg)
{
    cos_update *u = (cos_update *)arg;
    set_bitmask((uint32_t)u->new_cos, (uint64_t)u->new_bitmask);
}

    


int nk_cache_part_acquire(uint32_t percent)
{
    GLOBAL_LOCK_CONF;
    
    uint8_t length;
    uint8_t cos_index;
    
    cos_index = get_cur_thread() -> cache_part_state;

    DEBUG("acquire %u%% current cos is %u\n",percent,cos_index);

    if (!cachetable.usable || percent > 100) {
	GLOBAL_UNLOCK();
	ERROR("Cache partitioning disabled or impossible request\n");
	return -1;
    }

    // switch back to default first if necessary
    if (cos_index != NK_CACHE_PART_THREAD_DEFAULT
        && nk_cache_part_release(cos_index) < 0) {
	ERROR("Unable to release non-default partition\n");
	return -1;
    }

    GLOBAL_LOCK();

#if HAVE_FULL_BITMASK
    if (percent == 100) { 
	set_thread(FULL_BITMASK);
	GLOBAL_UNLOCK();
	return 0;
    }
#endif
    
    length = (uint8_t)CEIL_DIV(percent * cachetable.cbm_length,100); //Calculate how many partitions we need

    if (setup_cos_with_length(length)) {
	GLOBAL_UNLOCK();
	ERROR("Unable to satisfy request\n");
	return -1;
    }

    GLOBAL_UNLOCK();

    return 0;
}

int nk_cache_part_select(nk_cache_part_t part)
{
    GLOBAL_LOCK_CONF;
    
    uint8_t cos_index;
    

    if (!cachetable.usable
	|| part >= cachetable.cbm_length
	|| part == NK_CACHE_PART_INTERRUPT_DEFAULT
	|| cachetable.coses[part].refcount == 0) {
        DEBUG("Invalid cos\n");
        return -1;
    }

    cos_index = get_cur_thread() -> cache_part_state;

    if (cos_index != NK_CACHE_PART_THREAD_DEFAULT) {
	nk_cache_part_release(cos_index);
    }
    

    if (part!=NK_CACHE_PART_THREAD_DEFAULT && part!=FULL_BITMASK) { 
	GLOBAL_LOCK();
	cachetable.coses[part].refcount++;
	update_part_refcounts(cachetable.coses[part].bitmask, PART_INCREASE);
	GLOBAL_UNLOCK();
    }
    
    get_cur_thread()->cache_part_state = part;

    return 0;
}

int nk_cache_part_release(nk_cache_part_t cos_index)
{
    GLOBAL_LOCK_CONF;

    GLOBAL_LOCK();

    if (!cachetable.usable
	|| cos_index >= cachetable.cbm_length
	|| cachetable.coses[cos_index].refcount == 0
        || get_cur_thread() -> cache_part_state != cos_index) {
	GLOBAL_UNLOCK();
        DEBUG("Invalid cos\n");
        return -1;
    }

    if (cos_index == NK_CACHE_PART_THREAD_DEFAULT) {
	GLOBAL_UNLOCK();
        DEBUG("Can't release thread default bitmask\n");
        return -1;
    }

    if (cos_index == NK_CACHE_PART_INTERRUPT_DEFAULT) {
	GLOBAL_UNLOCK();
	DEBUG("Can't release interrupt bitmask\n");
	return -1;
    }

#if HAVE_FULL_BITMASK
    if (cos_index == FULL_BITMASK) {
	GLOBAL_UNLOCK();
	DEBUG("Can't release full bitmask\n");
	return -1;
    }
#endif
    
    set_thread(NK_CACHE_PART_THREAD_DEFAULT);

    update_part_refcounts(cachetable.coses[cos_index].bitmask, PART_DECREASE);
    cachetable.coses[cos_index].refcount--;

    GLOBAL_UNLOCK();

    return 0;
}

//Free the resources
int nk_cache_part_deinit()
{
    GLOBAL_LOCK_CONF;
    
    GLOBAL_LOCK();

    if (!cachetable.usable) {
	GLOBAL_UNLOCK();
        return 0;
    }

    free(cachetable.part_refcount);
    free(cachetable.coses);

    GLOBAL_UNLOCK();
    
    return 0;
}

/* CPUID Helpers */

static int get_vendor_string (char name[13])
{
    cpuid_ret_t id;
    cpuid(0, &id);
    ((uint32_t*)name)[0] = id.b;
    ((uint32_t*)name)[1] = id.d;
    ((uint32_t*)name)[2] = id.c;
    name[12] = '\0';
    return id.a;
}


static int is_intel(void)
{
    char name[13];
    get_vendor_string(name);
    return !strncmp(name, "GenuineIntel", 13);
}


// this must be invoked with the lock held
// must be holding lock
static int setup_cos_with_length(uint8_t length)
{
    uint32_t i;
    cos_update u;

    int new_bitmask; //The index of the bitmask
    int new_cos; //The index of the cos

    if ((new_cos = find_idle_cos()) < 0) { 
        DEBUG("No empty cos\n");
        return -1;
    }

    //Try to find 'length' continuous partitions. If can't find, return -1
    if (find_contiguous_bitmask(length, &new_bitmask) < 0) { 
        DEBUG("Not enough partitions\n");
        return -1;
    }

    DEBUG("Bitmask: %08x COS: %d\n", new_bitmask, new_cos);
    cachetable.coses[new_cos].bitmask = new_bitmask; //Update the cos table
    cachetable.coses[new_cos].refcount++;
    update_part_refcounts(new_bitmask, PART_INCREASE);

    DEBUG("Write bitmask to cos\n");

    u.new_cos = new_cos;
    u.new_bitmask = new_bitmask;

    //Write to all the CPUs, including self
    for (i = 0; i < nk_get_num_cpus(); i++) {
        if (i == my_cpu_id()) {
            cos_update_xcall(&u);
        } else {
            smp_xcall(i, cos_update_xcall, &u, 1);
        }
    }

    DEBUG("Set up cur thread\n");

    set_thread((uint64_t)new_cos); //Set the cos to current thread
    
    DEBUG("Done\n");
    
    return new_cos;
}

// Create a bitmask that begin with high bit and end with low bit [high,low]
static int build_bitmask(uint8_t high, uint8_t low, uint32_t* bitmask)
{
    int i;

    if (    low >= cachetable.cbm_length ||
            high >= cachetable.cbm_length ||
            low > high) {

        return -1;
    }

    *bitmask = 0;
    for (i = high; i >= low; i--) {
        *bitmask |= S_BIT(i);
    }
    return 0;
}

// Find 'length' continuous bitmasks. Different coses don't overlap
// must be holding lock
static int find_contiguous_bitmask(uint8_t length, uint32_t *bitmask)
{
    if (length == 0) {
        return -1;
    }

    int base;

    for (base = 0; base + length <= cachetable.cbm_length; base++) {
        if (is_part_usable_no_shared(base, length)) {
            if (build_bitmask(base + length - 1, base, bitmask) == 0) {
                return 0;
            }
        }
    }
    return -1;
}



// Judge whether the 'length' continuous bitmasks that begins with base is usable
// must be holding lock
int is_part_usable_no_shared(int base, uint8_t length)
{
    int i;

    for (i = base; i < (base + length); i++) {
        if (cachetable.part_refcount[i] > 0) {
            return 0;
	}
    }
    return 1;
}


//Update the use situation of the all the partition (increase or decrease the number of threads using the partition)
//If flag >= 0, increase all the partition used in bitmask. If flag < 0, decrease all the partitons used in bitmask
//must be holding lock
int update_part_refcounts(uint32_t bitmask, int flag)
{
    int i;

    for (i = cachetable.cbm_length - 1; i >= 0; i--) {
        if (!!(bitmask & (1 << i))) {
            if (flag >= 0) {
                cachetable.part_refcount[i]++;
            } else {
                cachetable.part_refcount[i]--;
            }
        }
    }
    return 0;
}

//Find a usable cos
// must be holding lock
static int find_idle_cos()
{
    int i;

    for (i = 0; i < cachetable.cos_size; i++) {
        if (cachetable.coses[i].refcount == 0) {
            return i;
        }
    }
    return -1;
}

//Set the bitmask to the corresponding cos
//Bitmask register
//--------------------------------------
//| Reserved(63 - 32) | Bitmask(31 - 0) |
//--------------------------------------
static int set_bitmask(nk_cache_part_t cos_index, uint64_t bitmask)
{
    msr_write(IA32_L3_QOS_MASK(cos_index), bitmask); //Set up the bitmask of corresponding cos
    return 0;
}

//Set the index of cos to current thread
//The structure of IA32_PQR_ASSOC
//--------------------------------------------------------
//| COS Index(63 - 32) | Reserved(31 - 10) | RMID(9 - 0) |
//--------------------------------------------------------
static int set_thread(uint16_t cos_index)
{
    nk_thread_t* cur_thread = get_cur_thread();
    cur_thread->cache_part_state = cos_index; 
    msr_write(IA32_PQR_ASSOC, ((uint64_t)cos_index) << 32);
    return 0;
}


nk_cache_part_t nk_cache_part_get_current()
{
    return get_cur_thread()->cache_part_state;
}
    
void nk_cache_part_dump()
{
    int i;
    GLOBAL_LOCK_CONF;

    GLOBAL_LOCK();
    nk_vc_printf("Cache partitioning state\n");
    nk_vc_printf(" usable    = %d\n",cachetable.usable);
    nk_vc_printf(" thread    = %u%%\n",cachetable.thread_default_percent);
    nk_vc_printf(" interrupt = %u%%\n",cachetable.interrupt_percent);
    nk_vc_printf(" cbm_len   = %u\n",cachetable.cbm_length);
    nk_vc_printf(" cos_size  = %u\n",cachetable.cos_size);
    for (i=0;i<cachetable.cos_size;i++) {
	nk_vc_printf(" cos[%d]   = bitmask %08x refcount %u\n",i,cachetable.coses[i].bitmask,cachetable.coses[i].refcount);
    }
    for (i=0;i<cachetable.cbm_length;i++) {
	nk_vc_printf(" part[%d]  = refcount %u\n",i,cachetable.part_refcount[i]);
    }

    GLOBAL_UNLOCK();
}

// only one group at a time can change its caching constraints

typedef enum { ACQUIRE, SELECT, RELEASE } group_op_t;

typedef struct group_state {
    nk_cache_part_t part;
    int             fail_count;
} group_state_t;

volatile static group_state_t group_state;

static int do_group(nk_thread_group_t *group, group_op_t op, nk_cache_part_t part, uint32_t percent)
{
    int leader=0;
    int rc = 0;
    
    nk_thread_group_election(group);

    leader = nk_thread_group_check_leader(group)==1;

    if (leader) {
	// one at a time
	spin_lock(&group_state_lock);

	group_state.part = part;
	group_state.fail_count = 0;

	if (op == ACQUIRE) {
	    // actually do the acquire
	    if (nk_cache_part_acquire(percent)) {
		DEBUG("Leader failed to acquire\n");
		group_state.fail_count++;
		rc = -1;
	    } else {
		DEBUG("Leader has acquired partition\n");
		group_state.part = nk_cache_part_get_current();
	    }
	}
    }

    nk_thread_group_barrier(group);

    if (!leader && op == ACQUIRE) {
	if (group_state.fail_count) {
	    // probably leader failed
	    DEBUG("Failing to acquire as leader or follower has failed\n");
	    rc = -1;
	    // nothing to do...
	} else {
	    if (nk_cache_part_select(group_state.part)) {
		DEBUG("Follower failed to acquire\n");
		group_state.fail_count++;
		rc = -1;
	    } else {
		DEBUG("Follower has acquired partition\n");
	    }
	}
    } else if (op == SELECT) {
	if (nk_cache_part_select(part)) {
	    DEBUG("leader or follower has failed to select\n");
	    group_state.fail_count++;
	    rc = -1;
	} else {
	    DEBUG("leader or follower has selected\n");
	}
    } else if (op == RELEASE) {
	if (nk_cache_part_release(part)) {
	    DEBUG("leader or follower has failed to release\n");
	    group_state.fail_count++;
	    rc = -1;
	} else {
	    DEBUG("leader or follower has released\n");
	}
    }

    // now wait for everyone and see if we've failed
    nk_thread_group_barrier(group);

    if (group_state.fail_count) {
	DEBUG("Failure somewhere... reverting\n");
	// we failed somewhere, so revert back, if possible
	if ((op == ACQUIRE || op == SELECT) && !rc) {
	    // rollback if I succeeded but someone else failed
	    DEBUG("releasing\n");
	    nk_cache_part_release(group_state.part);
	} else if (op == RELEASE) {
	    // nothing to do...
	    // anyone who actually failed to release will leak
	    ERROR("Huh - failure on group release\n");
	}

	// everyone bombs back to the default
	nk_cache_part_select(NK_CACHE_PART_THREAD_DEFAULT);

	rc = -1;
    } 

    // One final barrier before the leader lets the next request in
    nk_thread_group_barrier(group);
    
    if (leader) {
	spin_unlock(&group_state_lock);
    }
	
    return rc;
    
}

int nk_cache_part_group_acquire(nk_thread_group_t *group, uint32_t percent)
{
    return do_group(group,ACQUIRE,0,percent);
}

int nk_cache_part_group_select(nk_thread_group_t *group, nk_cache_part_t part)
{
    return do_group(group,SELECT,part,0);
}

int nk_cache_part_group_release(nk_thread_group_t *group, nk_cache_part_t part)
{
    return do_group(group,RELEASE,part,0);
}

static int
handle_cp (char * buf, void * priv)
{
    nk_cache_part_dump();
    return 0;
}


static int
cptest (char * buf, void * priv)
{
    uint64_t size, iteration;
    int percent, flag, num_threads, shared;

    if (sscanf(buf, "cacheparttest %lu %lu %d %d %d", &size, &iteration, &percent, &num_threads, &shared) == 5) {
        nk_vc_printf("Testing cache partitioning with multiple threads\n");
        test_cat_multi_threads(size, iteration, percent, num_threads, shared);
        return 0;
    }

    if (sscanf(buf, "cacheparttest %lu %lu %d", &size, &iteration, &percent) == 3) {
        nk_vc_printf("Testing cache partitioning with a single thread\n");
        test_cat_single_thread(size, iteration, percent);
        return 0;
    }

    nk_vc_printf("Unhandled cachepart test\n");

    return 0;
}


static struct shell_cmd_impl cp_impl = {
    .cmd      = "cachepart",
    .help_str = "cachepart",
    .handler  = handle_cp,
};
nk_register_shell_cmd(cp_impl);

static struct shell_cmd_impl cptest_impl = {
    .cmd      = "cacheparttest",
    .help_str = "cacheparttest size iter percent [threads shared]",
    .handler  = cptest,
};
nk_register_shell_cmd(cptest_impl);
