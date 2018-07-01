#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// We don't need v6
#define LWIP_IPV6 0

// We use nautilus system, so have threads to run in OS mode
#define NO_SYS 0

// We want the embedded loopback interface
#define LWIP_HAVE_LOOPIF 1

// We want it to use NK's malloc
// not that MEM_USE_POOLS/etc is off, so all allocation goes via NK
// we're not concerned about speed yet
#define MEM_LIBC_MALLOC 1

// ?
//#define LWIP_DBG_TYPES_ON 1

// New after here

// new - do core locking
#define LWIP_TCPIP_CORE_LOCKING 1

// put the tcpip thread on a large stack regardless of
// compile-time config of NK
#define TCPIP_THREAD_STACKSIZE (2*1024*1024)
// nice big mbox - we have plenty of memory
#define TCPIP_MBOX_SIZE        128

// Make everything else nice and chunky
#define DEFAULT_THREAD_STACKSIZE (2*1024*1024)
#define DEFAULT_RAW_RECVMBOX_SIZE 128
#define DEFAULT_UDP_RECVMBOX_SIZE 128
#define DEFAULT_TCP_RECVMBOX_SIZE 128
#define DEFAULT_ACCEPTMBOX_SIZE   128
#endif
