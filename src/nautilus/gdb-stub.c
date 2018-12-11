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
 * Copyright (c) 2017, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author:  Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/*
  This is an implementation of the gdb stub interface for Nautilus

  It is influenced by the HP public domain code contributed to the 
  gdb codebase (e.g., i386-stub.c). 

  This is tested on gdb 7.11 on ubuntu:

  GNU gdb (Ubuntu 7.11.1-0ubuntu1~16.5) 7.11.1

  with QEMU

*/

/*
  To use this functionality, configure remote debugging on, 
  and a debug serial port on.  For typical testing, we 
  have the debug serial port on COM1 and the serial redirect/log
  output port on COM2.  You typically also want to configure Nautilus
  to attach to the debugger at boot.  To do source level debugging
  Nautilus needs to be compiled with debugging symbols.   Note that
  we typically compile with optimization as well so that back-to-source
  mapping is rarely perfect. 

  Assuming you are attaching the debugger at boot, and you are
  working on QEMU, then configure the QEMU machine to put the
  debug serial port somewhere that is accessible, for example:

  qemu-system-x86_64 -vga std -smp 4 -m 2048 
       -chardev socket,host=localhost,port=5000,id=g,server 
          -device isa-serial,chardev=g 
       -chardev stdio,id=d 
          -device isa-serial,chardev=d
       -cdrom nautilus.iso 

  This places COM1 behind a tcp server at port 5000
  
  Then bring up gdb:

  $ /usr/bin/gdb nautilus.bin
  GNU gdb (Ubuntu 7.11.1-0ubuntu1~16.5) 7.11.1
  ...
  (gdb)

  And attach

  (gdb) target remote localhost:5000

  At this point, you'll see Nautilus boot an then pause at "attaching 
  to debugger now".  And gdb will show you something like:

  (gdb) target remote localhost:5000
  `/home/pdinda/test/nautilus/nautilus.bin' has changed; re-reading symbols.
  Remote debugging using localhost:5000
  nk_gdb_init () at src/nautilus/gdb-stub.c:1349
  1349	}
  (gdb) 

  You can now set breakpoints, etc.   When you wan the boot to continue,
  just type continue as usual:
  
  (gdb) continue

  The debugger will regain control under the following situations:

  - An exception occurs on any cpu
  - A breakpoint is hit on any cpu
  - nk_gdb_breakpoint_here() is hit on any cpu
  - you have the shell and use its "break" command
  - you have PS2 support and hit the debug key
    (currently F8) on the keyboard
  - you have debug interrupt support for the 
    debug serial driver (this is on by default) and
    you hit CTRL-C in GDB.

  For example, hitting CTRL-C in gdb:

  (gdb) continue
  Continuing.
  ^C[New Thread 2]
  [New Thread 3]
  [New Thread 4]

  Thread 1 received signal SIGINT, Interrupt.
  0x0000000000321288 in halt () at include/nautilus/cpu.h:298
  298	    asm volatile ("hlt");
  (gdb) 

  The model here is that each CPU is a thread.  When the debugger
  regains control, all CPUs are stopped.  

  For a 4 CPU  setup, you might see something like this:

  (gdb) info threads
    Id   Target Id         Frame 
  * 1    Thread 1 (cpu0)   0x0000000000321288 in halt () at include/nautilus/cpu.h:298
    2    Thread 2 (cpu1)   msr_read (msr=msr@entry=3221225729) at src/nautilus/msr.c:37
    3    Thread 3 (cpu2)   0x0000000000321288 in halt () at include/nautilus/cpu.h:298
    4    Thread 4 (cpu3)   io_delay () at include/nautilus/cpu.h:341

  And then we can see what's up on each CPU:

  (gdb) bt
  #0  0x0000000000321288 in halt () at include/nautilus/cpu.h:298
  #1  idle (in=<optimized out>, out=<optimized out>) at src/nautilus/idle.c:85
  #2  0x0000000000000000 in ?? ()

  [Halting in idle()]

  (gdb) thread 2
  [Switching to thread 2 (Thread 2)]
  #0  msr_read (msr=msr@entry=3221225729) at src/nautilus/msr.c:37
  37	{
  (gdb) bt
  #0  msr_read (msr=msr@entry=3221225729) at src/nautilus/msr.c:37
  #1  0x000000000033ac9e in __cpu_state_get_cpu () at include/nautilus/cpu_state.h:43
  #2  preempt_is_disabled () at include/nautilus/cpu_state.h:84
  #3  handle_special_switch (what=YIELDING, have_lock=0, flags=0 '\000', 
      lock_to_release=0x102028) at src/nautilus/scheduler.c:2636
  #4  0x000000000032127b in idle (in=<optimized out>, out=<optimized out>)
      at src/nautilus/idle.c:75
  #5  0x0000000000000000 in ?? ()

  [Yielding in idle()]

  (gdb) thread 4 
  [Switching to thread 4 (Thread 4)]
  #0  io_delay () at include/nautilus/cpu.h:341
  341	    asm volatile("outb %%al,%0" : : "dN" (DELAY_PORT));
  (gdb) bt
  #0  io_delay () at include/nautilus/cpu.h:341
  #1  udelay (n=896) at include/nautilus/cpu.h:357
  #2  burner (in=0x10b400, out=<optimized out>) at src/nautilus/shell.c:113
  #3  0x0000000000325570 in ?? () at src/nautilus/thread.c:601
  #4  0x0000000000000000 in ?? ()

  [Doing a delay in a test "burner" thread]

  CAVEATS
  -------
     - per-cpu variables are problematic since fsbase/gsbase has only 
       recently been supported in GDB, and this code defaults to not
       including them.   If you enable fsbase/gsbase for use with 
       GDB 8+, note that this may still not handle them correctly.   
       The best approach is probably to use the global array index
       system->cpus[i]... instead
     - don't change the segment registers from gdb (there is no need to
       ever do this in Nautilus anyway).   Other registers are fair game
     - there is no current support for floating point state.   It does 
       convey x87 and base SSE regs as required, but these are not 
       populated.   Easy to add if necessary at some point.  Same thing
       for advanced SSE, AVX, AVX*, etc. 
     - it is possible to avoid attaching gdb at startup (you can 
       disable this option).   When you do attach gdb ("target remote")
       you will need to have it send the interrupt (CTRL-C), though,
       or else interrupt gdb through some other means (F8, intentional
       breakpoint, etc).   There is nothing "special" about attaching
       gdb at boot - an attach is simply the result of processing any
       exception. 
 */

#include <nautilus/nautilus.h>
#include <nautilus/msr.h>
#include <dev/serial.h>
#include <nautilus/idt.h>
#include <nautilus/shutdown.h>
#include <nautilus/scheduler.h>
#include <nautilus/shell.h>
#include <nautilus/gdb-stub.h>
#include <nautilus/cpu_state.h>

/*
  Note that gdb 7 does not have support for FSbase/GSbase, but
  gdb 8 does. 

  For 7:  SKIP_WRITE_FSGS=1, NEED_ORIG_RAX=1, NEED_FSGS_BASE=0
  For 8:  ????  TODO
*/

#define SKIP_WRITE_FSGS 1   
#define NEED_ORIG_RAX   1
#define NEED_FSGS_BASE  0

#ifdef NAUT_CONFIG_DEBUG_REMOTE_DEBUGGER
#define DEBUG(fmt, args...)  DEBUG_PRINT("gdb: " fmt, ##args)
#else
#define DEBUG(fmt, args...)
#endif

#define ERROR(fmt, args...) ERROR_PRINT("gdb: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("gdb: " fmt, ##args)


#ifndef NAUT_CONFIG_SERIAL_DEBUGGER
#error Serial Debugging Device Is Not Enabled!
#endif

static inline uint8_t get_gdb()
{
    uint8_t c;

    serial_debugger_get(&c);
    return c;
}

static inline void put_gdb(uint8_t c)
{
    serial_debugger_put(c);
}


// This should never have any meaning in modern gdb
static int     remote_debug = 0;
/*  debug >  0 prints ill-formed commands in valid packets & checksum errors */

static const char hexchars[]="0123456789abcdef";


// This describes the register layout and ordering that GDB expects
// it conforms to gdb/regformats/i386/amd64-linux.dat or whichever
// architectural register description we want to use.
// Note that the enhanced SSE/AVX register sets (ymms, zmms, etc)
// are optional and are not included here.   Also note that
// Although we have included the x87 and SSE registers (xmms) here, we
// are not currently handling them (they are always zero and cannot be
// set)
//
// ORIG_RAX is to support linux system call restarts.  We do need to handle
// it, but it has no meaning in this kernel
//
// FSBASE/GSBASE are part of the gdb 8.0 register spec, and we should handle
// them at some point - right now, it's very important that we do not
// ever write FS or GS since theose also bork the base stuff, resulting in
// mysterious failures relating to per-cpu variables
typedef enum reg_name  {
    RAX = 0, // 64 bit
    RBX,
    RCX,
    RDX,
    RSI,
    RDI,
    RBP,
    RSP,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,

    //# 16
    RIP,     // @ + 8*16
    EFLAGS,  // @ + 8*17, size 32
    CS,      // @ + 8*17 + 4  segs are 32...
    SS,
    DS,
    ES,
    FS,
    GS,

    /* FIXME: We must have the basic x87 and SSE FP, but we will fake these */

    // # 24
    ST0,    // @ + 8*17 + 4*7 // STS are 80
    ST1,    
    ST2,    
    ST3,    
    ST4,    
    ST5,    
    ST6,
    ST7,  
    // #32
    FCTRL,  // @ + 8*17 + 4*7 + 8*10 // these are 32
    FSTAT,
    FTAG,
    FISEG,
    FIOFF,
    FOSEG,
    FOOFF,
    FOP,
    
    // #40
    XMM0,  // @ + 8*17 + 4*7 + 8*10 + 8*4 // 128 bits
    XMM1,
    XMM2,
    XMM3,
    XMM4,
    XMM5,
    XMM6,
    XMM7,
    XMM8,
    XMM9,
    XMM10,
    XMM11,
    XMM12,
    XMM13,
    XMM14,
    XMM15,
    // #56
    MXCSR, // @ + 8*17 + 4*7 + 8*10 + 8*4 + 16*16 // 32 bits
    
#if NEED_ORIG_RAX
    // #57
    ORIG_RAX, // @ @ + 8*17 + 4*7 + 8*10 + 8*4 + 16*16 + 4 // 64 bits
#endif

#if NEED_ORIG_FSGS_BASE
    // #58
    FS_BASE, // 64 bits
    GS_BASE, // 64 bits
#endif

    /* We are ignoring AVX (ymms), and AVX2/512 (zmms)  groups 
       Which would go here */
} reg_name_t;

   
#define MIN_REG_NAME RAX

/* Number of bytes of registers.  */
// not including FSBASE/GSBASE
#define NUM_REG_BYTES_CORE (8*17 + 4*7 + 8*10 + 8*4 + 16*16 + 4)
#if NEED_ORIG_RAX
#if NEED_FSGS_BASE
#define NUM_REG_BYTES (NUM_REG_BYTES_CORE + 8*3)
#define MAX_REG_NAME GS_BASE
#else
#define NUM_REG_BYTES (NUM_REG_BYTES_CORE + 8*1)
#define MAX_REG_NAME ORIG_RAX
#endif
#else
// !NEED_ORIG_RAX
#define NUM_REG_BYTES NUM_REG_BYTES_CORE
#define MAX_REG_NAME MXCSR
#endif 

// This is safe only for the "world stop" model
// where the cpu that took the exception owns it
// and decides which cpu's state currently resides
// in it.
static char registers[NUM_REG_BYTES];



static void gdb_reg_loc(reg_name_t n, int *off, int *size)
{
    if (n>=RAX && n<EFLAGS) {
        *off = 8*(n-RAX);
        *size = 8;
    }
    if (n>=EFLAGS && n<ST0) { 
        *off = (8*17) + (n-EFLAGS)*4;
        *size = 4;
    }
    if (n>=ST0 && n<=ST7) { 
        *off = (8*17 + 4*7) + (n-ST0)*10;
        *size= 10;
    }
    if (n>=FCTRL && n<=FOP) { 
        *off = (8*17 + 4*7 + 8*10) + (n-FCTRL)*4;
        *size = 4;
    }
    if (n>=XMM0 && n<=XMM15) { 
        *off = (8*17 + 4*7 + 8*10 + 8*4) + (n-XMM0)*16;
        *size = 16;
    }
    if (n==MXCSR) { 
        *off = (8*17 + 4*7 + 8*10 + 8*4 + 16*16);
        *size = 4;
    }
#if NEED_ORIG_RAX
    if (n==ORIG_RAX) { 
        *off = (8*17 + 4*7 + 8*10 + 8*4 + 16*16 + 4);
        *size = 8;
    }
#if NEED_FSGS_BASE
    if (n>=FS_BASE && n<=GS_BASE) { 
        *off = (8*17 + 4*7 + 8*10 + 8*4 + 16*16 + 4 + 8) + (n-FS_BASE)*8;
        *size = 8;
    }
#endif
#endif
}


static void get_reg(reg_name_t r, char *val)
{
    int off, len;
    gdb_reg_loc(r,&off,&len);
    memcpy(val, registers+off,len);
}

static void set_reg(reg_name_t r, char *val)
{
    int off, len;
    gdb_reg_loc(r,&off,&len);
    memcpy(registers+off,val,len);
}


/* Address of a routine to RTE to if we get a memory fault.  */
static void (*volatile mem_fault_routine) () = NULL;

/* Indicate to caller of mem2hex or hex2mem that there has been an
   error.  */
static volatile int mem_err = 0;

static void set_mem_err(void)
{
    mem_err = 1;
}

/* These are separate functions so that they are so short and sweet
   that the compiler won't save any registers (if there is a fault
   to mem_fault, they won't get restored, so there better not be any
   saved).  */
static inline int peek(char *addr)
{
    return *(volatile char *)addr;
}

static inline void poke(char *addr, int val)
{
    *(volatile char *)addr = val;
}


static int hex(char ch)
{
    if ((ch >= 'a') && (ch <= 'f')) {
        return (ch - 'a' + 10);
    }
    if ((ch >= '0') && (ch <= '9')) {
        return (ch - '0');
    }
    if ((ch >= 'A') && (ch <= 'F')) {
        return (ch - 'A' + 10);
    }
    return (-1);
}


   

/* convert the memory pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
/* If MAY_FAULT is non-zero, then we should set mem_err in response to
   a fault; if zero treat a fault like any other fault in the stub.  */
static char *mem2hex(char *mem, char *buf, int count, int may_fault)
{
    int i;
    unsigned char ch;
    
    if (may_fault) {
        mem_fault_routine = set_mem_err;
    }

    for (i = 0; i < count; i++) {
	
        ch = peek(mem++);
	
        if (may_fault && mem_err) {
            return (buf);
        }

        *buf++ = hexchars[ch >> 4];
        *buf++ = hexchars[ch % 16];
    }

    *buf = 0;
    if (may_fault) {
        mem_fault_routine = NULL;
    }

    return (buf);
}

static char *byte_to_hex(char byte, char *buf, int may_fault)
{
    return mem2hex(&byte,buf,1,may_fault);
}


/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
static char *hex2mem(char *buf, char *mem, int count, int may_fault)
{
    int i;
    unsigned char ch;
    
    if (may_fault) {
        mem_fault_routine = set_mem_err;
    }

    for (i = 0; i < count; i++) {
	
        ch = hex(*buf++) << 4;
        ch = ch + hex(*buf++);

        poke(mem++, ch);

        if (may_fault && mem_err) {
            return (mem);
        }
    }

    if (may_fault) {
        mem_fault_routine = NULL;
    }

    return (mem);
}

static char *hex_to_byte(char *buf, char *byte, int may_fault)
{
    return hex2mem(buf,byte,1,may_fault);
}



static char *reg_val_to_hex(reg_name_t r, char *buf, int may_fault)
{
    int off, len;
    gdb_reg_loc(r,&off,&len);
    return mem2hex(registers+off,buf,len,may_fault);
}

// set = convert hex to register value
static char *hex_to_reg_val(char *buf, reg_name_t r, int may_fault)
{
    int off, len;
    gdb_reg_loc(r,&off,&len);
    return hex2mem(buf,registers+off,len,may_fault);
}

// assume we have at most 256...
static char *hex_to_reg_name(char *buf, reg_name_t *name, int may_fault)
{
    char n;
    char *p = hex2mem(buf,&n,1,may_fault);
    *name = (reg_name_t) n;
    return p;
}

// assume we have at most 256...
static char *reg_name_to_hex(reg_name_t r, char *buf, int may_fault)
{
    char n = (char) r;
    return mem2hex(&n,buf,1,may_fault);
}

// What the GPRs pushed onto the stack by early_excp_common
// or early_irq_common via SAVE_GPRS() (see thread.h and
// excp_early.S) look like in memory
struct excp_gpr_state {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
} __packed;

/*
  Stack sees:

  exception_context  <- we get ptr to lowest addr
  GPRs saved by excp_early <- we want this, it's at -128
*/

#define EXCP_OFFSET_TO_GPRS -128


static void snapshot_regs(excp_entry_t * excp)
{
    int i;

    struct excp_gpr_state *r = (struct excp_gpr_state*)((addr_t)excp + EXCP_OFFSET_TO_GPRS);

    memset(registers,0,NUM_REG_BYTES);

    set_reg(RAX,(char*)&r->rax);
    set_reg(RBX,(char*)&r->rbx);
    set_reg(RCX,(char*)&r->rcx);
    set_reg(RDX,(char*)&r->rdx);
    set_reg(RSI,(char*)&r->rsi);
    set_reg(RDI,(char*)&r->rdi);
    set_reg(RBP,(char*)&r->rbp);
    set_reg(R8, (char*)&r->r8);
    set_reg(R9, (char*)&r->r9);
    set_reg(R10,(char*)&r->r10);
    set_reg(R11,(char*)&r->r11);
    set_reg(R12,(char*)&r->r12);
    set_reg(R13,(char*)&r->r13);
    set_reg(R14,(char*)&r->r14);
    set_reg(R15,(char*)&r->r15);
    
    // get critical data directly from the exception structure
    set_reg(CS,(char*)&excp->cs);
    set_reg(RIP,(char*)&excp->rip);
    set_reg(SS,(char*)&excp->ss);
    set_reg(RSP,(char*)&excp->rsp);
    set_reg(EFLAGS,(char*)&excp->rflags);

    // and DS, GS, FS, ES
    uint32_t val=0;

    __asm__ __volatile__ ("movw %%ds, %0" : "=m"(val) : : "memory");
    val &= 0xffff;
    set_reg(DS,(char*)&val);
    __asm__ __volatile__ ("movw %%gs, %0" : "=m"(val) : : "memory");
    val &= 0xffff;
    set_reg(GS,(char*)&val);
    __asm__ __volatile__ ("movw %%fs, %0" : "=m"(val) : : "memory");
    val &= 0xffff;
    set_reg(FS,(char*)&val);
    __asm__ __volatile__ ("movw %%es, %0" : "=m"(val) : : "memory");
    val &= 0xffff;
    set_reg(ES,(char*)&val);

#if NEED_ORIG_RAX
    set_reg(ORIG_RAX,(char*)&r->rax);
#endif

#if NEED_FSGS_BASE
    uint64_t base;
    base = msr_read(MSR_FS_BASE);
    set_reg(FS_BASE, (char*)&base);
    base = msr_read(MSR_GS_BASE);
    set_reg(GS_BASE, (char*)&base);
#endif

    // FIXME - copy out floating point state

}

static void update_regs(excp_entry_t * excp)
{
    int i;

    struct excp_gpr_state *r = (struct excp_gpr_state*)((addr_t)excp + EXCP_OFFSET_TO_GPRS);

    get_reg(RAX,(char*)&r->rax);
    get_reg(RBX,(char*)&r->rbx);
    get_reg(RCX,(char*)&r->rcx);
    get_reg(RDX,(char*)&r->rdx);
    get_reg(RSI,(char*)&r->rsi);
    get_reg(RDI,(char*)&r->rdi);
    get_reg(RBP,(char*)&r->rbp);
    get_reg(R8, (char*)&r->r8);
    get_reg(R9, (char*)&r->r9);
    get_reg(R10,(char*)&r->r10);
    get_reg(R11,(char*)&r->r11);
    get_reg(R12,(char*)&r->r12);
    get_reg(R13,(char*)&r->r13);
    get_reg(R14,(char*)&r->r14);
    get_reg(R15,(char*)&r->r15);
    
    // get critical data directly from the exception structure
    get_reg(CS,(char*)&excp->cs);
    get_reg(RIP,(char*)&excp->rip);
    get_reg(SS,(char*)&excp->ss);
    get_reg(RSP,(char*)&excp->rsp);
    get_reg(EFLAGS,(char*)&excp->rflags);

    // and DS, ES
    uint32_t val=0;

    get_reg(DS,(char*)&val);
    val &= 0xffff;
    __asm__ __volatile__ ("movw %0, %%ds" : : "m"(val) : "memory");
    get_reg(ES,(char*)&val);
    val &= 0xffff;
    __asm__ __volatile__ ("movw %0, %%es" : : "m"(val) : "memory");

    // Optionally update the FS/GS regs
    // note that if the target is using the GSBASE and FSBASE
    // MSRs, this could screw them up if we are also not updating
    // FSBASE/GSBASE MSRs *AFTER* this
    // We also need to be sure that nothing is using gs or fs between
    // these updates and the msr writes... 
#if SKIP_WRITE_FSGS
    // nothing
#else
    get_reg(GS,(char*)&val);
    val &= 0xffff;
    __asm__ __volatile__ ("movw %0, %%gs" : : "m"(val) : "memory");
    get_reg(FS,(char*)&val);
    val &= 0xffff;
    __asm__ __volatile__ ("movw %0, %%fs" : : "m"(val) : "memory");
#endif

    
#if NEED_FSGS_BASE
    uint64_t base;
    get_reg(FS_BASE, (char*)&base);
    msr_write(MSR_FS_BASE,base);
    get_reg(GS_BASE, (char*)&base);
    msr_write(MSR_GS_BASE,base);
#endif
    // What is the point of ORIG_RAX, should this be RAX on exit?

    // FIXME - copy out floating point state
}


/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX (NUM_REG_BYTES*16)




// These are safe only for the "world stop" model
// where the cpu that took the exception owns them 
static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];

/* scan for the sequence $<data>#<checksum>     */

static unsigned char *getpacket(void)
{
    unsigned char *buffer = &remcomInBuffer[0];
    unsigned char checksum;
    unsigned char xmitcsum;
    int count;
    char ch;
    
    while (1)  {
        /* wait around for the start character, ignore all other characters */
        while ((ch = get_gdb()) != '$') {
        }
	
    retry:
        checksum = 0;
        xmitcsum = -1;
        count = 0;
	
        /* now, read until a # or end of buffer is found */
        while (count < BUFMAX - 1) {
            ch = get_gdb();
            if (ch == '$') {
                goto retry;
            }
            if (ch == '#') {
                break;
            }
            checksum = checksum + ch;
            buffer[count] = ch;
            count = count + 1;
        }
        buffer[count] = 0;

        if (ch == '#') {
            ch = get_gdb();
            xmitcsum = hex(ch) << 4;
            ch = get_gdb();
            xmitcsum += hex(ch);
	    
            if (checksum != xmitcsum) {

                if (remote_debug){
                    ERROR("bad checksum.  My count = 0x%x, sent=0x%x. buf=%s\n",
                          checksum, xmitcsum, buffer);
                }
                put_gdb('-');	/* failed checksum */

            } else {

                put_gdb('+');	/* successful transfer */
		
                /* if a sequence char is present, reply the sequence ID */
                if (buffer[2] == ':') {
                    put_gdb(buffer[0]);
                    put_gdb(buffer[1]);
		    
                    return &buffer[3];
                }
		
                return &buffer[0];
            }
        }
    }
}

/* send the packet in buffer.  */

static void putpacket(unsigned char *buffer)
{
    unsigned char checksum;
    int count;
    char ch;
    
    /*  $<packet info>#<checksum>.  */
    do {
	
        put_gdb('$');
        checksum = 0;
        count = 0;
	
        while ((ch = buffer[count])) {
            put_gdb(ch);
            checksum += ch;
            count += 1;
        }
	
        put_gdb('#');
        put_gdb(hexchars[checksum >> 4]);
        put_gdb(hexchars[checksum % 16]);
	
    } while (get_gdb() != '+');
}

static void debug_error(char *format, char *parm)
{
    if (remote_debug) {
        ERROR("debug error %s with parameter %s\n", format, parm);
    }
}



/* this function takes the 386 exception vector and attempts to
   translate this number into a unix compatible signal value */
static int compute_signal (int exceptionVector, void *state)
{
    int sigval;

    if (state==(void*)0x1ULL) {
        // we are being invoked from the debug device's interrupt handler
        // needs to look like a SIGINT
        return 2;
    }
    
    switch (exceptionVector) {
    case DE_EXCP:
        sigval = 8;
        break;			/* divide by zero */
    case DB_EXCP:
        sigval = 5;
        break;			/* debug exception */
	
        /* there is no NMI_INT case */
	
    case BP_EXCP:
        sigval = 5;
        break;			/* breakpoint */
    case OF_EXCP:
        sigval = 16;
        break;			/* into instruction (overflow) */
    case BR_EXCP:
        sigval = 16;
        break;			/* bound instruction */
    case UD_EXCP:
        sigval = 4;
        break;			/* Invalid opcode */
    case NM_EXCP:
        sigval = 8;
        break;			/* coprocessor not available */
    case DF_EXCP:
        sigval = 7;
        break;			/* double fault */
    case CP_EXCP:
        sigval = 11;
        break;			/* coprocessor segment overrun */
    case TS_EXCP:
        sigval = 11;
        break;			/* Invalid TSS */
    case NP_EXCP:
        sigval = 11;
        break;			/* Segment not present */
    case SS_EXCP:
        sigval = 11;
        break;			/* stack exception */
    case GP_EXCP:
        sigval = 11;
        break;			/* general protection */
    case PF_EXCP:
        sigval = 11;
        break;			/* page fault */
    case MF_EXCP:
        sigval = 7;
        break;			/* coprocessor error */
    case AC_EXCP:
        sigval = 7;             /* alignment check */
        break;
    case MC_EXCP:               /* machine check */
        sigval = 7;
        break;
    case XM_EXCP:              
        sigval = 7;             /* SIMD FPU */
        break;
    case VE_EXCP:
        sigval = 7;             /* virtualization */
        break;
    case SE_EXCP:
        sigval = 7;             /* security */
        break;
    default:
        sigval = 7;		/* "software generated" */
    }
    
    return (sigval);
}

/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD AN INT */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/
static int hex2int(char **ptr, int *intValue)
{
    int numChars = 0;
    int hexValue;
    
    *intValue = 0;
    
    while (**ptr) {
	
        hexValue = hex(**ptr);
	
        if (hexValue >= 0) {
            *intValue = (*intValue << 4) | hexValue;
            numChars++;
        } else {
            break;
        }
	
        (*ptr)++;
    }
    
    return (numChars);
}

/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD A LONG */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/
static int hex2long(char **ptr, long *longValue)
{
    int numChars = 0;
    int hexValue;
    
    *longValue = 0;

    while (**ptr) {
	
        hexValue = hex(**ptr);
      
        if (hexValue >= 0) {
            *longValue = (*longValue << 4) | hexValue;
            numChars++;
        } else {
            break;
        }
	
        (*ptr)++;
    }

    return (numChars);
}

static struct entry {
    int (*handler)(excp_entry_t * excp, excp_vec_t vec, addr_t fault_addr, void * state_addr);
    void *state;
} orig_exception_state[NUM_EXCEPTIONS];
	
    


/*
 * This function does all command procesing for interfacing to gdb.
 */
int nk_gdb_handle_exception(excp_entry_t * excp, excp_vec_t vec, addr_t fault_addr, void * state_addr)
{
    int sigval, stepping;
    uint64_t addr, length;
    uint64_t orig_rip;
    int new_rip=0;
    int  core;
    int  thread;
    int  newthread;
    int have_per_cpu = __cpu_state_get_cpu() != 0 ;

    void *cur_excp = excp;

    int switched_cpus = 0;
    
    char *in;
    char *out;
       
    char *tok;

    // everyone waits on us
    // note this is a noop until the scheduler is fully active on all cpus
    // other CPUs will be forced into the scheduler via an interrupt
    // which will not fire an entry into this handler on them
    nk_sched_stop_world();
    
    DEBUG("entry - vector %x, error_code=%lx rip=%lx:%lx fault_addr=%lx\n", 
          vec, excp->error_code, excp->cs, excp->rip, fault_addr);

    snapshot_regs(excp);

    get_reg(RIP,(char*)&orig_rip);
    
    if (remote_debug) {
        uint64_t rip;
        uint32_t eflags;
        
        get_reg(RIP,(char*)&rip);
        get_reg(EFLAGS,(char*)&eflags);
        
        DEBUG("vector=%d, rip=0x%lx, eflags=0x%x\n",
             vec, rip, eflags);
    }
    
    /* reply to host that an exception has occurred */
    sigval = compute_signal(vec,state_addr);
    
    out = remcomOutBuffer;

    *out++ = 'T';	
    out = byte_to_hex(sigval,out,0);

    out = reg_name_to_hex(RSP,out,0);
    *out++ = ':';
    out = reg_val_to_hex(RSP,out,0);
    *out++ = ';';

    out = reg_name_to_hex(RBP,out,0);
    *out++ = ':';
    out = reg_val_to_hex(RBP,out,0);
    *out++ = ';';

    out = reg_name_to_hex(RIP,out,0);
    *out++ = ':';
    out = reg_val_to_hex(RIP,out,0);
    *out++ = ';';

    // GDB thread ids are ints (decimal encoded) with the following meanings:
    // thread -1 => all
    // thread  0 => any
    // thread >0 => actual thread
    
    if (have_per_cpu) {
        core = my_cpu_id() + 1;
        thread = core ;  // are 1 based...
    } else {
        core = 1;
        thread = 1;
    }
        
    out += sprintf(out,"core:%08x;thread:%d;", core,thread);

    *out = '\0';

    DEBUG("Sending Debugger \"%s\"\n",remcomOutBuffer);

    putpacket(remcomOutBuffer);
    
    stepping = 0;

    while (1 == 1)  {

        out = remcomOutBuffer;

        *out = 0;

        in = getpacket();

        DEBUG("Received from debugger \"%s\"\n",in);

        switch (*in++) {

        case '?':
        
            DEBUG("last signal request\n");
            *out++ = 'S';
            out = byte_to_hex(sigval,out,0);
            *out++ = 0;
            break;
        
        case 'd':
        
            DEBUG("toggle remote debug request\n");
            remote_debug = !(remote_debug);	/* toggle debug flag */
            break;
	    
        case 'g':		/* return the value of the CPU registers */
	    
            DEBUG("get registers request\n");
	    
            out = mem2hex(registers, out, NUM_REG_BYTES, 0);
        
            break;
        
        case 'G':		/* set the value of the CPU registers - return OK */
        
            DEBUG("set registers request\n");
	    
            in = hex2mem(in, registers, NUM_REG_BYTES, 0);

            update_regs(cur_excp);

            strcpy(out,"OK");
        
            break;
        
        case 'P':	{	/* set the value of a single CPU register - return OK */
        
            reg_name_t regname;
        
            DEBUG("set specific register request\n");
        
            if (hex2int(&in, (int*) &regname) && *in++ == '=') {
                DEBUG("set register %d\n",regname);
            }
        
            if (regname >= MIN_REG_NAME && regname <= MAX_REG_NAME ) {
                in = hex_to_reg_val(in, regname, 0);
                update_regs(cur_excp);
                strcpy(out, "OK");
                break;
            }
	    
            // error
            strcpy(out, "E01");
        }
            break;

            /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
        
        case 'm':
        
            /* TRY TO READ %x,%x.  IF WE SUCCEED, SET IN = 0 */
            DEBUG("read memory range request\n");
        
            if (hex2long(&in, &addr)) {
            
                if (*(in++) == ',') {
                
                    if (hex2long(&in, &length)) {
                        in = 0 ; // signal to following error test
                        mem_err = 0;
                        // intentionally not advancing out here
                        // since we may have a memory error
                        mem2hex((char *) addr, out, length, 1);
                        if (mem_err) {
                            strcpy (out,"E03");
                            debug_error ("memory fault",0);
                        }
                    }
                }
            } 
        
            if (in) {
                strcpy(out, "E01");
            }
	    
            break;
        
            /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
        
        case 'M':
	    
            /* TRY TO WRITE '%x,%x:'.  IF SUCCEED, SET IN = 0 */

            DEBUG("write memory range request\n");
        
            if (hex2long(&in, &addr)) {
            
                if (*(in++) == ',') {
                
                    if (hex2long(&in, &length)) {
                    
                        if (*(in++) == ':') {
                        
                            mem_err = 0;
                            in = hex2mem(in, (char *) addr, length, 1);
                        
                            if (mem_err) {
                                strcpy(out, "E03");
                                debug_error ("memory fault",0);
                            
                            }  else  {
                                strcpy (remcomOutBuffer, "OK");
                            }
                        
                            in = 0;
                        }
                    }
                }
            }
		
            if (in) {
                strcpy (out, "E02");
            }
	    
            break;

            /* sAA..AA   Step one instruction from AA..AA(optional) */
            /* cAA..AA    Continue at address AA..AA(optional) */
        
        case 's':
            DEBUG("set single stepping request\n");
            stepping = 1;
        
            // intentional fall through here...
        
        case 'c':
            /* try to read optional parameter, pc unchanged if no parm */
        
            if (hex2long (&in, &addr)) {
            
                set_reg(RIP,(char*)&addr);
            
                new_rip = 1;
            }

            DEBUG("continue at address %lx request (stepping=%d)\n", addr, stepping);

            uint32_t eflags;
        
            get_reg(EFLAGS, (char*)&eflags);
	    
            eflags &= 0xfffffeff;
        
            /* set the trace bit if we're stepping */
            if (stepping) {
                eflags |= 0x100;
            }
        
            set_reg(EFLAGS, (char*)&eflags);

            update_regs(cur_excp);
            
            goto out;
        
            break;
        
            /* kill the program */
        case 'k':		/* do nothing */
            DEBUG("kill request - rebooting\n");
            reboot();
        
            break;

        case 'T':
            if (sscanf(in,"%d",&newthread)==1) {
                DEBUG("Is thread %d alive?\n",newthread);
                if (newthread < (nk_get_num_cpus()+1)) {
                    out+=sprintf(out,"OK");
                } else {
                    out+=sprintf(out,"E01");
                }
            }

            in+=strlen(in);
            
            break;

        case 'H': // thread related
            if (sscanf(in,"g%d",&newthread)==1) {
                in+=strlen(in);
                DEBUG("Set thread %d\n", newthread);
                if (newthread==0 || newthread==thread) {
                    // anythread -> curthread
                    DEBUG("Remaining on current thread\n");
                    out += sprintf(out,"OK");
                } else if (newthread>0) {
                    // switching "threads"
                    // we are actually switching cores, but we will use
                    // destination core's current thread (which we know is out
                    // in an interrupt handler (sched_stop_world) to get
                    // the exception state and stack of that thread
                    struct nk_thread *t = nk_sched_get_cur_thread_on_cpu(newthread-1);
                    if (!t) {
                        DEBUG("No destination thread yet - cannot switch\n");
                        out += sprintf(out,"E01");
                    } else {
                        DEBUG("current thread on cpu is %d with rsp %p\n",t->tid,t->rsp);
                        cur_excp = (excp_entry_t * )(t->rsp - EXCP_OFFSET_TO_GPRS);
                        thread = newthread;
                        snapshot_regs(cur_excp);
                        out += sprintf(out,"OK");
                    }
                } else {
                    // attempting to switch to all threads?
                    out+=sprintf(out,"E01");
                }
            } else if (sscanf(in,"c%d",&newthread)==1) {
                in+=strlen(in);
                DEBUG("Continue thread %d\n", newthread);
                // Must continue current thread or all
                if (newthread!=thread && newthread!=-1 && newthread!=0) {
                    out+=sprintf(out,"E01");
                } else {
                    out += sprintf(out,"OK");
                }
            } else {
                in+=strlen(in);
            }
                

            break;
            
        case 'q': 
            if (!strncmp(in,"Supported:",10)) {
                DEBUG("Remote GDB Features: %s\n", in+10);
                in+=strlen(in);
                // We dundonutin
                //out += sprintf(out,"QThreadEvents+");
            } else if (!strncmp(in,"fThreadInfo",11)) {
                in += 12;
                DEBUG("Thread list request\n");
                if (have_per_cpu) {
                    int i;
                    out += sprintf(out, "m1");
                    for (i=1;i<nk_get_num_cpus();i++) {
                        out +=sprintf(out,",%d",i+1);
                    }
                } else {
                    out+=sprintf(out,"m1");
                }
            } else if (!strncmp(in,"sThreadInfo",11)) {
                in += 12;
                DEBUG("Thread list request (continued)\n");
                out+=sprintf(out,"l");
            } else if (sscanf(in,"ThreadExtraInfo,%d",&newthread)==1) {
                in+=strlen(in);
                char buf[80];
                sprintf(buf, "cpu%d",newthread-1);
                out=mem2hex(buf,out,strlen(buf),0);
                DEBUG("Thread extra info request - returning %s\n",buf);
            } else if (*in == 'C') {
                in++;
                out += sprintf(out,"QC%d",thread);
                DEBUG("Get current thread id - returning %d\n",thread);
            } else {
                //unsupported
                in+=strlen(in);
            }
            
            break;
        
        default:
            DEBUG("Ignoring unknown request \"%s\"\n",in-1);
            break;
        }			/* switch */
    
        /* reply to the request */
    
        DEBUG("Replying with \"%s\"\n",remcomOutBuffer);
        putpacket(remcomOutBuffer);
    
    }
    
    
    
 out:


    switch (vec) { 
    case DB_EXCP:
        DEBUG("Continuing from a debug exception (DB_EXCP) so not invoking nested handler\n");
        nk_sched_start_world();
        return 0;
        break; 
    case BP_EXCP:
        DEBUG("Continuing from a breakpoint (int3/BP_EXCP) so not invoking nested handler\n");
        nk_sched_start_world();
        return 0;
        break; 
    default:
        if (state_addr==(void*)0x1ULL) {
            // we got here from a debug device driver - it expects us to make nice
            // and exit correctly
            DEBUG("Continuing a debug device driver interrupt so not invoking nested handler\n");
            nk_sched_start_world();
            return 0;
        } else {
            // this needs to be wrapped so as to unwind the stack
            // so it looks like a direct invocation of the next handler
            DEBUG("Invoking original handler\n");
            nk_sched_start_world();
            return orig_exception_state[vec].handler(excp, vec, fault_addr, 
                                                     orig_exception_state[vec].state);
        }
    }

}

// Should be invoked by cpu 0, and assumes 
// that all other cpus have the same installed exception handlers and state
// hooking exceptions for debugging should happen AFTER ALL EXCEPTION
// HANDLERS HAVE BEEN INSTALLED
static void hook_exceptions()
{
    int i;
    for (i=0;i<NUM_EXCEPTIONS;i++) { 
        idt_get_entry(i,
                      (ulong_t*)&orig_exception_state[i].handler,
                      (ulong_t*)&orig_exception_state[i].state);
    }

    for (i=0;i<NUM_EXCEPTIONS;i++) { 
        idt_assign_entry(i,(ulong_t)nk_gdb_handle_exception,(ulong_t)0);
    }

}



int nk_gdb_init()
{
    
    hook_exceptions();
    
    INFO("inited\n");

#ifdef NAUT_CONFIG_ATTACH_REMOTE_DEBUGGER_AT_BOOT
    INFO("attaching to debugger now\n");
    nk_gdb_breakpoint_here();
#endif
    
    return 0;
}

int nk_gdb_init_ap()
{
    // nothing for this to do at the moment
    DEBUG("inited (as AP)\n");
    return 0;
}

static int
handle_break (char * buf, void * priv)
{
    nk_gdb_breakpoint_here();
    return 0;
}


static struct shell_cmd_impl break_impl = {
    .cmd      = "break",
    .help_str = "break",
    .handler  = handle_break,
};
nk_register_shell_cmd(break_impl);
