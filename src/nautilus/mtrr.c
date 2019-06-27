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
 * Copyright (c) 2018, Peter Dinda
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/smp.h>
#include <nautilus/cpuid.h>
#include <nautilus/msr.h>
#include <nautilus/shell.h>
#include <nautilus/mtrr.h>

#define NAUT_CONFIG_MTRR_DEBUG 0
#define NAUT_CONFIG_MTRR_CAPTURE_AT_BOOT         1
#define NAUT_CONFIG_MTRR_CORRECT_AT_BOOT         1
#define NK_MTRR_FIXED_COUNT  88
#define NK_MTRR_MAX_VARIABLE 256

#if NAUT_CONFIG_MTRR_DEBUG
#define DEBUG(fmt, args...)  DEBUG_PRINT("mtrr: " fmt, ##args)
#else
#define DEBUG(fmt, args...)
#endif

#define ERROR(fmt, args...) ERROR_PRINT("mtrr: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("mtrr: " fmt, ##args)


int 
nk_mtrr_get_features (int *num_var, int *flags)
{
    cpuid_ret_t c;

    cpuid(0x1,&c);
    if (c.d>>12 & 0x1) {
        // have MTRRs
        uint64_t m = msr_read(0xfe);
        *flags = 0;
        if (m>>8 & 0x1) {
            *flags |= NK_MTRR_HAS_FIXED;
        }
        if (m>>10 & 0x1) {
            *flags |= NK_MTRR_HAS_WC;
        }
        if (m>>11 & 0x1) {
            *flags |= NK_MTRR_HAS_SMRR;
        }
        *num_var = m & 0xff;
        return 0;
    } else {
        return -1;
    }
}


int 
nk_mtrr_get_default (int *enable, int *fixed_enable, uint8_t *type)
{
    uint64_t m = msr_read(0x2ff);
    *enable = m>>11 & 0x1;
    *fixed_enable = m >>10 & 0x1;
    *type = m & 0xff;
    return 0;
}


int 
nk_mtrr_set_default (int enable, int fixed_enable, uint8_t type)
{
    uint64_t m = 0;
    m = type;
    m |= (!!enable) << 11;
    m |= (!!fixed_enable) << 10;
    msr_write(0x2ff, m);
    return 0;
}


int 
nk_mtrr_addr_to_fixed_num (void *addr)
{
    uint64_t a = (uint64_t)addr;

    if (a<0x80000) {
        return a / 0x10000;
    }
    if (a<0xc0000) {
        return 8 + (a - 0x80000)/0x4000;
    }
    if (a<0x100000) {
        return 24 + (a - 0xc0000)/0x1000;
    }

    ERROR("Attempt to resolve %p to fixed mtrr\n", addr);

    return -1;
}


static uint32_t 
nk_mtrr_fixed_num_to_msr (int num)
{
    if (num<0) {
        goto out;
    }

    if (num<8) {
        return 0x250;
    }

    if (num<24) {
        return 0x258 + (num>=16);
    }

    if (num<NK_MTRR_FIXED_COUNT) {
        return 0x268 + (num-24)/8;
    }
out:
    ERROR("Attempt to resolve out-of-range fixed mtrr %d to msr\n",num);
    return -1;
}


int 
nk_mtrr_fixed_num_to_range (int num, void **start, void **end)
{
    if (num<0) {
        goto out;
    }

    if (num<8) {
        *start = (void*)(uint64_t)(num*0x10000);
        *end = *start+0x10000;
        return 0;
    }

    if (num<24) {
        *start = (void*)(uint64_t)(0x80000 + (num-8) * 0x4000);
        *end = *start + 0x4000;
        return 0;
    }

    if (num<NK_MTRR_FIXED_COUNT) {
        *start =  (void*)(uint64_t)(0xc0000 + ( num-24) * 0x1000);
        *end = *start + 0x1000;
        return 0;
    }

out:
    ERROR("Attempt to resolve out-of-range fixed mtrr %d to range\n",num);
    return -1;
}


int 
nk_mtrr_get_fixed (int num, uint8_t *type)
{
    int msr = nk_mtrr_fixed_num_to_msr(num);

    if (msr>0) {
        uint64_t m = msr_read(msr);
        *type = m >> (8 * (num % 8)) & 0xff;
        return 0;
    } else {
        return -1;
    }
}


int 
nk_mtrr_set_fixed (int num, uint8_t type)
{
    int msr = nk_mtrr_fixed_num_to_msr(num);

    if (msr>0) {
        uint64_t m = msr_read(msr);
        //DEBUG("read msr as 0x%lx\n",m);
        // zero what's there
        uint64_t mask = 0xffULL;
        mask <<= (8 * (num % 8));
        m &= ~mask;
        // write what we need
        mask = type;
        mask <<= (8 * (num % 8));
        m |= mask;
        // push to msr
        msr_write(msr,m);
        return 0;
    } else {
        return -1;
    }
}


#define SIZE52 0xfffffffffffffULL

static uint64_t or_mask=-1, and_mask=-1;
static uint8_t  phys_addr_bits;

#define SMRR_OR_MASK 0xffffffff00000000ULL

static inline 
void gen_size_masks (void) 
{
    if (or_mask==-1) { 
        cpuid_ret_t c;

        // assumes Intel and AMD have same interface here
        cpuid(0x80000000, &c);

        if (c.a >= 0x80000008) {
            DEBUG("Machine has phys addr bits cpuid\n");
            cpuid(0x80000008,&c);
            phys_addr_bits = c.a & 0xff;
        } else {
            phys_addr_bits = 36;
        }

        DEBUG("Physical Address Space: %d bits\n", phys_addr_bits);

        //or_mask = ~((1ULL<<(phys_addr_bits - 12))-1);
        // and_mask = ~or_mask & 0xfffff00000ULL;
        or_mask  = ~((1ULL<<phys_addr_bits) - 1);
        and_mask = ~or_mask & 0xfffff00000000ULL;

        DEBUG("or_mask=0x%lx and_mask=0x%lx\n",or_mask,and_mask);
    }
}


static inline int 
variable_match (void *ptr, int num, uint8_t *type)
{
    uint64_t b, m, t, pb, pm;

    gen_size_masks();

    b = msr_read(0x200 + num*2);
    m = msr_read(0x200 + num*2 + 1);

    pb = b>>12;
    pm = m>>12;

    t = ((uint64_t)ptr)>>12;

    if  (((pm & pb) == (pm & t)) && (m>>11 & 0x1)) {
        // match means in region and region is valid
        *type = b & 0xff;
        return 1;
    } else {
        return 0;
    }
}
    

int 
nk_mtrr_get_variable (int num, 
                      void **start, 
                      void **end, 
                      uint8_t *type, 
                      int *valid)
{
    int count, flags;
    uint64_t b,m;

    if (nk_mtrr_get_features(&count,&flags) || num<-1 || num>=count) {
        return -1;
    }


    if (num>=0) { 
        b = msr_read(0x200 + num*2);
        m = msr_read(0x200 + num*2 + 1);
    } else {
        // SMRR
        b = msr_read(0x1f2);
        m = msr_read(0x1f3);
    }

    DEBUG("b=0x%lx m=0x%lx\n",b,m);

    *valid = (m>>11 & 0x1);

    DEBUG("valid=%d\n", *valid);

    *type = b & 0xff;

    DEBUG("type=0x%x\n",*type);

    *start = (void*)(b & ~0xfff);

    m &= ~0xfff;  // kill all but mask

    if (num>=0) { 
        gen_size_masks();
        *end = *start - (m | or_mask);
    } else {
        // SMRR
        // the base and mask are known to be 20 bits (pages) long,
        // as if the physical address size is 32 bits
        *start = (void*)(((uint64_t)*start) & 0xffffffffULL);
        *end = *start - (m | SMRR_OR_MASK);
    }

    DEBUG("get %d as start=0x%lx, end=0x%lx, type=0x%x, valid=%d\n", num, *start, *end, *type, *valid);

    return 0;
}

	
int 
nk_mtrr_set_variable (int num, void *start, void *end, uint8_t type, int valid)
{
    int count, flags;
    uint64_t sm1, b, m;

    if (nk_mtrr_get_features(&count,&flags) || num<-1 || num>=count) {
	ERROR("Invalid set request count=%d, flags=%x\n", count, flags);
        return -1;
    }

    DEBUG("set %d start=%p, end=%p, type=0x%x, valid=%d\n", num, start,end,type,valid);
    
    b = ((uint64_t)start & ~0xfff) + type;

    DEBUG("b=%p\n",b);

    sm1 = ((uint64_t)end - (uint64_t)start) - 1;

    DEBUG("sm1=%p\n",sm1);
    
    if (num >= 0) {
        gen_size_masks();
        m = (((~or_mask) - sm1) & ~0xfff) | (valid<<11);
        DEBUG("m=%p\n",m);
        msr_write(0x200 + num*2, b);
        msr_write(0x200 + num*2 + 1, m);
    } else {
        // SMRR must be 32 bit address space
        m = (((~SMRR_OR_MASK) - sm1) & ~0xfff) | (valid<<11);
        DEBUG("m=%p\n",m);
        msr_write(0x1f2, b);
        msr_write(0x1f3, m);
    }
    
    DEBUG("set %d to [%p,%p) type 0x%x valid=%d, b=0x%lx, m=0x%lx\n",
	  num, start, end, type, valid, b, m);

    return 0;
}

#define DEC(x) ({ x==NK_MTRR_TYPE_UC ? "uncacheable" : x==NK_MTRR_TYPE_WC ? "writecombining" : x==NK_MTRR_TYPE_WT ? "writethrough" : x==NK_MTRR_TYPE_WP ? "writeprotect" : x==NK_MTRR_TYPE_WB ? "writeback" : "UNKNOWN"; })


int 
nk_mtrr_find_type (void *addr, uint8_t *type, char **typestr)
{
    int num, flags,i;
    int found = 0;
    uint8_t curtype;

    if (nk_mtrr_get_features(&num, &flags)) {
        return -1;
    }

    if ((flags & NK_MTRR_HAS_FIXED) && ((uint64_t)addr)<(1<<20)) {
        if (nk_mtrr_get_fixed(nk_mtrr_addr_to_fixed_num(addr),type)) {
            return -1;
        } else {
            *typestr = DEC(*type);
        }
        return 0;
    }

    // need to aggregate across every matching
    *type = NK_MTRR_TYPE_WB;
    for (i = 0; i < num; i++) {
        if (variable_match(addr,i,&curtype)) {
            DEBUG("matches region %d type 0x%x\n",i,curtype);
            if (curtype==NK_MTRR_TYPE_UC) {
                // UC has precedence over everything
                *type=NK_MTRR_TYPE_UC;
            } else if (curtype == NK_MTRR_TYPE_WT && *type == NK_MTRR_TYPE_WB) {
                // WT has precedence over WB
                *type=NK_MTRR_TYPE_WT;
            } else {
                *type=curtype;
            }
            found=1;
        }
    }

    if (found) {
        // found in variable regions
        *typestr = DEC(*type);
        return 0;
    }

    // last resort, default region
    nk_mtrr_get_default(&num, &flags, type);

    *typestr = DEC(*type);

    return 0;
}


static void 
mtrr_dump_xcall (void *arg)
{
    struct nk_virtual_console *c = (struct nk_virtual_console *)arg;
    
    int num, flags;
    int en, fen, valid;
    uint8_t type;
    void *start, *end;
    int i;
    
    if (nk_mtrr_get_features(&num,&flags)) {
        nk_vc_printf_specific(c,"CPU %d does not have MTRR support\n", my_cpu_id());
        return;
    }
    
    nk_vc_printf_specific(c,"CPU %d MTRR %s %s %s %d variable regions\n", my_cpu_id(),
			  flags & NK_MTRR_HAS_FIXED ? "fixed" : "",
			  flags & NK_MTRR_HAS_WC ? "writecombining" : "",
			  flags & NK_MTRR_HAS_SMRR ? "smrr" : "",
			  num);
    
    nk_mtrr_get_default(&en, &fen, &type);

    nk_vc_printf_specific(c," default region => %s %s 0x%x %s\n",
			  en ? "enabled" : "",
			  fen ? "fixed_enabled" : "",
			  type, DEC(type));
    
    if (flags & NK_MTRR_HAS_SMRR) {
        nk_mtrr_get_variable(-1, &start,&end,&type,&valid);
        nk_vc_printf_specific(c, " smrr region => [%016p, %016lp) type 0x%x %s %s\n",
                start, end, type, DEC(type), valid ? "valid" : "invalid");
    } else {
        nk_vc_printf_specific(c," smrr region => NONE\n");
    }
    
    nk_vc_printf_specific(c," fixed regions\n");

    for (i=0;i<NK_MTRR_FIXED_COUNT;i++) {
        nk_mtrr_fixed_num_to_range(i,&start,&end);
        nk_mtrr_get_fixed(i,&type);
        nk_vc_printf_specific(c,"  [%016p,%016p) => 0x%x %s\n", start, end, type, DEC(type));
    }

    nk_vc_printf_specific(c," variable regions\n");

    for (i=0;i<num;i++) {
        nk_mtrr_get_variable(i,&start,&end,&type,&valid);
        nk_vc_printf_specific(c,"  [%016p,%016p) => 0x%x %s %s\n", start, end, type, DEC(type), valid ? "valid" : "invalid");
    }
}


void 
nk_mtrr_dump (int cpu)
{
    if (cpu == -1) {
        uint32_t i;
        for (i = 0; i < nk_get_num_cpus(); i++) {
            if (i == my_cpu_id()) {
                mtrr_dump_xcall(get_cur_thread()->vc);
            } else {
                smp_xcall(i, mtrr_dump_xcall, get_cur_thread()->vc, 1);
            }
        }
    } else {
        if (cpu==my_cpu_id()) {
            mtrr_dump_xcall(get_cur_thread()->vc);
        } else {
            smp_xcall(cpu, mtrr_dump_xcall, get_cur_thread()->vc, 1);
        }
    }
}


#if !NAUT_CONFIG_MTRR_CAPTURE_AT_BOOT && !NAUT_CONFIG_MTRR_CORRECT_AT_BOOT

int nk_mtrr_init()
{
    return 0;
}

int nk_mtrr_init_ap()
{
    return 0;
}

void nk_mtrr_deinit()
{
}

#else

struct nk_variable_mttr {
    void    *start;
    void    *end;
    uint8_t type;
    int     valid;
};

struct nk_mttr_config {
    int      valid;
    int      num_var;
    int      flags;
    int      enable;
    int      fixed_enable;
    uint8_t  default_type;
    uint8_t  fixed_types[NK_MTRR_FIXED_COUNT];
    struct nk_variable_mttr smrr;
    struct nk_variable_mttr var[NK_MTRR_MAX_VARIABLE];
};

static struct nk_mttr_config global_mtrrs;


int nk_mtrr_init()
{
    
    DEBUG("init\n");

    global_mtrrs.valid = 0;

    int i;
 	
    if (nk_mtrr_get_features(&global_mtrrs.num_var,&global_mtrrs.flags)) {
	return 0;
    }

    global_mtrrs.valid = 1;
    
    if (global_mtrrs.num_var > NK_MTRR_MAX_VARIABLE) {
	ERROR("only capturing first %d variable mttrs out of %d\n", NK_MTRR_MAX_VARIABLE, global_mtrrs.num_var);
	global_mtrrs.num_var = NK_MTRR_MAX_VARIABLE;
    }

    if (nk_mtrr_get_default(&global_mtrrs.enable,&global_mtrrs.fixed_enable,&global_mtrrs.default_type)) {
	ERROR("System has no default MTRR?!\n");
	return -1;
    }

    if (global_mtrrs.flags & NK_MTRR_HAS_FIXED) { 
	for (i=0;i<NK_MTRR_FIXED_COUNT;i++) {
	    if (nk_mtrr_get_fixed(i,&global_mtrrs.fixed_types[i])) {
		ERROR("Failed to capture fixed MTRR %d\n",i);
		return -1;
	    }
	}
    }

    if (global_mtrrs.flags & NK_MTRR_HAS_SMRR) {
	if (nk_mtrr_get_variable(-1,&global_mtrrs.smrr.start, &global_mtrrs.smrr.end, &global_mtrrs.smrr.type, &global_mtrrs.smrr.valid)) {
	    ERROR("Failed to capture SMRR\n");
	    return -1;
	}
    }

    for (i=0;i<global_mtrrs.num_var;i++) {
	if (nk_mtrr_get_variable(i,&global_mtrrs.var[i].start, &global_mtrrs.var[i].end, &global_mtrrs.var[i].type, &global_mtrrs.var[i].valid)) {
	    ERROR("Failed to capture variable MTRR %d\n",i);
	    return -1;
	}
    }
    
    return 0;
}

#if !NAUT_CONFIG_MTRR_CORRECT_AT_BOOT

int 
nk_mtrr_init_ap (void)
{
    return 0;
}

#else

int 
nk_mtrr_init_ap (void)
{
    struct nk_mttr_config my_mtrrs;
    int d = 0;
    int s = 0;
    int f = 0;
    int v = 0;
    int i;

    DEBUG("init\n");

    if (!global_mtrrs.valid) {
        return 0;
    }

    if (nk_mtrr_get_features(&my_mtrrs.num_var,&my_mtrrs.flags)) {
        return -1;
    }

    my_mtrrs.valid = 1;

#define CHECK(x,p) \
    ({ int bad; \
     if (my_mtrrs.x != global_mtrrs.x) {				\
     if (p) {DEBUG("global/local mismatch: global has 0x%lx " #x " while local has 0x%lx\n", global_mtrrs.x, my_mtrrs.x);} \
     bad=1;							\
     } else { bad=0;}\
     bad;		\
     })

    if (CHECK(num_var,1)) { return -1; }
    if (CHECK(flags,1)) { return -1;}

    if (nk_mtrr_get_default(&my_mtrrs.enable,&my_mtrrs.fixed_enable,&my_mtrrs.default_type)) {
        ERROR("Cannot read default MTRR?!\n");
        return -1;
    }

    if (CHECK(enable,1) || CHECK(fixed_enable,1) || CHECK(default_type,1)) {
        DEBUG("Attempting to fix default mismatch\n");
        if (nk_mtrr_set_default(global_mtrrs.enable,global_mtrrs.fixed_enable,global_mtrrs.default_type)) {
            ERROR("Failed to correct default, giving up!\n");
            return -1;
        }
        d++;
    }

    if (my_mtrrs.flags & NK_MTRR_HAS_FIXED) { 
        for (i=0;i<NK_MTRR_FIXED_COUNT;i++) {
            if (nk_mtrr_get_fixed(i,&my_mtrrs.fixed_types[i])) {
                ERROR("Failed to capture fixed MTRR %d\n",i);
                return -1;
            }
            if (CHECK(fixed_types[i],1)) {
                DEBUG("Attempting to fix fixed MTRR %d\n",i);
                if (nk_mtrr_set_fixed(i,global_mtrrs.fixed_types[i])) {
                    ERROR("Failed to correct fixed MTRR %d, giving up\n", i);
                    return -1;
                }
                f++;
            }
        }
    }

    if (my_mtrrs.flags & NK_MTRR_HAS_SMRR) {
        if (nk_mtrr_get_variable(-1,&my_mtrrs.smrr.start, &my_mtrrs.smrr.end, &my_mtrrs.smrr.type, &my_mtrrs.smrr.valid)) {
            ERROR("Failed to capture SMRR\n");
            return -1;
        }
        if (CHECK(smrr.start,1) || CHECK(smrr.end,1) || CHECK(smrr.type,1) || CHECK(smrr.valid,1)) {
            DEBUG("Attempting to fix SMRR\n");
            // this is probably a bad idea since we would expect smrr to be locked up by this point...
            if (nk_mtrr_set_variable(-1,global_mtrrs.smrr.start, global_mtrrs.smrr.end, global_mtrrs.smrr.type, global_mtrrs.smrr.valid)) {
                ERROR("Failed to correct SMRR, giving up\n");
                return -1;
            }
            s++;
        }
    }

    for (i=0;i<my_mtrrs.num_var;i++) {
        if (nk_mtrr_get_variable(i,&my_mtrrs.var[i].start, &my_mtrrs.var[i].end, &my_mtrrs.var[i].type, &my_mtrrs.var[i].valid)) {
            ERROR("Failed to capture variable MTRR %d\n",i);
            return -1;
        }
        if (CHECK(var[i].start,1) || CHECK(var[i].end,1) || CHECK(var[i].type,1) || CHECK(var[i].valid,1)) {
            DEBUG("Attempting to fix variable MTRR %d\n",i);
            if (nk_mtrr_set_variable(i,global_mtrrs.var[i].start, global_mtrrs.var[i].end, global_mtrrs.var[i].type, global_mtrrs.var[i].valid)) {
                ERROR("Failed to fix variable MTRR %d, giving up\n",i);
                return -1;
            }
            v++;
        }

    }

    if ((d+f+s+v)>0) {
        INFO("%d MTRR mismatches detected with corrections attempted (%d default, %d fixed, %d smrr, %d variable)\n",(d+f+s+v),d,f,s,v);
    } else {
        DEBUG("cpu matches BSP\n");
    }

    return 0;
}

#endif

void 
nk_mtrr_deinit (void)
{
    DEBUG("deinit\n");
}

#endif

static int
handle_mtrrs (char * buf, void * priv)
{
    int cpu;

    if ((sscanf(buf,"mtrrs %d", &cpu)==1) ||
            (cpu=-1, !strcmp(buf,"mtrrs"))) {
        nk_mtrr_dump(cpu);
        return 0;
    }

    nk_vc_printf("invalid mtrr command\n");

    return 0;
}

static struct shell_cmd_impl mtrrs_impl = {
    .cmd      = "mtrrs",
    .help_str = "mtrrs [cpu]",
    .handler  = handle_mtrrs,
};
nk_register_shell_cmd(mtrrs_impl);


static int
handle_mt (char * buf, void * priv)
{
    uint64_t addr;

    if (sscanf(buf,"mt %lx", &addr)==1) {
        uint8_t type;
        char *typestr;

        if (nk_mtrr_find_type((void*)addr,&type,&typestr)) {
            nk_vc_printf("Cannot find memory type for %p\n",(void*)addr);
        } else {
            nk_vc_printf("Mem[0x%016lx] has type 0x%02x %s\n", addr, type, typestr);
        }
        return 0;
    }

    nk_vc_printf("invalid mt command\n");

    return 0;
}

static struct shell_cmd_impl mt_impl = {
    .cmd      = "mt",
    .help_str = "mt addr",
    .handler  = handle_mt,
};
nk_register_shell_cmd(mt_impl);
