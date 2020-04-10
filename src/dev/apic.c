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
#include <nautilus/cpu.h>
#include <nautilus/cpuid.h>
#include <nautilus/msr.h>
#include <nautilus/irq.h>
#include <nautilus/paging.h>
#include <nautilus/nautilus.h>
#include <nautilus/percpu.h>
#include <nautilus/intrinsics.h>
#include <nautilus/mm.h>
#include <nautilus/shell.h>
#include <nautilus/timer.h>
#include <nautilus/dev.h>
#include <dev/apic.h>
#include <dev/i8254.h>
#include <lib/bitops.h>

#include <dev/gpio.h>

#ifndef NAUT_CONFIG_DEBUG_APIC
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define APIC_DEBUG(fmt, args...) DEBUG_PRINT("APIC: " fmt, ##args)
#define APIC_PRINT(fmt, args...) INFO_PRINT("APIC: " fmt, ##args)
#define APIC_WARN(fmt, args...)  WARN_PRINT("APIC: " fmt, ##args)
#define APIC_ERROR(fmt, args...) ERROR_PRINT("APIC: " fmt, ##args)

static const char * apic_err_codes[8] = {
    "[Send Checksum Error]",
    "[Receive Checksum Error]",
    "[Send Accept Error]",
    "[Receive Accept Error]",
    "[Redirectable IPI]",
    "[Send Illegal Vector]",
    "[Receive Illegal Vector]",
    "[Illegal Register Address]"
};

static const char * apic_modes[4] = {
    "Invalid",
    "Disabled",
    "XAPIC",
    "X2APIC",
};

static apic_mode_t get_max_mode()
{
    cpuid_ret_t c;
    cpuid(0x1, &c);
    if ((c.c >> 21)&0x1) { 	
	APIC_DEBUG("APIC with X2APIC support\n");
	return APIC_X2APIC;
    }
    return APIC_XAPIC;  // it must at least do this...
}

static apic_mode_t get_mode()
{
    uint64_t val;
    uint64_t EN, EXTD;

    val= msr_read(APIC_BASE_MSR);
    EN = (val>>11)&0x1;
    EXTD = (val>>10)&0x1;

    if (!EN && !EXTD) { 
	return APIC_DISABLED;
    }
    
    if (EN && !EXTD) {
	return APIC_XAPIC;
    }
    
    if (EN && EXTD) { 
	return APIC_X2APIC;
    }

    return APIC_INVALID;
}


static int set_mode(struct apic_dev *apic, apic_mode_t mode)
{
    uint64_t val;
    apic_mode_t cur = get_mode();

    if (mode==cur) { 
	apic->mode = mode;
	return 0;
    }

    if (mode==APIC_INVALID) { 
	return -1;
    }

    // first go back to disabled
    val = msr_read(APIC_BASE_MSR);
    val &= ~(0x3 << 10);
    msr_write(APIC_BASE_MSR,val);
    // now go to the relevant mode progressing "upwards"
    if (mode!=APIC_DISABLED) { 
	// switch to XAPIC first
	val |= 0x2<<10;
	msr_write(APIC_BASE_MSR,val);
	if (mode==APIC_X2APIC) { 
	    // now to X2APIC if requested
	    val |= 0x3 <<10;
	    msr_write(APIC_BASE_MSR,val);
	}
    }
    apic->mode=mode;
    return 0;
}




static int
spur_int_handler (excp_entry_t * excp, excp_vec_t v, void *state)
{
    APIC_WARN("APIC (ID=0x%x) Received Spurious Interrupt on core %u\n",
        per_cpu_get(apic)->id,
        my_cpu_id());

    struct apic_dev * a = per_cpu_get(apic);
    a->spur_int_cnt++;

    /* we don't need to EOI here */
    return 0;
}

static int
null_kick (excp_entry_t * excp, excp_vec_t v, void *state)
{
    struct apic_dev * apic = (struct apic_dev*)per_cpu_get(apic);
    
    // this communicates that we are in a kick to the scheduler
    // the scheduler will then set this to zero as a side-effect
    // of calling apic_update_oneshot_timer
    apic->in_kick_interrupt = 1;

    IRQ_HANDLER_END();
    return 0;
}

static int
error_int_handler (excp_entry_t * excp, excp_vec_t v, void *state)
{
    struct apic_dev * apic = per_cpu_get(apic);
    char * s = "[Unknown Error]";
    uint8_t i = 0;
    uint32_t err = 0;

    apic_write(apic, APIC_REG_ESR, 0);
    err = apic_read(apic, APIC_REG_ESR);
    apic_do_eoi();

    apic->err_int_cnt++;

    err &= 0xff;

    APIC_WARN("Error interrupt recieved from local APIC (ID=0x%x) on Core %u (error=0x%x):\n", 
            per_cpu_get(apic)->id, my_cpu_id(), err);

    while (err) {

        if (err & 0x1) {
            s = (char*)apic_err_codes[i];
            APIC_WARN("\t%s\n", s);
        }

        ++i;
        err >>= 1;
    }

    return 0;
}


static int
dummy_int_handler (excp_entry_t * excp, excp_vec_t v, void *state)
{
    panic("Received an interrupt from an Extended LVT vector  on LAPIC (0x%x) on core %u (Should be masked)\n",
        per_cpu_get(apic)->id,
        my_cpu_id());

    return 0;
}


static int
pc_int_handler (excp_entry_t * excp, excp_vec_t v, void *state)
{
    panic("Received a performance counter interrupt from the LAPIC (0x%x) on core %u (Should be masked)\n",
        per_cpu_get(apic)->id,
        my_cpu_id());

    return 0;
}


static int
thermal_int_handler (excp_entry_t * excp, excp_vec_t v, void *state)
{
    panic("Received a thermal interrupt from the LAPIC (0x%x) on core %u (Should be masked)\n",
        per_cpu_get(apic)->id,
        my_cpu_id());

    return 0;
}


static uint8_t
check_apic_avail (void)
{
    cpuid_ret_t cp;
    struct cpuid_feature_flags * flags;

    cpuid(CPUID_FEATURE_INFO, &cp);
    flags = (struct cpuid_feature_flags *)&cp.c;

    return flags->edx.apic;
}


static uint8_t
apic_is_bsp (struct apic_dev * apic)
{
    uint64_t data;
    data = msr_read(APIC_BASE_MSR);
    return !!APIC_IS_BSP(data);
}


static void
apic_sw_enable (struct apic_dev * apic)
{
    uint32_t val;
    uint8_t flags = irq_disable_save();
    val = apic_read(apic, APIC_REG_SPIV);
    apic_write(apic, APIC_REG_SPIV, val | APIC_SPIV_SW_ENABLE);
    irq_enable_restore(flags);
}


static void
apic_sw_disable (struct apic_dev * apic)
{
    uint32_t val;
    uint8_t flags = irq_disable_save();
    val = apic_read(apic, APIC_REG_SPIV);
    apic_write(apic, APIC_REG_SPIV, val & ~APIC_SPIV_SW_ENABLE);
    irq_enable_restore(flags);
}


static void
apic_assign_spiv (struct apic_dev * apic, uint8_t spiv_vec)
{
    apic_write(apic, 
            APIC_REG_SPIV,
            apic_read(apic, APIC_REG_SPIV) | spiv_vec);
}


static inline void
apic_global_enable (void)
{
    msr_write(APIC_BASE_MSR, msr_read(APIC_BASE_MSR) | APIC_GLOBAL_ENABLE);
}

// This request only makes sense if we are in xapic mode
static ulong_t 
apic_get_base_addr (void) 
{
    uint64_t data;
    data = msr_read(APIC_BASE_MSR);

    // we're assuming PAE is on
    return (addr_t)(data & APIC_BASE_ADDR_MASK);
}


// This request only makes sense if we are in xapic mode
static void
apic_set_base_addr (struct apic_dev * apic, addr_t addr)
{
    uint64_t data;
    data = msr_read(APIC_BASE_MSR);
    msr_write(APIC_BASE_MSR, (addr & APIC_BASE_ADDR_MASK) | (data & 0xfff));
}


void 
apic_do_eoi (void)
{
    struct apic_dev * apic = (struct apic_dev*)per_cpu_get(apic);
    ASSERT(apic);
    apic_write(apic, APIC_REG_EOR, 0);
}


uint32_t
apic_get_id (struct apic_dev * apic)
{
    if (apic->mode==APIC_X2APIC) {
	return apic_read(apic, APIC_REG_ID);
    } else {
	return (apic_read(apic, APIC_REG_ID) >> APIC_ID_SHIFT) & 0xff;
    }
}


static inline uint8_t 
apic_get_version (struct apic_dev * apic)
{
    return APIC_VERSION(apic_read(apic, APIC_REG_LVR));
}


uint32_t 
apic_wait_for_send(struct apic_dev * apic)
{
    uint32_t res;
    int n = 0;

    do {
	if (!(res = apic_read(apic, APIC_REG_ICR) & ICR_SEND_PENDING)) {
	    break;
        }
        udelay(100);
    } while (n++ < 1000);

    return res;
}


int 
apic_get_maxlvt (struct apic_dev * apic)
{
    uint_t v;

    v = apic_read(apic, APIC_REG_LVR);
    return ((v >> 16) & 0xffu);
}


int
apic_read_timer (struct apic_dev * apic)
{
    return apic_read(apic, APIC_REG_TMCCT);
}


void
apic_self_ipi (struct apic_dev * apic, uint_t vector)
{
    if (apic->mode==APIC_X2APIC) {
	uint8_t flags = irq_disable_save();
	apic_write(apic, APIC_REG_SELF_IPI, vector);
	irq_enable_restore(flags);
    } else {
	apic_ipi(apic,my_cpu_id(),vector);
    }
}

void 
apic_send_iipi (struct apic_dev * apic, uint32_t remote_id) 
{
    uint8_t flags = irq_disable_save();
    apic_write_icr(apic, 
		   remote_id,
		   ICR_TRIG_MODE_LEVEL| ICR_LEVEL_ASSERT | ICR_DEL_MODE_INIT);
    irq_enable_restore(flags);
}


void
apic_deinit_iipi (struct apic_dev * apic, uint32_t remote_id)
{
    uint8_t flags = irq_disable_save();
    apic_write_icr(apic,
		   remote_id,
		   ICR_TRIG_MODE_LEVEL| ICR_DEL_MODE_INIT);
    irq_enable_restore(flags);
}


void
apic_send_sipi (struct apic_dev * apic, uint32_t remote_id, uint8_t target)
{
    uint8_t flags = irq_disable_save();
    apic_write_icr(apic,
		   remote_id,
		   ICR_DEL_MODE_STARTUP | target);
    irq_enable_restore(flags);
}


void
apic_bcast_iipi (struct apic_dev * apic) 
{
    uint8_t flags = irq_disable_save();
    apic_write_icr(apic,
		   0,
		   APIC_IPI_OTHERS | ICR_LEVEL_ASSERT | ICR_TRIG_MODE_LEVEL | ICR_DEL_MODE_INIT);
    irq_enable_restore(flags);
}


void
apic_bcast_deinit_iipi (struct apic_dev * apic)
{
    uint8_t flags = irq_disable_save();
    apic_write_icr(apic,
		   0,
		   APIC_IPI_OTHERS | ICR_TRIG_MODE_LEVEL | ICR_DEL_MODE_INIT);
    irq_enable_restore(flags);
}


void
apic_bcast_sipi (struct apic_dev * apic, uint8_t target)
{
    uint8_t flags = irq_disable_save();
    apic_write_icr(apic,
		   0,
		   APIC_IPI_OTHERS | ICR_DEL_MODE_STARTUP | target);
    irq_enable_restore(flags);
}

static void calibrate_apic_timer(struct apic_dev *apic);
static int apic_timer_handler(excp_entry_t * excp, excp_vec_t vec, void *state);


static void
apic_timer_setup (struct apic_dev * apic, uint32_t quantum_ms)
{
    uint32_t busfreq;
    uint32_t tmp;
    cpuid_ret_t ret;
    int x2apic, tscdeadline, arat; 

    APIC_DEBUG("Setting up Local APIC timer for APIC 0x%x\n", apic->id);

    cpuid(0x1, &ret);
  
    x2apic = (ret.c >> 21) & 0x1;
    tscdeadline = (ret.c >> 24) & 0x1;

    cpuid(0x6, &ret);
    arat = (ret.a >> 2) & 0x1;

    APIC_DEBUG("APIC timer has:  x2apic=%d tscdeadline=%d arat=%d\n",
	       x2apic, tscdeadline, arat);

    // Note that no state is used here since APICs are per-CPU
    if (register_int_handler(APIC_TIMER_INT_VEC,
			     apic_timer_handler,
			     NULL) != 0) {
        panic("Could not register APIC timer handler\n");
    }

    calibrate_apic_timer(apic);

    apic_set_oneshot_timer(apic,apic_realtime_to_ticks(apic,quantum_ms*1000000ULL));
}


/**
 * Converts an entry in a local APIC's Local Vector Table to a
 * human-readable string.
 * (NOTE: taken from Kitten)
 */
static char *
lvt_stringify (uint32_t entry, char *buf)
{
	uint32_t delivery_mode = entry & 0x700;

	if (delivery_mode == APIC_DEL_MODE_FIXED) {
		sprintf(buf, "FIXED -> IDT VECTOR %u",
			entry & APIC_LVT_VEC_MASK
		);
	} else if (delivery_mode == APIC_DEL_MODE_NMI) {
		sprintf(buf, "NMI   -> IDT VECTOR 2"); 
	} else if (delivery_mode == APIC_DEL_MODE_EXTINT) {
		sprintf(buf, "ExtINT, hooked to old 8259A PIC");
	} else {
		sprintf(buf, "UNKNOWN");
	}

	if (entry & APIC_LVT_DISABLED)
		strcat(buf, ", MASKED");

	return buf;
}


static inline uint8_t 
amd_has_ext_lvt (struct apic_dev * apic)
{
    uint32_t ver = apic_read(apic, APIC_REG_LVR);

    if (APIC_LVR_HAS_EXT_LVT(ver)) {
        return 1;
    }

    return 0;
}


static void
amd_setup_ext_lvt (struct apic_dev * apic)
{
    if (APIC_EXFR_GET_LVT(apic_read(apic, APIC_REG_EXFR))) {

        int i;
        for (i = 0; i < APIC_EXFR_GET_LVT(apic_read(apic, APIC_REG_EXFR)); i++) {

            /* we assign a bogus vector to extended LVT entries */
            apic_write(apic, APIC_REG_EXTLVT(i), 0 | 
                    APIC_LVT_DISABLED | 
                    APIC_EXT_LVT_DUMMY_VEC);
        }
    }

}


static void
apic_dump (struct apic_dev * apic)
{
	char buf[128];
	apic_mode_t mode = get_mode();


	APIC_DEBUG("DUMP (LOGICAL CPU #%u):\n", my_cpu_id());

	APIC_DEBUG(
		"  ID:  0x%08x (id=%d) (mode=%s)\n",
		apic_read(apic, APIC_REG_ID),
		mode!=APIC_X2APIC ? APIC_GET_ID(apic_read(apic, APIC_REG_ID)) : apic_read(apic,APIC_REG_ID),
		apic_modes[mode]
	);

	APIC_DEBUG(
		   "  VER: 0x%08x (version=0x%x, max_lvt=%d)\n",
		   apic_read(apic, APIC_REG_LVR),
		   APIC_LVR_VER(apic_read(apic, APIC_REG_LVR)),
		   APIC_LVR_MAX(apic_read(apic, APIC_REG_LVR))
		   );
	
	if (mode!=APIC_X2APIC) { 
	    APIC_DEBUG(
		       "  BASE ADDR: %p\n",
		       apic->base_addr
		       );
	}

	if (nk_is_amd() && amd_has_ext_lvt(apic)) {
	    APIC_DEBUG(
                "  EXT (AMD-only): 0x%08x (Ext LVT Count=%u, Ext APIC ID=%u, Specific EOI=%u, Int Enable Reg=%u)\n",
                apic_read(apic, APIC_REG_EXFR),
                APIC_EXFR_GET_LVT(apic_read(apic, APIC_REG_EXFR)),
                APIC_EXFR_GET_XAIDC(apic_read(apic, APIC_REG_EXFR)),
                APIC_EXFR_GET_SNIC(apic_read(apic, APIC_REG_EXFR)),
                APIC_EXFR_GET_INC(apic_read(apic, APIC_REG_EXFR))
                );

        int i;
        for (i = 0; i < APIC_EXFR_GET_LVT(apic_read(apic, APIC_REG_EXFR)); i++) {
            APIC_DEBUG(
                "      EXT-LVT[%u]: 0x%08x (%s)\n", 
                i,
                apic_read(apic, APIC_REG_EXTLVT(i)),
                lvt_stringify(apic_read(apic, APIC_REG_EXTLVT(i)), buf)
            );
        }
    }
        
    APIC_DEBUG(
		"  ESR: 0x%08x (Error Status Reg, non-zero is bad)\n",
		apic_read(apic, APIC_REG_ESR)
	);
    APIC_DEBUG(
		"  SVR: 0x%08x (Spurious vector=%d, %s, %s)\n",
		apic_read(apic, APIC_REG_SPIV),
		apic_read(apic, APIC_REG_SPIV) & APIC_SPIV_VEC_MASK,
		(apic_read(apic, APIC_REG_SPIV) & APIC_SPIV_SW_ENABLE)
			? "APIC IS ENABLED"
			: "APIC IS DISABLED",
        (apic_read(apic, APIC_REG_SPIV) & APIC_SPIV_CORE_FOCUS)
            ? "Core Focusing Disabled"
            : "Core Focusing Enabled"
	);

	/*
 	 * Local Vector Table
 	 */
	APIC_DEBUG("  Local Vector Table Entries:\n");
    char * timer_mode;
    if (apic_read(apic, APIC_REG_LVTT) & APIC_TIMER_PERIODIC) {
        timer_mode = "Periodic";
    } else if (apic_read(apic, APIC_REG_LVTT) & APIC_TIMER_TSCDLINE) {
        timer_mode = "TSC-Deadline";
    } else {
        timer_mode = "One-shot";
    }

	APIC_DEBUG("      LVT[0] Timer:     0x%08x (mode=%s, %s)\n",
		apic_read(apic, APIC_REG_LVTT),
        timer_mode,
		lvt_stringify(apic_read(apic, APIC_REG_LVTT), buf)
	);
	APIC_DEBUG("      LVT[1] Thermal:   0x%08x (%s)\n",
		apic_read(apic, APIC_REG_LVTTHMR),
		lvt_stringify(apic_read(apic, APIC_REG_LVTTHMR), buf)
	);
	APIC_DEBUG("      LVT[2] Perf Cnt:  0x%08x (%s)\n",
		apic_read(apic, APIC_REG_LVTPC),
		lvt_stringify(apic_read(apic, APIC_REG_LVTPC), buf)
	);
	APIC_DEBUG("      LVT[3] LINT0 Pin: 0x%08x (%s)\n",
		apic_read(apic, APIC_REG_LVT0),
		lvt_stringify(apic_read(apic, APIC_REG_LVT0), buf)
	);
	APIC_DEBUG("      LVT[4] LINT1 Pin: 0x%08x (%s)\n",
		apic_read(apic, APIC_REG_LVT1),
		lvt_stringify(apic_read(apic, APIC_REG_LVT1), buf)
	);
	APIC_DEBUG("      LVT[5] Error:     0x%08x (%s)\n",
		apic_read(apic, APIC_REG_LVTERR),
		lvt_stringify(apic_read(apic, APIC_REG_LVTERR), buf)
	);

	/*
 	 * APIC timer configuration registers
 	 */
	APIC_DEBUG("  Local APIC Timer:\n");
	APIC_DEBUG("      DCR (Divide Config Reg): 0x%08x\n",
		apic_read(apic, APIC_REG_TMDCR)
	);
	APIC_DEBUG("      ICT (Initial Count Reg): 0x%08x\n",
		apic_read(apic, APIC_REG_TMICT)
	);

	APIC_DEBUG("      CCT (Current Count Reg): 0x%08x\n",
		apic_read(apic, APIC_REG_TMCCT)
	);

	/*
 	 * Logical APIC addressing mode registers
 	 */
	APIC_DEBUG("  Logical Addressing Mode Information:\n");
	if (mode!=APIC_X2APIC) {
	    APIC_DEBUG("      LDR (Logical Dest Reg):  0x%08x (id=%d)\n",
		       apic_read(apic, APIC_REG_LDR),
		       GET_APIC_LOGICAL_ID(apic_read(apic, APIC_REG_LDR))
		       );
	} else {
	    APIC_DEBUG("      LDR (Logical Dest Reg):  0x%08x (cluster=%d, id=%d)\n",
		       apic_read(apic, APIC_REG_LDR),
		       APIC_LDR_X2APIC_CLUSTER(apic_read(apic, APIC_REG_LDR)),
		       APIC_LDR_X2APIC_LOGID(apic_read(apic, APIC_REG_LDR))
		       );
	}
	if (mode!=APIC_X2APIC) {
	    APIC_DEBUG("      DFR (Dest Format Reg):   0x%08x (%s)\n",
		       apic_read(apic, APIC_REG_DFR),
		       (apic_read(apic, APIC_REG_DFR) == APIC_DFR_FLAT) ? "FLAT" : "CLUSTER"
		       );
	}

	/*
 	 * Task/processor/arbitration priority registers
 	 */
	APIC_DEBUG("  Task/Processor/Arbitration Priorities:\n");
	APIC_DEBUG("      TPR (Task Priority Reg):        0x%08x\n",
		apic_read(apic, APIC_REG_TPR)
	);
	APIC_DEBUG("      PPR (Processor Priority Reg):   0x%08x\n",
#ifndef NAUT_CONFIG_GEM5    // unimplemented in GEM5 - WTF?
		   apic_read(apic, APIC_REG_PPR)
#else
		   0
#endif
	);
	if (mode!=APIC_X2APIC) { 
	    // reserved in X2APIC
	    APIC_DEBUG("      APR (Arbitration Priority Reg): 0x%08x\n",
#ifndef NAUT_CONFIG_GEM5    // unimplemented in GEM5 - WTF?
		       apic_read(apic, APIC_REG_APR)
#else
		       0
#endif
		       );
	}


    /* 
     * ISR/IRR
     */
    APIC_DEBUG("  IRR/ISR:\n");
    APIC_DEBUG("      IRR (Interrupt Request Reg):       0x%08x\n",
            apic_read(apic, APIC_GET_IRR(0)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_IRR(1)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_IRR(2)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_IRR(3)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_IRR(4)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_IRR(5)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_IRR(6)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_IRR(7)));

    APIC_DEBUG("      ISR (In-Service Reg):              0x%08x\n",
            apic_read(apic, APIC_GET_ISR(0)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_ISR(1)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_ISR(2)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_ISR(3)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_ISR(4)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_ISR(5)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_ISR(6)));
    APIC_DEBUG("                                         0x%08x\n",
            apic_read(apic, APIC_GET_ISR(7)));

}

static struct nk_dev_int ops = {
    .open = 0,
    .close = 0,
};

void
apic_init (struct cpu * core)
{
    struct apic_dev * apic = NULL;
    ulong_t base_addr;
    uint32_t val;
    apic_mode_t curmode,maxmode;

    apic = (struct apic_dev*)malloc(sizeof(struct apic_dev));
    if (!apic) {
        panic("Could not allocate apic struct\n");
    }
    memset(apic, 0, sizeof(struct apic_dev));
    core->apic = apic;

    if (!check_apic_avail()) {
        panic("No APIC found on core %u, dying\n", core->id);
    } 

    curmode = get_mode();
    maxmode = get_max_mode();
    APIC_DEBUG("APIC's initial mode is %s\n", apic_modes[curmode]);

#ifdef NAUT_CONFIG_APIC_FORCE_XAPIC_MODE
    APIC_DEBUG("Setting APIC mode to XAPIC (Forced - Maximum mode is %s)\n",
	       apic_modes[maxmode]);
    set_mode(apic,APIC_XAPIC);
#else 
    APIC_DEBUG("Setting APIC mode to maximum available mode (%s)\n",
	       apic_modes[maxmode]);
    set_mode(apic,maxmode);
#endif
   
    /* In response to AMD erratum #663 
     * the damn thing may give us lint interrupts
     * even when we have them masked
     */
    if (nk_is_amd()  && cpuid_get_family() == 0x15) {
        APIC_DEBUG("Writing Bridge Ctrl MSR for AMD Errata #663\n");
        msr_write(AMD_MSR_NBRIDGE_CTL, 
                msr_read(AMD_MSR_NBRIDGE_CTL) | 
                (1ULL<<23) | 
                (1ULL<<54));
    }

    if (apic->mode!=APIC_X2APIC) {
	base_addr       = apic_get_base_addr();
	
	/* idempotent when not compiled as HRT */
	apic->base_addr = pa_to_va(base_addr);
	
#ifndef NAUT_CONFIG_HVM_HRT
	if (core->is_bsp) {
	    /* map in the lapic as uncacheable */
	    if (nk_map_page_nocache(apic->base_addr, PTE_PRESENT_BIT|PTE_WRITABLE_BIT, PS_4K) == -1) {
		panic("Could not map APIC\n");
	    }
	}
#endif
    }

    apic->version   = apic_get_version(apic);
    apic->id        = apic_get_id(apic);

#ifndef NAUT_CONFIG_XEON_PHI
    if (apic->version < 0x10 || apic->version > 0x15) {
        panic("Unsupported APIC version (0x%1x)\n", (unsigned)apic->version);
    }
#endif

    if (apic->mode==APIC_X2APIC) {
	// LDR is not writeable in X2APIC mode
	// we just have to go with what it is
	// see note in apic.h about how to derive
	// the logical id from the physical id
        val = apic_read(apic,APIC_REG_LDR);
        APIC_DEBUG("X2APIC LDR=0x%x (cluster 0x%x, logical id 0x%x)\n", 
		   val, APIC_LDR_X2APIC_CLUSTER(val), APIC_LDR_X2APIC_LOGID(val));
	
    } else {
        val = apic_read(apic, APIC_REG_LDR) & ~APIC_LDR_MASK;
	// flat group 1 is for watchdog NMIs.
	// At least one bit must be set in a group	
	val |= SET_APIC_LOGICAL_ID(1);   
	apic_write(apic, APIC_REG_LDR, val);
	apic_write(apic, APIC_REG_DFR, APIC_DFR_FLAT);
    }

    apic_write(apic, APIC_REG_TPR, apic_read(apic, APIC_REG_TPR) & 0xffffff00);                       // accept all interrupts
    apic_write(apic, APIC_REG_LVTT,    APIC_DEL_MODE_FIXED | APIC_LVT_DISABLED);                      // disable timer interrupts intially
    apic_write(apic, APIC_REG_LVTPC,   APIC_DEL_MODE_FIXED | APIC_LVT_DISABLED | APIC_PC_INT_VEC);    // disable perf cntr interrupts
    apic_write(apic, APIC_REG_LVTTHMR, APIC_DEL_MODE_FIXED | APIC_LVT_DISABLED | APIC_THRML_INT_VEC); // disable thermal interrupts

    /* do we have AMD extended LVT entries to deal with */
    if (nk_is_amd() && amd_has_ext_lvt(apic)) {
        amd_setup_ext_lvt(apic);
    }
            

    /* mask 8259a interrupts */
    apic_write(apic, APIC_REG_LVT0, APIC_DEL_MODE_EXTINT  | APIC_LVT_DISABLED);

    /* only BSP takes NMI interrupts */
    apic_write(apic, APIC_REG_LVT1, 
	       APIC_DEL_MODE_NMI | (core->is_bsp ? 0 : APIC_LVT_DISABLED));

    apic_write(apic, APIC_REG_LVTERR, APIC_DEL_MODE_FIXED | APIC_ERROR_INT_VEC); // allow error interrupts

    // clear the ESR
    apic_write(apic, APIC_REG_ESR, 0u);

    apic_global_enable();

    // assign interrupt handlers
    // all cores share the same IDT/etc, so only the BSP needs to do this
    if (core->is_bsp) {

	// Note that no state is used here since APICs are per-CPU

        if (register_int_handler(APIC_NULL_KICK_VEC, null_kick, NULL) != 0) {
            panic("Could not register null kick interrupt handler\n");
        }

        if (register_int_handler(APIC_SPUR_INT_VEC, spur_int_handler, NULL) != 0) {
            panic("Could not register spurious interrupt handler\n");
        }

        if (register_int_handler(APIC_ERROR_INT_VEC, error_int_handler, NULL) != 0) {
            panic("Could not register spurious interrupt handler\n");
            return;
        }

        /* we shouldn't ever get these, but just in case */
        if (register_int_handler(APIC_PC_INT_VEC, pc_int_handler, NULL) != 0) {
            panic("Could not register perf counter interrupt handler\n");
            return;
        }

        if (register_int_handler(APIC_THRML_INT_VEC, thermal_int_handler, NULL) != 0) {
            panic("Could not register thermal interrupt handler\n");
            return;
        }

        if (register_int_handler(APIC_EXT_LVT_DUMMY_VEC, dummy_int_handler, NULL) != 0) {
            panic("Could not register dummy ext lvt handler\n");
            return;
        }
    }

    apic_assign_spiv(apic, APIC_SPUR_INT_VEC);

    /*

      The presence of the APIC Extended Space area is indicated by bit 31 of the APIC Version Register (at offset 30h in APIC space).
      The presence of the IER and SEOI functionality is identified by bits 0 and 1, respectively, of the APIC Extended Feature Register (located at offset 400h in APIC space). IER and SEOI are enabled by setting bits 0 and 1, respectively, of the APIC Extended Control Register (located at offset 410h).
    */

    {
	if (nk_is_amd()) {
	    if (amd_has_ext_lvt(apic)) {
		APIC_DEBUG("AMD APIC has extended space area\n");
		uint32_t e = apic_read(apic, APIC_REG_EXFR);
		if (e & 0x1) { 
		    APIC_DEBUG("AMD APIC has IER\n");
		} else {
		    APIC_DEBUG("AMD APIC does NOT have IER\n");
		}
	    } else {
		APIC_DEBUG("AMD APIC does NOT have extended space area\n");
	    }
	} else {
	    APIC_DEBUG("Not an AMD APIC\n");
	} 
	
    }
    

    /* turn it on */
    apic_sw_enable(apic);

    /* pass in quantum as milliseconds */
#ifndef NAUT_CONFIG_XEON_PHI
    apic_timer_setup(apic, 1000/NAUT_CONFIG_HZ);
#endif

    apic_dump(apic);

    char n[32];
    snprintf(n,32,"apic%u",core->id);
    nk_dev_register(n,NK_DEV_INTR,0,&ops,apic);
}



void apic_set_oneshot_timer(struct apic_dev *apic, uint32_t ticks) 
{
    apic_write(apic, APIC_REG_LVTT, APIC_TIMER_ONESHOT | APIC_DEL_MODE_FIXED | APIC_TIMER_INT_VEC);
    apic_write(apic, APIC_REG_TMDCR, APIC_TIMER_DIVCODE);

    if (!ticks) {
	ticks=1; 
    }
    apic_write(apic, APIC_REG_TMICT, ticks);
    apic->timer_set = 1;
    apic->current_ticks = ticks;
}

void apic_update_oneshot_timer(struct apic_dev *apic, uint32_t ticks,
			       nk_timer_condition_t cond)
{
    if (!apic->timer_set) { 
	apic_set_oneshot_timer(apic,ticks);
    } else {
	switch (cond) { 
	case UNCOND:
	    apic_set_oneshot_timer(apic,ticks);
	    break;
	case IF_EARLIER:
	    if (ticks < apic->current_ticks) { apic_set_oneshot_timer(apic,ticks);}
	    break;
	case IF_LATER:
	    if (ticks > apic->current_ticks) { apic_set_oneshot_timer(apic,ticks);}
	    break;
	}
    }
    // note that this is set at the entry to apic_timer_handler
    apic->in_timer_interrupt=0;
    // note that this is set at the entry to null_kick
    apic->in_kick_interrupt=0;
}
	    



uint32_t apic_cycles_to_ticks(struct apic_dev *apic, uint64_t cycles)
{
    return cycles/apic->cycles_per_tick;
}

uint32_t apic_realtime_to_ticks(struct apic_dev *apic, uint64_t ns)
{
    return ((ns*1000ULL)/apic->ps_per_tick);
}


uint64_t apic_realtime_to_cycles(struct apic_dev *apic, uint64_t ns)
{
    return (ns*apic->cycles_per_us)/1000ULL;
}

uint64_t apic_cycles_to_realtime(struct apic_dev *apic, uint64_t cycles)
{
    return 1000ULL*(cycles/(apic->cycles_per_us));
}


// this is 10 ms (1/100)
#define TEST_TIME_SEC_RECIP 100
#define MAX_TRIES 100


// Currently modes 0 and 1 are supported.   Try mode 0 first
static int calibrate_apic_timer_using_pit(struct apic_dev *apic, int mode)
{
    uint64_t start, end;
    uint16_t pit_count;
    uint32_t tries = 0;


try_once:

    // First determine the APIC's bus frequency by calibrating it
    // against a known clock (the PIT).   We do not know the base
    // rate of this APIC, but we do know the base rate of all PITs.
    // The PIT counts at about 1193180 Hz (~1.2 MHz)
    //

    pit_count = (1193180 / TEST_TIME_SEC_RECIP);

    if (mode==1) { 
	// In the way we are using the PIT, it will count one more than
	// the target count, hence the "- 1" in the following:
	pit_count--;
    }
    

    // Use APIC timer in one shot mode with the divider we will use 
    // in normal execution.  We will count down from a large number
    // and do not expect interrupts because it should not hit zero.
    // timer is masked because we don't want an interrupt to occur at all
    // On KNL, masking also seems to have the side effect of reseting the count

    apic_write(apic, APIC_REG_LVTT, APIC_TIMER_ONESHOT | APIC_DEL_MODE_FIXED | APIC_TIMER_INT_VEC | APIC_TIMER_MASK);
    apic_write(apic, APIC_REG_TMDCR, APIC_TIMER_DIVCODE);


    //
    // The following logic is based on the Intersil 8254 datasheet,
    // combined with the well-known logic for how the 8254 is used in
    // legacy support on PC-compatible platforms.
    //
    // The 8254 PIT appears in legacy land at ports 
    // The 8254 PIT has 3 counters wired on a PC as follows:
    //
    //    0: IRQ 0 => not used in Nautilus
    //    1: DMA refresh => not used in any modern hardware
    //    2: Speaker/Output => we use this, as is common
    //        
    // Counter 2 is gated by an output of the keyboard controller, and
    // its output is controlled via the same path.  Its output is
    // made available on an input of the keyboard controller.
    // These are visible at port 0x61 in legacy land.
    //
    // At this point, all interrupts are off and masked on the CPU.
    // Also, the 8254 is in reset state, which means it is undefined,
    // but also that it will not raise any output from any counter.
    //
    // An 8254 counter is trivial. First you write a mode for counting
    // on the counter to the 8254's command register (0x43).
    // Next you write a count to the port corresonding to the counter
    // (0x40-0x42) with the LSB first, then the MSB.  A gate input
    // determines if the counter runs.  The gates are controlled via
    // port 0x61.  In some modes, if the gate is high, the write of the MSB
    // will start the counter.   In others, you need to use the the gate
    // as an edge trigger to start it.
    //
    //
    // In mode 0 we will use the PIT (8254)'s counter 2 in mode 0 "Interrupt on
    // Terminal Count" and keep the gate high.  In this one-shot mode,
    // the LSB write to the initial count register will start the
    // counter.  The output will transition low before the LSB write,
    // and then high after the count is done.
    //
    // In mode 1 we will use the PIT (8254)'s counter 2 in mode 1 "Hardware
    // Retriggerable One-shot" and give the gate a positive edge.   In this
    // one-shot mode, it is the gate edge that causes it to start counting
    // The output will transition low after the gate edge and then high
    // after the count is done. 
    //
    //

    uint8_t gate, detect, ctrl;

    // 
    // configure PIT channel 2 for terminal count in binary
    //
    // 10 11 000 0 (mode 0) or 10 11 001 0 (mode 1)

    ctrl = PIT_CHAN(PIT_CHAN_SEL_2) | PIT_ACC_MODE(PIT_ACC_MODE_BOTH);

    if (mode==0) {
	ctrl |= PIT_MODE(PIT_MODE_TERMINAL_COUNT) ;
    } else {
	ctrl |= PIT_MODE(PIT_MODE_ONESHOT);
    }

    outb(ctrl,PIT_CMD_REG);

    // PIT  channel 2 is wired to the keyboard controller, which
    // allows to read when the PIT count hits zero.  Normally this path
    // is used to buzz the internal speaker (really!).
    // Port 0x61 bit 0 gates channel 2 counter
    //         mode 0: high to run
    //         mode 1: positive edge to run
    // Port 0x61 bit 1 gates channel 2 output to physical speaker (=1 to run speaker)
    // Port 0x61 bit 5 is channel 2 counter output (regardless of speaker being driven)

    // Now enable it - we want gate on, speaker off
    
    gate = inb(KB_CTRL_PORT_B);

    gate &= 0xfc; // channel 2 speaker output disabled

    if (mode==0) { 
	gate |= 0x1;  // channel 2 gate enabled, write of count will start counter
    } else {
	// gate disabled, write of gate will start counter
    }

    outb(gate,KB_CTRL_PORT_B);

    // PIT channel 2 counter is now waiting for a count to start/restart
    // we do this LSB then MSB

    outb(pit_count & 0xff, PIT_CHAN2_DATA); // LSB

    io_delay();

    outb((uint8_t)(pit_count >> 8), PIT_CHAN2_DATA); // MSB

    if (mode==0) { 
	// The PIT counter is now running
    } else {
	// pulse
	ctrl = inb(KB_CTRL_PORT_B);
	ctrl &= 0xfe;
	outb(ctrl, KB_CTRL_PORT_B);   // force gate low if it's not alread
	ctrl |= 0x1;                
	outb(ctrl, KB_CTRL_PORT_B); // force gate high to give positive edge
	// the PIT counter is now running
    }

    /// reset apic timer to our count down value 
    apic_write(apic, APIC_REG_TMICT, 0xffffffff);


    // we need to be sure we are not racing with a 1->0 transition on the
    // output line.  Therefore we will wait for at least one clock interval at
    // 1.19.. Mhz which is 838 ns.   a port 0x80 read will take 1 us, so
    // we introduce at least 2 us here to be safe
    io_delay();
    io_delay();
    
    int count =0;
    start = rdtsc();
    // we are now waiting for the PIT to finish
    // We are much faster than the PIT, so this loop should spin at least once
    while (1) {
	detect = inb(KB_CTRL_PORT_B);
	if ((detect & 0x20)) {
	    break;
	}
	count++;
    }
    end = rdtsc();

    if (count==0) {
	// this is a weird machine that does not get this mode right...
	APIC_PRINT("PIT scan count of zero encountered, failing calibration for mode %d\n",mode);
	return -1;
    }

    //APIC_DEBUG("After %d iterations, PIT has finished\n", count);

    // a known amount of real-time has now finished

    // Now we have 1/TEST_TIME_SEC_RECIP seconds of real time in APIC timer ticks
    uint32_t apic_timer_ticks = 0xffffffff - apic_read(apic,APIC_REG_TMCCT) + 1;

    APIC_DEBUG("One test period (1/%u sec) took %u APIC ticks, pit_count=%u, and %lu cycles\n",
	       TEST_TIME_SEC_RECIP,apic_timer_ticks,(unsigned) pit_count, end-start);
    
    // occasionally, real hardware can provide surprise
    // results here, probably due to an SMI.
    if (tries < MAX_TRIES && (end-start < 1000000 || apic_timer_ticks<100)) { 
	APIC_DEBUG("Test Period is impossible - trying again\n");
	tries++;
	goto try_once;
    }

#ifdef NAUT_CONFIG_GEM5_FORCE_APIC_TIMER_CALIBRATION
    apic->bus_freq_hz = NAUT_CONFIG_GEM5_APIC_BUS_FREQ_HZ;
#else
    apic->bus_freq_hz = APIC_TIMER_DIV * apic_timer_ticks * TEST_TIME_SEC_RECIP;
#endif
    
    APIC_DEBUG("Detected APIC 0x%x bus frequency as %lu Hz\n", apic->id, apic->bus_freq_hz);

    // picoseconds are used to try to keep precision for ns and cycle -> tick conversions
    apic->ps_per_tick = (1000000000000ULL / apic->bus_freq_hz) * APIC_TIMER_DIV;

    APIC_DEBUG("Detected APIC 0x%x real time per tick as %lu ps\n", apic->id, apic->ps_per_tick);

#ifdef NAUT_CONFIG_GEM5_FORCE_APIC_TIMER_CALIBRATION
    apic->cycles_per_us = NAUT_CONFIG_GEM5_APIC_CYCLES_PER_US;
#else
    // us are used here to also keep precision for cycle->ns and ns->cycles conversions
    apic->cycles_per_us = ((end - start) * TEST_TIME_SEC_RECIP)/1000000ULL;
#endif

    APIC_DEBUG("Detected APIC 0x%x cycles per us as %lu (core at %lu Hz)\n",apic->id,apic->cycles_per_us,apic->cycles_per_us*1000000); 
    
    if (!apic->bus_freq_hz || !apic->ps_per_tick || !apic->cycles_per_us) { 
	APIC_ERROR("Detected time calibration numbers cannot be zero....\n");
	return -1;
    }

    APIC_DEBUG("Succeeded in calibration with mode %d - spun for %d iterations\n", mode,count);
    
    return 0;

}

static void calibrate_apic_timer(struct apic_dev *apic) 
{

#ifndef NAUT_CONFIG_APIC_TIMER_CALIBRATE_INDEPENDENTLY
    if (!apic_is_bsp(apic)) {
	// clone core bsp, assuming it is already up
	//extern struct naut_info nautilus_info;
	struct apic_dev *bsp_apic = nautilus_info.sys.cpus[0]->apic;
	apic->bus_freq_hz = bsp_apic->bus_freq_hz;
	apic->ps_per_tick = bsp_apic->ps_per_tick;
	apic->cycles_per_us = bsp_apic->cycles_per_us;
	apic->cycles_per_tick = bsp_apic->cycles_per_tick;
	
	APIC_DEBUG("AP APIC id=0x%x cloned BSP APIC's timer configuration\n",
		   apic->id);

	return;
    }

#endif

    int mode;

    // We use PIT calibration, trying mode 0 first, which should work correctly
    // on all machines and on emulation (Phi, Qemu).
    if (mode=0, calibrate_apic_timer_using_pit(apic,mode)) {
	// if we fail, we will try mode 1, which hopefully will work correctly on
	// most real hardware
	APIC_DEBUG("Failed calibration using PIT mode 0, retrying with mode 1\n");
	if (mode=1, calibrate_apic_timer_using_pit(apic,mode)) {
	    APIC_ERROR("Failed calibration PIT modes 0 and 1.... Time has no meaning here...\n");
	    panic("Failed calibration PIT modes 0 and 1.... Time has no meaning here...\n");
	    return;
	}
    }

    
    /////////////////////////////////////////////////////////////////
    // Now we will determine the calibration of the TSC to APIC time
    ////////////////////////////////////////////////////////////////

// Gem5 is ungodly slow and will not show variation
#ifdef NAUT_CONFIG_GEM5
#define NUM_TRIALS 1
#define LOOP_ITERS 1000
#else
#define NUM_TRIALS 50
#define LOOP_ITERS 1000000
#endif

    const uint64_t num_trials = NUM_TRIALS;
    uint64_t tsc_diff;
    uint64_t apic_diff;
    uint64_t scale_sum = 0;
    uint64_t scale_min = -1;
    uint64_t scale;
    uint64_t start, end;

    int i = 0;

    extern void nk_simple_timing_loop(uint64_t iter_count);

    for (i = 0; i < num_trials; i++) {
	// set APIC for a long countdown time, longer than our test 
	// mask to avoid interrupt, also to deal with mask side effect on KNL
	apic_write(apic, APIC_REG_LVTT, APIC_TIMER_ONESHOT | APIC_DEL_MODE_FIXED | APIC_TIMER_INT_VEC | APIC_TIMER_MASK);
	apic_write(apic, APIC_REG_TMDCR, APIC_TIMER_DIVCODE);
	// start it
	apic_write(apic, APIC_REG_TMICT, 0xffffffff);
	start = rdtsc();

	
	// now time a random amount of cycle burning
	// with both the tsc and the apic timer
	nk_simple_timing_loop(LOOP_ITERS);

	// now collect time using both
	end = rdtsc();
	tsc_diff = (end - start);

	apic_diff = (0xffffffff - apic_read(apic, APIC_REG_TMCCT) + 1);

	scale = tsc_diff / apic_diff;
	scale_sum += scale;

	if (scale<scale_min) { 
	    scale_min=scale; 
	}
    }

    apic->cycles_per_tick = scale_sum/num_trials;

    APIC_PRINT("Detected APIC 0x%x at %lu Hz and cycles per us as %lu (core at %lu Hz) calibration mode=%d\n",apic->id,apic->bus_freq_hz,apic->cycles_per_us,apic->cycles_per_us*1000000,mode); 
    APIC_PRINT("Detected APIC 0x%x CPU cycles per tick as %lu cycles (min was %lu)\n", apic->id, apic->cycles_per_tick,scale_min);

}


static int apic_timer_handler(excp_entry_t * excp, excp_vec_t vec, void *state)
{
    NK_GPIO_OUTPUT_MASK(0x2,GPIO_OR);

    struct apic_dev * apic = (struct apic_dev*)per_cpu_get(apic);

    uint64_t time_to_next_ns;

    apic->in_timer_interrupt=1;

    apic->timer_count++;

    apic->timer_set = 0;

    // do all our callbacks
    // note that currently all cores see the events
    time_to_next_ns = nk_timer_handler();

    // note that the low-level interrupt handler code in excp_early.S
    // takes care of invoking the scheduler if needed, and the scheduler
    // will in turn change the time after we leave - it may set the
    // timer to expire earlier.  The scheduler can refer to 
    // the apic to determine that it is being invoked in timer interrupt
    // context.   In this way, it can differentiate:
    //   1. invocation from a thread
    //   2. invocation from a interrupt context for some other interrupt
    //   3. invocation from a timer interrupt context
    // This is important as a flaky APIC timer (we're looking at you, QEMU)
    // can fire before the expected elapsed time expires.  If the scheduler
    // doesn't reset the timer when this happens, the scheduler may be delayed
    // as far as the next interrupt or cooperative rescheduling request,
    // breaking real-time semantics.  

    if (time_to_next_ns == -1) { 
	// indicates "infinite", which we turn into the maximum timer count
	apic_set_oneshot_timer(apic,-1);
    } else {
	apic_set_oneshot_timer(apic,apic_realtime_to_ticks(apic,time_to_next_ns));
    }

    IRQ_HANDLER_END();

    NK_GPIO_OUTPUT_MASK(~0x2,GPIO_AND);

    return 0;
}

static int
handle_int (char * buf, void * priv)
{
    int cpu, intr;

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

    nk_vc_printf("invalid int command\n");

    return 0;
}

static struct shell_cmd_impl int_impl = {
    .cmd      = "int",
    .help_str = "int [cpu] v",
    .handler  = handle_int,
};
nk_register_shell_cmd(int_impl);
