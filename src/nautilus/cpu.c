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
 * Copyright (c) 2015, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/cpu.h>
#include <nautilus/cpuid.h>
#include <nautilus/percpu.h>
#include <nautilus/naut_types.h>
#include <nautilus/naut_string.h>
#include <nautilus/backtrace.h>
#include <nautilus/shell.h>
#include <nautilus/topo.h>
#include <nautilus/irq.h>
#include <dev/i8254.h>


static int
get_vendor_string (char name[13])
{
    cpuid_ret_t ret;
    cpuid(0, &ret);
    ((uint32_t*)name)[0] = ret.b;
    ((uint32_t*)name)[1] = ret.d;
    ((uint32_t*)name)[2] = ret.c;
    name[12] = '\0';
    return ret.a;
}


uint8_t 
nk_is_amd (void) 
{
    char name[13];
    get_vendor_string(name);
    return !strncmp(name, "AuthenticAMD", 13);
}

uint8_t
nk_is_intel (void)
{
    char name[13];
    get_vendor_string(name);
    return !strncmp(name, "GenuineIntel", 13);
}


uint32_t
nk_topo_get_smt_id (struct cpu * cpu)
{
    return cpu->coord->smt_id;
}


uint32_t
nk_topo_get_my_smt_id (void)
{
    return nk_topo_get_smt_id(get_cpu());
}


uint32_t
nk_topo_get_socket_id (struct cpu * cpu)
{
    return cpu->coord->pkg_id;
}


uint32_t
nk_topo_get_my_socket_id (void)
{
    return nk_topo_get_socket_id(get_cpu());
}

uint32_t
nk_topo_get_phys_core_id (struct cpu * cpu)
{
    return cpu->coord->core_id;
}

uint32_t
nk_topo_get_my_phys_core_id (void)
{
    return nk_topo_get_phys_core_id(get_cpu());
}

uint8_t
nk_topo_cpus_share_phys_core (struct cpu * a, struct cpu * b)
{
    return nk_topo_cpus_share_socket(a, b) && (a->coord->core_id == b->coord->core_id);
}

uint8_t
nk_topo_same_phys_core_as_me (struct cpu * other)
{
    return nk_topo_cpus_share_phys_core(get_cpu(), other);
}


uint8_t
nk_topo_cpus_share_socket (struct cpu * a, struct cpu * b)
{
    return a->coord->pkg_id == b->coord->pkg_id;
}

uint8_t
nk_topo_same_socket_as_me (struct cpu * other)
{
    return nk_topo_cpus_share_socket(get_cpu(), other);
}

static uint8_t (*const cpu_filter_funcs[])(struct cpu*, struct cpu*) = 
{
    [NK_TOPO_PHYS_CORE_FILT] = nk_topo_cpus_share_phys_core,
    [NK_TOPO_SOCKET_FILT]    = nk_topo_cpus_share_socket,
};


// We assume CPUs are not changing at this point, so no locks necessary
void
nk_topo_map_sibling_cpus (void (func)(struct cpu * cpu, void * state), nk_topo_filt_t filter, void * state)
{
    struct sys_info * sys = per_cpu_get(system);
    int i;

    if (filter != NK_TOPO_PHYS_CORE_FILT && filter != NK_TOPO_SOCKET_FILT && filter != NK_TOPO_ALL_FILT) {
        ERROR_PRINT("Sibling CPU mapping only supports socket-level and physcore-level mapping\n");
        return;
    }

    for (i = 0; i < nk_get_num_cpus(); i++) {
		if (i == my_cpu_id())
			continue;

        if (filter == NK_TOPO_ALL_FILT || cpu_filter_funcs[filter](get_cpu(), sys->cpus[i])) 
            func(sys->cpus[i], state);
    }
}

void 
nk_topo_map_core_sibling_cpus (void (func)(struct cpu * cpu, void * state), void * state)
{
    nk_topo_map_sibling_cpus(func, NK_TOPO_PHYS_CORE_FILT, state);
}

void 
nk_topo_map_socket_sibling_cpus (void (func)(struct cpu * cpu, void * state), void * state)
{
    nk_topo_map_sibling_cpus(func, NK_TOPO_SOCKET_FILT, state);
}

static void
topo_test (struct cpu * cpu, void * state)
{
    nk_vc_printf("MAPPER: Applying func to CPU %d\n", cpu->id);
}

static int
handle_cputopotest (char * buf, void * priv)
{
    char aps;
    if (((aps='a', strcmp(buf,"cputopotest a"))==0) ||
			((aps='p', strcmp(buf,"cputopotest p"))==0) ||
			((aps='s', strcmp(buf,"cputopotest s"))==0)) {
        switch (aps) { 
            case 'a': 
				nk_vc_printf("Mapping func to all siblings\n");
                nk_topo_map_sibling_cpus(topo_test, NK_TOPO_ALL_FILT, NULL);
                break;
            case 'p': 
				nk_vc_printf("Mapping func to core siblings\n");
                nk_topo_map_core_sibling_cpus(topo_test, NULL);
                break;
            case 's': 
				nk_vc_printf("Mapping func to socket siblings\n");
                nk_topo_map_socket_sibling_cpus(topo_test, NULL);
                break;
            default:
                nk_vc_printf("Unknown cputopotest command requested\n");
                return -1;
        }
    }

    return 0;
}

static struct shell_cmd_impl cputopo_impl = {
    .cmd      = "cputopotest",
    .help_str = "cputopotest <a|p|s>",
    .handler  = handle_cputopotest,
};
nk_register_shell_cmd(cputopo_impl);


/*
 * nk_detect_cpu_freq
 *
 * detect this CPU's frequency in KHz
 *
 * returns freq on success, 0 on error
 *
 */
ulong_t
nk_detect_cpu_freq (uint32_t cpu) 
{
    uint8_t flags = irq_disable_save();
    ulong_t khz = i8254_calib_tsc();
    if (khz == ULONG_MAX) {
        ERROR_PRINT("Unable to detect CPU frequency\n");
        goto out_err;
    }

    irq_enable_restore(flags);
    return khz;

out_err:
    irq_enable_restore(flags);
    return -1;
}


static int
handle_regs (char * buf, void * priv)
{
    extern int nk_interrupt_like_trampoline(void (*)(struct nk_regs *));
    uint64_t tid;

    if (sscanf(buf,"regs %lu",&tid) == 1) { 
        nk_thread_t *t = nk_find_thread_by_tid(tid);
        if (!t) {
            nk_vc_printf("No such thread\n");
        } else {
            nk_print_regs((struct nk_regs *) t->rsp);
        }
        return 0;
    }

    nk_interrupt_like_trampoline(nk_print_regs);

    return 0;
}

static struct shell_cmd_impl regs_impl = {
    .cmd      = "regs",
    .help_str = "regs [t]",
    .handler  = handle_regs,
};
nk_register_shell_cmd(regs_impl);

static int
handle_in (char * buf, void * priv)
{
    uint64_t data, addr;
    char bwdq;

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

    nk_vc_printf("invalid in command\n");

    return 0;
}

static struct shell_cmd_impl in_impl = {
    .cmd      = "in",
    .help_str = "in [bwd] addr",
    .handler  = handle_in,
};
nk_register_shell_cmd(in_impl);

static int
handle_out (char * buf, void * priv)
{
    uint64_t data, addr;
    char bwdq;

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

    nk_vc_printf("invalid out command\n");

    return 0;
}

static struct shell_cmd_impl out_impl = {
    .cmd      = "out",
    .help_str = "out [bwd] addr data",
    .handler  = handle_out,
};
nk_register_shell_cmd(out_impl);



