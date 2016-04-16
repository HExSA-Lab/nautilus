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
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/multiboot2.h>
#include <nautilus/mb_utils.h>
#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/paging.h>
#include <nautilus/irq.h>
#include <nautilus/mm.h>
#include <arch/hrt/hrt.h>

#define PML4_STRIDE (0x1ULL << (12+9+9+9))

#ifndef NAUT_CONFIG_DEBUG_HRT
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define HRT_DEBUG(fmt, args...) DEBUG_PRINT("HRT: " fmt, ##args)
#define HRT_PRINT(fmt, args...) printk("HRT: " fmt, ##args)
#define HRT_WARN(fmt, args...)  WARN_PRINT("HRT: " fmt, ##args)

static volatile unsigned long long *sync_proto = 0;
static volatile int done_sync = 0;


static void 
dump_pml4 (void *pt)
{
  uint64_t *pte = (uint64_t *)pt;
  uint64_t i;

  for (i=0;i<512;i++,pte++) {
    if ((i<0x10) || (i>=0x78 && i<0x88) || (i>=0xf8 && i<=0x108) ) {
      printk("0x%llx:  %p to 0x%llx entry: 0x%llx\n",i,(void*)(i*PML4_STRIDE),*pte & ~0xfff, *pte);
    }
  }

}


static void* 
cr3_to_gva (uint64_t cr3)
{
  uint64_t gva_offset = nautilus_info.sys.mb_info->hrt_info->gva_offset;
  void * gva_of_pt = (void*)(gva_offset + (cr3 & ~0xfffULL));

  return gva_of_pt;
}


static inline uint64_t 
get_my_cr3 (void)
{
  uint64_t cr3;
  __asm__ __volatile__ (" movq %%cr3, %%rax; "
            " movq %%rax, %0; "
            : "=m"(cr3)
            :
            : "%rax"
            );
  return cr3;
}


static inline void 
flush_tlb (void)
{
  __asm__ __volatile__ (" movq %%cr3, %%rax;"
            " movq %%rax, %%cr3;"
            " movq %%cr4, %%rax;"
            " movq %%rax, %%cr4;"
            :
            :
            : "%rax");
}


static void 
merge_with_ros_cr3 (uint64_t ros_cr3)
{
  void *hrt_pml4_gva=cr3_to_gva(get_my_cr3());
  void *ros_pml4_gva=cr3_to_gva(ros_cr3);

  memcpy(hrt_pml4_gva,ros_pml4_gva,2048);

  flush_tlb();
}


static void 
unmerge_from_ros (void)
{
  void *hrt_pml4_gva = cr3_to_gva(get_my_cr3());
  memset(hrt_pml4_gva,0,2048);
  flush_tlb();
}


int 
nautilus_hrt_upcall_handler (excp_entry_t * excp, excp_vec_t vec)
{
    uint64_t *page = (uint64_t *) ((uint64_t) nautilus_info.sys.mb_info->hrt_info->comm_page_gpa + HRT_HIHALF_OFFSET);  // fixup if using gvaoffset
    uint64_t a1, a2;
    unsigned long long sync_count=0;

    HRT_DEBUG("HRT upcall (page=%p)\n", page);

    a1 = page[0];
    a2 = page[1];

    HRT_DEBUG("HRT comm page at %p, a1=0x%llx a2=0x%llx\n",page,a1,a2);

    switch (a1) {
    case 0x0:
        HRT_DEBUG("HRT null upcall\n");
        break;
    case 0x20:
        HRT_DEBUG("HRT invoke function %p\n", (void*)a2);
        // fake, our function is "print this string"
        HRT_DEBUG("First word of string is %llx\n",*((uint64_t*)a2));
        HRT_DEBUG("The ROS sent us the string %s\n",a2);
        HRT_DEBUG("HRT indicating function completion\n");
        hvm_hcall(0x2f,0,0,0,0,0,0,0);
        break;
    case 0x21:
        HRT_DEBUG("HRT invoke paralllel function %p\n", (void*)a2);
        HRT_DEBUG("HRT indicating parallel function completion\n");
        hvm_hcall(0x2f,0,0,0,0,0,0,0);
        break;

    case 0x28:
        HRT_DEBUG("HRT setup for synchronous invocation %p\n", (void*)a2);
        HRT_DEBUG("HRT indicating synchronous invocation setup completion\n");
        sync_proto = (unsigned long long *)a2;
        done_sync=0;
        hvm_hcall(0x2f,0,0,0,0,0,0,0);
        sync_count=1;
        do {
            while (!done_sync && sync_proto[0]!=sync_count) { }
            if (!done_sync) {
                HRT_DEBUG("HRT synchronous invoke of %p\n",(void*) (sync_proto[2]));
                sync_count++;
                sync_proto[1]++;
            }
        } while (!done_sync);

        HRT_DEBUG("HRT leaving synchronous mode\n");

        break;

    case 0x29:
        HRT_DEBUG("HRT teardown for synchronous invocation %p\n", (void*)a2);
        HRT_DEBUG("HRT indicating synchronous invocation teardown completion\n");
        done_sync=1;
        hvm_hcall(0x2f,0,0,0,0,0,0,0);
        break;

    case 0x30:
        HRT_DEBUG("HRT merge address space with %p\n", (void*)a2);
        merge_with_ros_cr3(a2);
        HRT_DEBUG("HRT indicating merge completion\n");
        hvm_hcall(0x3f,0,0,0,0,0,0,0);
        break;

    case 0x31:
        HRT_DEBUG("HRT unmerge address space\n");
        unmerge_from_ros();
        HRT_DEBUG("HRT indicating unmerge completion\n");
        hvm_hcall(0x3f,0,0,0,0,0,0,0);
        break;
    default:
        ERROR_PRINT("Unknown HVM request %p\n",(void*)a1);
        break;
    }

    return 0;
}


int 
hrt_init_cpus (struct sys_info * sys)
{
    int i;
    struct multiboot_tag_hrt * hrt = sys->mb_info->hrt_info;
    unsigned core2apic_offset;

    if (!hrt) {
        panic("No HRT information found!\n");
    }

    sys->bsp_id   = 0;
    sys->num_cpus = hrt->total_num_apics - hrt->first_hrt_apic_id;

    core2apic_offset = hrt->first_hrt_apic_id;

    if (sys->num_cpus > NAUT_CONFIG_MAX_CPUS) {
        panic("Nautilus kernel currently compiled for a maximum CPU count of %u, attempting to use %u\n", NAUT_CONFIG_MAX_CPUS, sys->num_cpus);
    }

    HRT_PRINT("HRT detected %u CPUs\n", sys->num_cpus);

    for (i = 0; i < sys->num_cpus; i++) {
        struct cpu * new_cpu = mm_boot_alloc(sizeof(struct cpu));

        if (!new_cpu) {
            ERROR_PRINT("Could not allocate CPU struct\n");
            return -1;
        }
        memset(new_cpu, 0, sizeof(struct cpu));

        if (i == sys->bsp_id) {
            new_cpu->is_bsp = 1;
        }

        new_cpu->id         = i;
        new_cpu->lapic_id   = i + core2apic_offset;
        new_cpu->enabled    = 1;
        new_cpu->cpu_sig    = 0;
        new_cpu->feat_flags = 0;
        new_cpu->system     = sys;
        new_cpu->cpu_khz    = 0;

        spinlock_init(&(new_cpu->lock));

        sys->cpus[i] = new_cpu;
        
    }

    return 0;
}


int
hrt_init_ioapics (struct sys_info * sys)
{
    struct multiboot_tag_hrt * hrt = sys->mb_info->hrt_info;

    if (!hrt) {
        panic("No HRT information found!\n");
    }

    if (hrt->have_hrt_ioapic) {

        sys->num_ioapics = 1;

        struct ioapic * ioa = mm_boot_alloc(sizeof(struct ioapic));
        if (!ioa) {
            ERROR_PRINT("Could not allocate IOAPIC\n");
            return -1;
        }
        memset(ioa, 0, sizeof(struct ioapic));

        ioa->id      = 0;
        ioa->version = 0;
        ioa->usable  = 1;

        // KCH TODO: maybe the bases should be communicated
        ioa->base            = 0xfec00000;
        ioa->first_hrt_entry = hrt->first_hrt_ioapic_entry;

        sys->ioapics[0] = ioa;

            
    } else {
        sys->num_ioapics = 0;
    }

    return 0;
}


int
__early_init_hrt (struct naut_info * naut)
{
    if (hrt_init_cpus(&(naut->sys)) < 0) {
        panic("Could not initialize HRT cores\n");
    }

    if (hrt_init_ioapics(&(naut->sys)) < 0) {
        panic("Could not initialize HRT IOAPICS\n");
    }

    return 0;
}

void 
hrt_putchar (char c)
{ 
    outb(c, 0xc0c0);
}


void 
hrt_print (const char * s) 
{
    while (*s) {
        hrt_putchar(*s);
        s++;
    }
}

void 
hrt_puts (const char *s)
{
   hrt_print(s); 
   hrt_putchar('\n');
}

int
hvm_hrt_init (void)
{
    HRT_DEBUG("Pinging the VMM with HRT init status\n");
    hvm_hcall(0,0,0,0,0,0,0,0);

    HRT_PRINT("Registering HRT upcall handler\n");
    if (register_int_handler(nautilus_info.sys.mb_info->hrt_info->hrt_int_vec,
                nautilus_hrt_upcall_handler, 0)) {
        ERROR_PRINT("Cannot install HRT upcall handler\n");
    }
    return 0;
}
