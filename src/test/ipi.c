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
 * Copyright (c) 2016, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <nautilus/percpu.h>
#include <nautilus/atomic.h>
#include <nautilus/math.h>
#include <nautilus/naut_assert.h>
#include <nautilus/fs.h>
#include <nautilus/fprintk.h>
#include <nautilus/shell.h>
#include <nautilus/getopt.h>
#include <dev/apic.h>
#include <test/test.h>
#include <test/ipi.h>
#include <asm/bitops.h>

#define CEIL_DIV(x,y)  (((x)/(y)) + !!((x)%(y)))

// TODO: take in input file from command line
// TODO: get file output working (ext2 fixes)

#define IPI_PRINT(fmt, args...) \
    if (data->use_file) { \
        fprintk(data->fd, fmt, ##args); \
    } else { \
        nk_vc_printf_specific(data->vc, fmt, ##args); \
    } 

#define rdtscll(val)                    \
    do {                        \
    uint64_t tsc;                   \
    uint32_t a, d;                  \
    asm volatile("rdtsc" : "=a" (a), "=d" (d)); \
    *(uint32_t *)&(tsc) = a;            \
    *(uint32_t *)(((unsigned char *)&tsc) + 4) = d;   \
    val = tsc;                  \
    } while (0)


#define ERROR(fmt, args...) ERROR_PRINT("IPI: " fmt "\n", ##args)


static const char* exp_types[3] = {"ONEWAY", "ROUNDTRIP", "BROADCAST"};

static ipi_exp_data_t * glob_exp_data;

static inline const char*
type2str (ipi_exp_type_t type) 
{
    if (type < 3) {
        return exp_types[type];
    } 
    return "UNKNOWN";
}


static uint64_t ipi_oneway_end = 0;
static volatile int done = 0;


static inline void 
bset(unsigned int nr, unsigned long * addr)
{
        asm volatile("lock bts %1, %0"
            : "+m" (*(volatile long *)(addr)) : "Ir" (nr) : "memory");
}


static struct time_struct {
    uint64_t start_cycle;
    uint64_t end_cycle;
    uint64_t sum;
    uint64_t max;
    uint64_t min;
    volatile uint64_t  done;
} time;


static inline void __always_inline
time_start (void)
{
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    time.start_cycle = lo | ((uint64_t)(hi) << 32);
}


static inline void __always_inline
time_end (void)
{
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    time.end_cycle = lo | ((uint64_t)(hi) << 32);
}


static int
ping (excp_entry_t * excp, excp_vec_t vec, void *state)
{
    struct apic_dev * apic = per_cpu_get(apic);
    apic_write_icr(apic,
		   0,
		   APIC_DEL_MODE_FIXED | PONG_VEC);
    IRQ_HANDLER_END();
    return 0;
}



static int
pong (excp_entry_t * excp, excp_vec_t vec, void *state)
{
    rdtscll(ipi_oneway_end);
    IRQ_HANDLER_END();
    *(volatile int *)&done = 1;
    return 0;
}


static int
pong_bcast (excp_entry_t * excp, excp_vec_t vec, void *state)
{
    uint64_t tmp;
    ipi_exp_data_t * data = glob_exp_data;
    rdtscll(tmp);

    data->bcast_cntrs[my_cpu_id()] = tmp;
	bset(my_cpu_id(), (unsigned long *)data->ack_bitmap);
	IRQ_HANDLER_END();
	return 0;
}


static int
ipi_exp_setup (ipi_exp_data_t * data)
{
    nk_fs_fd_t fd;

    if (register_int_handler(PING_VEC, ping, NULL) != 0) {
        ERROR("Could not register int handler");
        return -1;
    }

    if (register_int_handler(PONG_VEC, pong, NULL) != 0) {
        ERROR("Could not register int handler");
        return -1;
    }

    if (register_int_handler(PONG_BCAST_VEC, pong_bcast, NULL) != 0) {
        ERROR("Could not register int handler");
        return -1;
    }

    // should we output to a file?
    if (data->use_file) {

        char fbuf[512] = {0};
        strncpy(fbuf, "rootfs:/", 8);
        strncpy((char*)(fbuf+8), data->fname, strnlen(data->fname, IPI_MAX_FNAME_LEN)+1);
        fd = nk_fs_open(fbuf, O_CREAT|O_RDWR, 0);

        if (fd == FS_BAD_FD) {
            nk_vc_printf("Could not open file %s\n", fbuf);
            return -1;
        }

        data->fd = fd;
    }

    // save the current VC. This is important for xcalls
    data->vc = nk_get_cur_vc();

    // save a global pointer for ipi handlers
    glob_exp_data = data;

	return 0;
}


static void
ipi_exp_cleanup (ipi_exp_data_t * data)
{
    if (data->use_file) {
        nk_fs_close(data->fd);
    }
}


static void
__ipi_measure_roundtrip (void * arg)
{
	ipi_exp_data_t * data = (ipi_exp_data_t*)arg;
	uint32_t trials       = data->trials;
	cpu_id_t cpu          = data->dst_core;
    struct apic_dev * apic = per_cpu_get(apic);
	unsigned remote_apic = nk_get_nautilus_info()->sys.cpus[cpu]->lapic_id;
    uint32_t i;

	/* warm it up */
    for (i = 0; i < trials; i++) {
		apic_ipi(apic, remote_apic, PING_VEC);
		while (!(*(volatile int*)&done));
		done = 0;
	}

    for (i = 0; i < trials; i++) {
    
        uint64_t end = 0, start = 0;
	
		apic_ipi(apic, remote_apic, PING_VEC);

        rdtscll(start);

        while (!(*(volatile int*)&done));

        rdtscll(end);

        if (start > end) {
            IPI_PRINT("ERROR: Strangeness occured in cycle count\n");
        }

		done = 0;

		IPI_PRINT("SC: %u TC: %u TRIAL: %u - %u cycles\n", 
                my_cpu_id(), 
                cpu,
                i,
                end-start);
	}

}


static void
__ipi_measure_oneway (void * arg)
{
	ipi_exp_data_t * data = (ipi_exp_data_t*)arg;
	uint32_t trials       = data->trials;
	cpu_id_t cpu          = data->dst_core;
    struct apic_dev * apic = per_cpu_get(apic);
    uint32_t i;

	unsigned remote_apic = nk_get_nautilus_info()->sys.cpus[cpu]->lapic_id;

	/* We don't really know how internal interrupt logic works at the
	 * remote end, but it can't hurt being paranoid and assuming
	 * that it needs to be warmed up
	 */
    for (i = 0; i < trials; i++) {
		apic_ipi(apic, remote_apic, PONG_VEC);
		while (!done);
		done = 0;
	}

    for (i = 0; i < trials; i++) {
    
        uint64_t start = 0;

		apic_ipi(apic, remote_apic, PONG_VEC);

        rdtscll(start);

        while ((*(volatile int*)&done) == 0) { }

		done = 0;

		IPI_PRINT("SC: %u TC: %u TRIAL: %u - %u cycles\n", 
					 my_cpu_id(), 
					 cpu,
					 i,
					 ipi_oneway_end-start);
	}
}


/*
 * Helper function to decide whether or not we
 * should initiate the measurement on the core
 * we're currently running on or some other
 * remote core (using cross-core calls)
 *
 * This function will block if it is the remote
 * case.
 */
static inline void 
ipi_measurement_dispatch (ipi_exp_data_t * data)
{
	if (data->src_core == my_cpu_id()) {
		data->measure_func((void*)data);
	} else {
		// wait on it to finish (last arg)
		smp_xcall(data->src_core, data->measure_func, (void*)data, 1);
	}
}


/*
 * Measures IPI latency between two particular cores
 * of interest
 */
static void
ipi_measure_pairwise_oto (ipi_exp_data_t * data)
{
	ipi_measurement_dispatch(data);
}


/*
 *	Measures IPI latencies between one particular
 *	source core of interest and all destination
 *	cores 
 */
static void
ipi_measure_pairwise_ota (ipi_exp_data_t * data)
{
	int i;
	
	for (i = 0; i < nk_get_num_cpus(); i++) {

		// don't send to myself
		if (i == data->src_core) {
			continue;
		}

		data->dst_core = i;

        ipi_measurement_dispatch(data);
	}
}


/*
 * Measures IPI latencies between all cores and
 * one particular destination core of interest
 */
static void
ipi_measure_pairwise_ato (ipi_exp_data_t * data)
{
	int i;

	for (i = 0; i < nk_get_num_cpus(); i++) {

		// don't send to myself
		if (i == data->dst_core) {
			continue;
		}

		// not strictly necessary, but here for symmetry
		data->src_core = i;

		ipi_measurement_dispatch(data);
	}

}


/* 
 * Measures either oneway or roundtrip latencies
 * between all pairs of cores, ignoring the diagonal
 */
static void
ipi_measure_pairwise_ata (ipi_exp_data_t * data)
{
	int i,j;
		
	for (i = 0; i < nk_get_num_cpus(); i++) {
		for (j = 0; j < nk_get_num_cpus(); j++) {

			// skip the diagonal (no self sends)
			if (i == j) {
				continue;
			}

			data->src_core = i;
			data->dst_core = j;

			ipi_measurement_dispatch(data);
		}
	}

}


/*
 * Top level function for pairwise IPIs (either
 * oneway or rountrip IPIs)
 */
static void
ipi_pairwise (ipi_exp_data_t * data) 
{
	if (data->src_type == TYPE_ONE) {
		if (data->dst_type == TYPE_ONE) {
			// one-to-one
			ipi_measure_pairwise_oto(data);
		} else {
			// one-to-all
			ipi_measure_pairwise_ota(data);
		}
	} else {
		if (data->dst_type == TYPE_ONE) {
			// all-to-one
			ipi_measure_pairwise_ato(data);
		} else {
			// all-to-all
			ipi_measure_pairwise_ata(data);
		}
	}
}


static inline void
cores_wait(ipi_exp_data_t * data)
{
    unsigned me    = (unsigned)my_cpu_id();
    unsigned lidx  = BIT_WORD(me); 
    uint64_t bmask = ~(BIT_MASK(me));
    int i;
    uint8_t ready = 0;

    while (!ready) {

        ready = 1;

        for (i = 0; i < data->bitmap_len; i++) {

            // reset if not all bitmaps are filled
            if (i == lidx) {
                if (data->ack_bitmap[i] != bmask) {
                    ready = 0;
                }
            } else {
                if (data->ack_bitmap[i] != 0xffffffffffffffff) {
                    ready = 0;
                }
            }
        }

    }
}


static int
bcast_setup (ipi_exp_data_t * data)
{
    unsigned nlong;
    volatile uint64_t * counters;
    volatile uint64_t * ack_map;

    // allocate core counters
    counters = malloc(sizeof(uint64_t)*nk_get_num_cpus());
    if (!counters) {
        nk_vc_printf("Could not allocate core counters in %s\n", __func__);
        return -1;
    }

    nlong   = CEIL_DIV(nk_get_num_cpus(),BITS_PER_LONG);
    ack_map = malloc(nlong*sizeof(uint64_t));
    if (!ack_map) {
        nk_vc_printf("Could not allocate core bitamp in %s\n", __func__);
        return -1;
    }

    data->bitmap_len  = nlong;
    data->ack_bitmap  = ack_map;
    data->bcast_cntrs = counters;

    return 0;
}


static void
bcast_cleanup (ipi_exp_data_t * data)
{
    free((void*)data->ack_bitmap);
    free((void*)data->bcast_cntrs);
}


static inline void
reset_bcast_data (ipi_exp_data_t * data)
{
    unsigned fill_bits; // number of unused bits in the bitmap at the end

	memset((void*)data->ack_bitmap, 0, sizeof(uint64_t)*data->bitmap_len);
	memset((void*)data->bcast_cntrs, 0, sizeof(uint64_t)*nk_get_num_cpus());

    fill_bits = data->bitmap_len*BITS_PER_LONG - nk_get_num_cpus();

    // set fill bits all to one
    if (fill_bits) {
        // max=BITS_PER_LONG-1, min=1
        data->ack_bitmap[data->bitmap_len-1] = ~((1ULL<<(BITS_PER_LONG-fill_bits))-1);
    }

}



static void
__ipi_measure_bcast (void * arg)
{
	ipi_exp_data_t * data  = (ipi_exp_data_t*)arg;
    struct apic_dev * apic = per_cpu_get(apic);
	uint32_t trials        = data->trials;
    uint64_t start = 0;
    int i;
		
    bcast_setup(data);

    reset_bcast_data(data);

	/* warm it up */
    for (i = 0; i < trials; i++) {
        apic_write(apic, APIC_REG_ESR, 0);
		apic_bcast_ipi(apic, PONG_BCAST_VEC);
		cores_wait(data);
	}

    reset_bcast_data(data);

    for (i = 0; i < trials; i++) {

        apic_write(apic, APIC_REG_ESR, 0);

        rdtscll(start);

		apic_bcast_ipi(apic, PONG_BCAST_VEC);

        cores_wait(data);

		int j;
		for (j = 0; j < nk_get_num_cpus(); j++) {

			if (j == my_cpu_id()) continue;

			if (start > data->bcast_cntrs[j]) {
				IPI_PRINT("ERROR: Strangeness occured in cycle count - start=%llu end=%llu\n", start, data->bcast_cntrs[j]);
			}

            IPI_PRINT("SC: %u TC: %u TRIAL: %u - %u cycles\n", 
                    my_cpu_id(),
                    j,
                    i,
                    data->bcast_cntrs[j]-start);
		}

        reset_bcast_data(data);

	}

    bcast_cleanup(data);
}


/* 
 * Measure IPI broadcast latencies from one 
 * particular source core
 */
static inline void 
ipi_measure_broadcast_ota (ipi_exp_data_t * data)
{
	ipi_measurement_dispatch(data);
}


/*
 * Measure IPI broadcast latencies from all
 * cores
 */
static void
ipi_measure_broadcast_ata (ipi_exp_data_t * data)
{
	int i;

	for (i = 0; i < nk_get_num_cpus(); i++) {
		data->src_core = i;
		ipi_measurement_dispatch(data);
	}
}


/*
 * Top level function for broadcast IPIs. Only 
 * two options here, either all sources, or one
 * source
 */
static void
ipi_broadcast (ipi_exp_data_t * data)
{
	if (data->src_type == TYPE_ONE) {
		ipi_measure_broadcast_ota(data);
	} else {
		ipi_measure_broadcast_ata(data);
	}
}


static void
ipi_print_parms (ipi_exp_data_t * data) 
{
    IPI_PRINT("# IPI experiment:\n");
    IPI_PRINT("#    TYPE: %s\n", type2str(data->type));
    IPI_PRINT("#    TRIALS: %u\n", data->trials);
    IPI_PRINT("#    SRC_TYPE: %s\n", (data->src_type == TYPE_ONE) ? "ONE" : "ALL");
    IPI_PRINT("#    SRC_CORE: %d\n", (data->src_type == TYPE_ONE) ? data->src_core : -1);
    IPI_PRINT("#    DST_TYPE: %s\n", (data->dst_type == TYPE_ONE) ? "ONE" : "ALL");
    IPI_PRINT("#    DST_CORE: %d\n", (data->dst_type == TYPE_ONE) ? data->dst_core : -1);
    if (data->use_file) {
        IPI_PRINT("#    OUT_FILE: %s\n", data->fname);
    }
}


/*
 * This function is the top-level (and only external) interface
 * for IPI measurements. Invocation from the shell will lead here.
 */
int
ipi_run_exps (ipi_exp_data_t * data)
{
    // NOTE: can use IPI_PRINT macro after this call
	if (ipi_exp_setup(data) != 0) {
		nk_vc_printf("ERROR: could not set up ipi experiment\n");
		return -1;
	}

    ipi_print_parms(data);

	switch (data->type) {
		case EXP_ONEWAY:
			data->measure_func = __ipi_measure_oneway;
			ipi_pairwise(data);
			break;
		case EXP_ROUNDTRIP:
			data->measure_func = __ipi_measure_roundtrip;
			ipi_pairwise(data);
			break;
		case EXP_BROADCAST:
			data->measure_func = __ipi_measure_bcast;
			ipi_broadcast(data);
			break;
		default:
			nk_vc_printf("ERROR: invalid experiment type %d\n", data->type);
			return -1;
	}
			
    ipi_exp_cleanup(data);

	return 0;
}


static int
handle_ipitest (char * buf, void * priv)
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
#endif
    }

    // which source type is it 
	if (sscanf(buf, "-s %u", &sid)==1) {
        data->src_type = TYPE_ONE;
        data->src_core = sid; 
        buf += 3;

        // skip over src core
        while (*buf && *buf!=' ') { buf++;}

        // find next arg
        while (*buf && *buf==' ') { buf++;}

	} else { 
        data->src_type = TYPE_ALL;
    }

        
    if (sscanf(buf, "-d %u", &did)==1) {
        data->dst_type = TYPE_ONE;
        data->dst_core = did;
    } else {
        data->dst_type = TYPE_ALL;
    }
		
	if (ipi_run_exps(data) != 0) {
        nk_vc_printf("Could not run ipi experiment\n");
        return 0;
    }

    free(data);

	return 0;
}


static struct shell_cmd_impl ipitest_impl = {
    .cmd      = "ipitest",
    .help_str = "ipitest type (oneway | roundtrip | broadcast) trials [-f <filename>] [-s <src_id> | all] [-d <dst_id> | all]",
    .handler  = handle_ipitest,
};
nk_register_shell_cmd(ipitest_impl);


static int
handle_ipi_test_cmd (int argc, char ** argv)
{
    int c;
	ipi_exp_data_t * data = malloc(sizeof(ipi_exp_data_t));
	if (!data) {
		nk_vc_printf("ERROR: could not allocate IPI experiment data\n");
		return -1;
	}
	memset(data, 0, sizeof(ipi_exp_data_t));

    // defaults
    data->type     = EXP_ONEWAY;
    data->src_type = TYPE_ALL;
    data->src_core = 0;
    data->dst_type = TYPE_ALL;
    data->dst_core = 1;
    data->trials   = 10;

    INFO_PRINT("ARGC: %d\n", argc);

    while ((c = getopt(argc, argv, "t:n:s:d:")) != -1) {
        switch (c) {
            case 't': {
                if (strcmp(optarg, "oneway") == 0) {
                    data->type = EXP_ONEWAY;
                } else if (strcmp(optarg, "roundtrip") == 0) {
                    data->type = EXP_ROUNDTRIP;
                } else if (strcmp(optarg, "broadcast") == 0) {
                    data->type = EXP_BROADCAST;
                } else {
                    ERROR("Unknown test type");
                    return -1;
                }
                break;
                      }
            case 'n': {
                data->trials = atoi(optarg);
                if (data->trials <= 0) {
                    ERROR("Must have >0 trials");
                    return -1;
                }
                break;
                      }
            case 's': 
            case 'd':
                      {
                if (strcmp(optarg, "all") == 0) {
                    if (c == 's') {
                        data->src_type = TYPE_ALL;
                        data->src_core = -1;
                    } else {
                        data->dst_type = TYPE_ALL;
                        data->dst_core = -1;
                    }
                } else {
                    int core = atoi(optarg);
                    if (core >= nk_get_num_cpus()) {
                        ERROR("CPU ID (%d) too large, max is %d\n", core, nk_get_num_cpus()-1);
                        return -1;
                    }

                    if (c == 's') {
                        data->src_type = TYPE_ONE;
                        data->src_core = core;
                    } else {
                        data->dst_type = TYPE_ONE;
                        data->dst_core = core;
                    }
                }
                break;
                      }
            default: {
                ERROR("Unknown IPI test option");
                return -1;
            }
        }
    }

    ipi_run_exps(data);
    
    free(data);
    return 0;
}

static struct nk_test_impl ipitest_test_impl = {
    .name         = "ipitest",
    .handler      = handle_ipi_test_cmd,
    .default_args = "-t oneway -n 10 -s all -d all",
};

nk_register_test(ipitest_test_impl);
