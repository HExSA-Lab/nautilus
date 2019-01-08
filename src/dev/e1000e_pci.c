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
 * Copyright (c) 2017, Panitan Wongse-ammat
 * Copyright (c) 2017, Peter Dinda
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Panitan Wongse-ammat <Panitan.W@u.northwesttern.edu>
 *          Marc Warrior <warrior@u.northwestern.edu>
 *          Galen Lansbury <galenlansbury2017@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/netdev.h>
#include <nautilus/cpu.h>
#include <dev/pci.h>
#include <nautilus/mm.h>              // malloc, free
#include <dev/e1000e_pci.h>
#include <nautilus/irq.h>             // interrupt register
#include <nautilus/naut_string.h>     // memset, memcpy
#include <nautilus/dev.h>             // NK_DEV_REQ_*
#include <nautilus/timer.h>           // nk_sleep(ns);
#include <nautilus/cpu.h>             // udelay

#ifndef NAUT_CONFIG_DEBUG_E1000E_PCI
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define INFO(fmt, args...)     INFO_PRINT("e1000e_pci: " fmt, ##args)
#define DEBUG(fmt, args...)    DEBUG_PRINT("e1000e_pci: " fmt, ##args)
#define ERROR(fmt, args...)    ERROR_PRINT("e1000e_pci: " fmt, ##args)

#define READ_MEM(d, o)         (*((volatile uint32_t*)(((d)->mem_start)+(o))))
#define WRITE_MEM(d, o, v)     ((*((volatile uint32_t*)(((d)->mem_start)+(o))))=(v))

#define READ_MEM64(d, o)       (*((volatile uint64_t*)((d)->mem_start + (o))))
#define WRITE_MEM64(d, o, v)     ((*((volatile uint64_t*)(((d)->mem_start)+(o))))=(v))

#define RXD_RING (state->rxd_ring) // e1000e_ring *
#define RXD_PREV_HEAD (RXD_RING->head_prev)
#define RXD_TAIL (RXD_RING->tail_pos)
#define RX_PACKET_BUFFER (RXD_RING->packet_buffer) // void *, packet buff addr
#define RXD_RING_BUFFER (RXD_RING->ring_buffer) // void *, ring buff addr
#define RX_DESC(i) (((struct e1000e_rx_desc*)RXD_RING_BUFFER)[(i)])
#define RXD_ERRORS(i) (((struct e1000e_rx_desc*)RXD_RING_BUFFER)[(i)].errors)
#define RXD_STATUS(i) (((struct e1000e_rx_desc*)RXD_RING_BUFFER)[(i)].status)
#define RXD_LENGTH(i) (((struct e1000e_rx_desc*)RXD_RING_BUFFER)[(i)].length)
#define RXD_ADDR(i) (((struct e1000e_rx_desc*)RXD_RING_BUFFER)[(i)].addr)

#define RXD_COUNT (RXD_RING->count)
#define RXD_INC(a,b) (((a) + (b)) % RXD_RING->count) // modular increment rxd index

#define TXD_RING (state->tx_ring) // e1000e_ring *
#define TXD_PREV_HEAD (TXD_RING->head_prev)
#define TXD_TAIL (TXD_RING->tail_pos)
#define TX_PACKET_BUFFER (TXD_RING->packet_buffer) // void *, packet buff addr
#define TXD_RING_BUFFER (TXD_RING->ring_buffer) // void *, ring buff addr
#define TXD_CMD(i) (((struct e1000e_tx_desc*)TXD_RING_BUFFER)[(i)].cmd)
#define TXD_STATUS(i) (((struct e1000e_tx_desc*)TXD_RING_BUFFER)[(i)].status)
#define TXD_LENGTH(i) (((struct e1000e_tx_desc*)TXD_RING_BUFFER)[(i)].length)
#define TXD_ADDR(i) (((struct e1000e_tx_desc*)TXD_RING_BUFFER)[(i)].addr)

#define TXD_COUNT (TXD_RING->count)
#define TXD_INC(a,b) (((a) + (b)) % TXD_RING->count) // modular increment txd index

// a is old index, b is incr amount, c is queue size
#define TXMAP (state->tx_map)
#define RXMAP (state->rx_map)



    

// Constant variables
// These variables are configurable, and they will change the number of descriptors.
// The number of descriptors is always a multiple of eight.
// both tx_dsc_count and rx_dsc_count should be multiple of 16.

#define TX_DSC_COUNT          128     
#define TX_BLOCKSIZE          256      // bytes available per DMA block
#define RX_DSC_COUNT          128      // equal to DMA block count
#define RX_BLOCKSIZE          256      // bytes available per DMA block
#define RESTART_DELAY         5        // usec = 5 us 
#define RX_PACKET_BUFFER_SIZE 2048     // the size of the packet buffer

// After this line, PLEASE DO NOT CHANGE ANYTHING.


// transmission unit
// Data sheet from page 36
// 16384 bytes is from the maximum packet buffer size that e1000 can receive.
// 16288 bytes is the maximum packet size that e1000 can send in theory.
// Ethernet standard MTU is 1500 bytes.
#define MAX_TU                    16384    // maximum transmission unit 
#define MIN_TU                    48       // minimum transmission unit 
#define MAC_LEN                   6        // the length of the mac address

// PCI CONFIG SPACE ************************************
#define INTEL_VENDOR_ID               0x8086
#define E1000E_DEVICE_ID              0x10D3
#define E1000E_PCI_CMD_OFFSET         0x4    // Device Control - RW
#define E1000E_PCI_STATUS_OFFSET      0x6    // Device Status - RO

// PCI command register
#define E1000E_PCI_CMD_IO_ACCESS_EN   1       // io access enable
#define E1000E_PCI_CMD_MEM_ACCESS_EN  (1<<1)  // memory access enable
#define E1000E_PCI_CMD_LANRW_EN       (1<<2)  // enable mastering lan r/w
#define E1000E_PCI_CMD_INT_DISABLE    (1<<10) // legacy interrupt disable when set

// PCI status register
#define E1000E_PCI_STATUS_INT         (1<<3)

// REGISTER OFFSETS ************************************
#define E1000E_CTRL_OFFSET    0x00000
#define E1000E_STATUS_OFFSET  0x00008
#define E1000E_GCR_OFFSET     0x05B00
// flow control address
#define E1000E_FCAL_OFFSET    0x00028
#define E1000E_FCAH_OFFSET    0x0002C
#define E1000E_FCT_OFFSET     0x00030

// transmit
#define E1000E_TDBAL_OFFSET   0x3800   // transmit descriptor base address low check
#define E1000E_TDBAH_OFFSET   0x3804   // transmit descriptor base address high check
#define E1000E_TDLEN_OFFSET   0x3808   // transmit descriptor list length check
#define E1000E_TDH_OFFSET     0x3810   // transmit descriptor head check
#define E1000E_TDT_OFFSET     0x3818   // transmit descriptor tail check
#define E1000E_TCTL_OFFSET    0x0400   // transmit control check check
#define E1000E_TIPG_OFFSET    0x0410   // transmit interpacket gap check
#define E1000E_TXDCTL_OFFSET  0x03828  // transmit descriptor control r/w check

// receive
#define E1000E_RAL_OFFSET     0x5400   // receive address low check
#define E1000E_RAH_OFFSET     0x5404   // receive address high check
#define E1000E_RDBAL_OFFSET   0x2800   // rx descriptor base address low check
#define E1000E_RDBAH_OFFSET   0x2804   // rx descriptor base address high check
#define E1000E_RDLEN_OFFSET   0x2808   // receive descriptor list length check
#define E1000E_RDH_OFFSET     0x2810   // receive descriptor head check
#define E1000E_RDT_OFFSET     0x2818   // receive descriptor tail check
#define E1000E_RCTL_OFFSET    0x0100   // receive control check
#define E1000E_RDTR_OFFSET_NEW 0x2820
#define E1000E_RDTR_OFFSET_ALIAS 0x0108 // receive delay timer check
#define E1000E_RSRPD_OFFSET   0x02C00  // rx small packet detect interrupt r/w
#define E1000E_RXDCTL_OFFSET  0x2828   // receive descriptor control
#define E1000E_RADV_OFFSET    0x282C   // receive interrupt absolute delay timer

// statistics error
#define E1000E_CRCERRS_OFFSET 0x04000  // crc error count
//// missed pkt count (insufficient receive fifo space)
#define E1000E_MPC_OFFSET     0x04010

#define E1000E_TPT_OFFSET     0x40D4   // total package transmit
#define E1000E_TPR_OFFSET     0x40D0   // total package receive
#define E1000E_TORL_OFFSET    0x040C0  // total octet receive low
#define E1000E_TORH_OFFSET    0x040C8  // total octet receive high
#define E1000E_TOTL_OFFSET    0x040C8  // total octet transmit low 
#define E1000E_TOTH_OFFSET    0x040CC  // total octet transmit high

#define E1000E_RXERRC_OFFSET  0x0400C  // rx error count
#define E1000E_RNBC_OFFSET    0x040A0  // receive no buffer count
#define E1000E_GPRC_OFFSET    0x04074  // good packets receive count
#define E1000E_GPTC_OFFSET    0x04080  // good packets transmit count
#define E1000E_COLC_OFFSET    0x04028  // collision count
#define E1000E_RLEC_OFFSET    0x04040  // receive length error
#define E1000E_RUC_OFFSET     0x040A4  // receive under size count
#define E1000E_ROC_OFFSET     0x040AC  // receive over size count

// 4 interrupt register offset
#define E1000E_ICR_OFFSET     0x000C0  /* interrupt cause read register check*/
#define E1000E_ICS_OFFSET     0x000C8  /* interrupt cause set register */
#define E1000E_IMS_OFFSET     0x000D0  /* interrupt mask set/read register */
#define E1000E_IMC_OFFSET     0x000D8  /* interrupt mask clear */
#define E1000E_TIDV_OFFSET    0x03820  /* transmit interrupt delay value r/w */

#define E1000E_AIT_OFFSET     0x00458  /* Adaptive IFS Throttle r/w */
#define E1000E_TADV_OFFSET    0x0382C  /* transmit absolute interrupt delay value */ 

// REGISTER BIT MASKS **********************************
#define E1000E_GCR_B22               (1<<22)
// Ctrl
#define E1000E_CTRL_FD               1           // full deplex
#define E1000E_CTRL_SLU              (1<<6)      // set link up
#define E1000E_CTRL_ILOS             (1<<7)      // loss of signal polarity
#define E1000E_CTRL_SPEED_10M        (0<<8)      // speed selection 10M
#define E1000E_CTRL_SPEED_100M       (1<<8)      // speed selection 100M
#define E1000E_CTRL_SPEED_1GV1       (2<<8)      // speed selection 1g v1
#define E1000E_CTRL_SPEED_1GV2       (3<<8)      // speed selection 1g v2
#define E1000E_CTRL_SPEED_MASK       (3<<8)      // bit mask for speed in ctrl reg
#define E1000E_CTRL_SPEED_SHIFT      8           // bit shift for speed in ctrl reg
#define E1000E_CTRL_FRCSPD           (1<<11)     // force speed
#define E1000E_CTRL_FRCDPLX          (1<<12)     // force duplex
#define E1000E_CTRL_RST              (1<<26)     // reset
#define E1000E_CTRL_RFCE             (1<<27)     // receive flow control enable
#define E1000E_CTRL_TFCE             (1<<28)     // transmit control flow enable

// Status
#define E1000E_STATUS_FD             1           // full, half duplex = 1, 0
#define E1000E_STATUS_LU             (1<<1)      // link up established = 1
#define E1000E_STATUS_SPEED_10M      (0<<6)
#define E1000E_STATUS_SPEED_100M     (1<<6)
#define E1000E_STATUS_SPEED_1G       (2<<6)
#define E1000E_STATUS_SPEED_MASK     (3<<6)
#define E1000E_STATUS_SPEED_SHIFT    6           // bit shift for speed in status reg
#define E1000E_STATUS_ASDV_MASK      (3<<8)      // auto speed detect value
#define E1000E_STATUS_ASDV_SHIFT     8           // bit shift for auto speed detect value 

#define E1000E_STATUS_PHYRA          (1<<10)     // PHY required initialization

// E1000E Transmit Control Register
#define E1000E_TCTL_EN               (1 << 1)    // transmit enable
#define E1000E_TCTL_PSP              (1 << 3)    // pad short packet
#define E1000E_TCTL_CT               (0x0f << 4) // collision threshold
#define E1000E_TCTL_CT_SHIFT         4
#define E1000E_TCTL_COLD_SHIFT       12
// collision distance full duplex 0x03f, 
#define E1000E_TCTL_COLD_FD          (0x03f << 12)
// collision distance half duplex 0x1ff 
#define E1000E_TCTL_COLD_HD          (0x1ff << 12)
#define E1000E_TCTL_SWXOFF           22
#define E1000E_TCTL_PBE              23
#define E1000E_TCTL_RTLC             24
#define E1000E_TCTL_UNORTX           25
#define E1000E_TCTL_TXDSCMT_SHIFT    26
#define E1000E_TCTL_MULR             28
#define E1000E_TCTL_RRTHRESH         29

// E1000E Transmit Descriptor Control
//// granularity thresholds unit 0b cache line, 1b descriptors
#define E1000E_TXDCTL_GRAN           (1<<24)
// CLEANUP
#define E1000E_TXDCTL_WTHRESH        0 //(1<<16)

// IPG = inter packet gap
#define E1000E_TIPG_IPGT             0x08          // IPG transmit time
#define E1000E_TIPG_IPGR1_SHIFT      10
#define E1000E_TIPG_IPGR1            (0x2 << 10)   // TIPG1 = 2
#define E1000E_TIPG_IPGR2_SHIFT      20
#define E1000E_TIPG_IPGR2            (0xa << 20)   // TIPG2 = 10

// E1000E Receive Control Register 
#define E1000E_RCTL_EN               (1 << 1)    // Receiver Enable
#define E1000E_RCTL_SBP              (1 << 2)    // Store Bad Packets
#define E1000E_RCTL_UPE              (1 << 3)    // Unicast Promiscuous Enabled
#define E1000E_RCTL_MPE              (1 << 4)    // Multicast Promiscuous Enabled
#define E1000E_RCTL_LPE              (1 << 5)    // Long Packet Reception Enable
#define E1000E_RCTL_LBM_NONE         (0 << 6)    // No Loopback
#define E1000E_RCTL_LBM_PHY          (3 << 6)    // PHY or external SerDesc loopback
#define E1000E_RCTL_RDMTS_SHIFT      8           // left shift count 
#define E1000E_RCTL_RDMTS_MASK       (3 << 8)    // mask rdmts
#define E1000E_RCTL_RDMTS_HALF       (0 << 8)    // Free Buffer Threshold is 1/2 of RDLEN
#define E1000E_RCTL_RDMTS_QUARTER    (1 << 8)    // Free Buffer Threshold is 1/4 of RDLEN
#define E1000E_RCTL_RDMTS_EIGHTH     (2 << 8)    // Free Buffer Threshold is 1/8 of RDLEN
#define E1000E_RCTL_DTYP_LEGACY      (0 << 10)   // legacy descriptor type
#define E1000E_RCTL_DTYP_PACKET_SPLIT (1 << 10)  // packet split descriptor type
#define E1000E_RCTL_MO_36            (0 << 12)   // Multicast Offset - bits 47:36
#define E1000E_RCTL_MO_35            (1 << 12)   // Multicast Offset - bits 46:35
#define E1000E_RCTL_MO_34            (2 << 12)   // Multicast Offset - bits 45:34
#define E1000E_RCTL_MO_32            (3 << 12)   // Multicast Offset - bits 43:32
#define E1000E_RCTL_BAM              (1 << 15)   // Broadcast Accept Mode
#define E1000E_RCTL_VFE              (1 << 18)   // VLAN Filter Enable
#define E1000E_RCTL_CFIEN            (1 << 19)   // Canonical Form Indicator Enable
#define E1000E_RCTL_CFI              (1 << 20)   // Canonical Form Indicator Bit Value
#define E1000E_RCTL_DPF              (1 << 22)   // Discard Pause Frames
#define E1000E_RCTL_PMCF             (1 << 23)   // Pass MAC Control Frames
#define E1000E_RCTL_SECRC            (1 << 26)   // Strip Ethernet CRC

// Buffer Sizes
// RCTL.BSEX = 0
#define E1000E_RCTL_BSIZE_256        (3 << 16)
#define E1000E_RCTL_BSIZE_512        (2 << 16)
#define E1000E_RCTL_BSIZE_1024       (1 << 16)
#define E1000E_RCTL_BSIZE_2048       (0 << 16)
// RCTL.BSEX = 1
#define E1000E_RCTL_BSIZE_4096       ((3 << 16) | (1 << 25))
#define E1000E_RCTL_BSIZE_8192       ((2 << 16) | (1 << 25))
#define E1000E_RCTL_BSIZE_16384      ((1 << 16) | (1 << 25))
#define E1000E_RCTL_BSIZE_MASK       ~((3 << 16) | (1 << 25))

// RXDCTL
#define E1000E_RXDCTL_GRAN           (1 << 24)   // Granularity
#define E1000E_RXDCTL_WTHRESH        (1 << 16)   // number of written-back rxd
#define E1000E_RXDCTL_PTHRESH        (1 << 0)    // number of prefetching rxd
#define E1000E_RXDCTL_HTHRESH        (1 << 8)    // number of available host rxd

#define E1000E_RECV_BSIZE_256        256
#define E1000E_RECV_BSIZE_512        512
#define E1000E_RECV_BSIZE_1024       1024
#define E1000E_RECV_BSIZE_2048       2048
#define E1000E_RECV_BSIZE_4096       4096
#define E1000E_RECV_BSIZE_8192       8192
#define E1000E_RECV_BSIZE_16384      16384
#define E1000E_RECV_BSIZE_MAX        16384
#define E1000E_RECV_BSIZE_MIN        E1000E_RECV_BSIZE_256

// interrupt bits of icr register
#define E1000E_ICR_TXDW              1          // transmit descriptor written back 
#define E1000E_ICR_TXQE              (1 << 1)   // transmit queue empty 
#define E1000E_ICR_LSC               (1 << 2)   // link state change 
#define E1000E_ICR_RXO               (1 << 6)   // receive overrun 
#define E1000E_ICR_RXT0              (1 << 7)   // receiver timer interrupt 
#define E1000E_ICR_TXD_LOW           (1 << 15)  // transmit descriptor low threshold hit 
#define E1000E_ICR_SRPD              (1 << 16)  // small receive packet detected
#define E1000E_ICR_RXQ0              (1 << 20)  // receive queue 0 interrupt
#define E1000E_ICR_RXQ1              (1 << 21)  // receive queue 1 interrupt
#define E1000E_ICR_TXQ0              (1 << 22)  // transmit queue 0 interrupt
#define E1000E_ICR_TXQ1              (1 << 23)  // transmit queue 1 interrupt
#define E1000E_ICR_OTHER             (1 << 24)  // other interrupts
#define E1000E_ICR_INT_ASSERTED      (1 << 31)  // interrupt asserted
#define E1000E_RDTR_FPD              (1 << 31)  // flush partial descriptor block

#define E1000E_RSPD_MASK             (0xfff)  // 

// the same encoding for status.speed, status.asdv, and ctrl.speed
#define E1000E_SPEED_ENCODING_10M     0
#define E1000E_SPEED_ENCODING_100M    1
#define E1000E_SPEED_ENCODING_1G_V1   2
#define E1000E_SPEED_ENCODING_1G_V2   3

// transmit descriptor
//// command 
#define E1000E_TXD_CMD_EOP       1
#define E1000E_TXD_CMD_IFCS      0x2
#define E1000E_TXD_CMD_IC        0x4
#define E1000E_TXD_CMD_RS        0x8
#define E1000E_TXD_CMD_RSV       0x10
#define E1000E_TXD_CMD_DEXT      0x20
#define E1000E_TXD_CMD_VLE       0x40
#define E1000E_TXD_CMD_IDE       0x80


struct e1000e_desc_ring {
  void *ring_buffer;
  uint32_t head_prev;
  uint32_t tail_pos;
  uint32_t count;
  void *packet_buffer;
  uint32_t blocksize;
};

struct e1000e_rx_desc {
  uint64_t* addr;
  uint16_t length;
  uint16_t checksum;
  struct {
    uint8_t dd    : 1;
    uint8_t eop   : 1;
    uint8_t ixsm  : 1;
    uint8_t vp    : 1;
    uint8_t rsv   : 1;
    uint8_t tcpcs : 1;
    uint8_t ipcs  : 1;
    uint8_t pif   : 1;
  } status;
  uint8_t errors;
  uint16_t special;
} __attribute__((packed));

// legacy mode
struct e1000e_tx_desc {
  uint64_t* addr;
  uint16_t length;
  uint8_t  cso;
  union {
    struct {
      uint8_t eop : 1;
      uint8_t ifcs: 1;
      uint8_t ic  : 1;
      uint8_t rs  : 1;
      uint8_t rsvd: 1;
      uint8_t dext: 1;
      uint8_t vle : 1;
      uint8_t ide : 1;
    } bit;
    uint8_t byte;
  } cmd;
  struct {
    uint8_t dd    : 1;
    uint8_t ec    : 1;
    uint8_t lc    : 1;
    uint8_t rsvtu : 1;
    uint8_t rsvd2 : 4;
  } status;
  uint8_t css;
  uint16_t special;
} __attribute__((packed)); 

// TODO change the type of context to void
struct e1000e_fn_map {
  void (*callback)(nk_net_dev_status_t, void *);
  uint64_t *context;
};

struct e1000e_map_ring {
  struct e1000e_fn_map *map_ring;
  // head and tail positions of the fn_map ring queue
  uint64_t head_pos;
  uint64_t tail_pos;
  // the number of elements in the map ring buffer
  uint64_t ring_len;
};


// Timing
struct tsc {
  uint64_t start;
  uint64_t end;
};
typedef struct tsc tsc_t;

struct operation {
  // should be a postx, but I did last week.
  tsc_t postx_map; // measure the time in post_tx, post_rx
  tsc_t xpkt; // send_packet, receive_packet 
  tsc_t irq; // irq_handler
  tsc_t irq_unmap; // unmap callback in the irq function
  tsc_t irq_callback; // invoke the callback function in the irq
  
  tsc_t irq_macro; // IRQ_HANDLER_END() macro
  tsc_t irq_reg; // reading a value from the reg in the irq function
  tsc_t dev_wait;
  uint32_t dev_wait_count;
};
typedef struct operation op_t;

struct iteration {
  op_t tx;
  op_t rx;
  uint32_t irq_tx;
  uint32_t irq_rx;
  uint32_t irq_unknown; // count the unknown irq
};
typedef struct iteration iteration_t;

// add timing code
#define TIMING 0

#if TIMING
#define TIMING_GET_TSC(x)                       ((x) = rdtsc())
#define TIMING_DIFF_TSC(r,s,e)                  ((r)=(e)-(s))
#else
#define TIMING_GET_TSC(x)                   
#define TIMING_DIFF_TSC(r,s,e)              
#endif

struct e1000e_state {
  // a pointer to the base class
  struct nk_net_dev *netdev;
  // pci interrupt and interupt vector
  struct pci_dev *pci_dev;

  // our device list
  struct list_head node;

    // Where registers are mapped into the I/O address space
  uint16_t  ioport_start;
  uint16_t  ioport_end;  
  // Where registers are mapped into the physical memory address space
  uint64_t  mem_start;
  uint64_t  mem_end;
    
  char name[DEV_NAME_LEN];
  uint8_t mac_addr[6];

  struct e1000e_desc_ring *tx_ring;
  struct e1000e_desc_ring *rxd_ring;
  // a circular queue mapping between callback function and tx descriptor
  struct e1000e_map_ring *tx_map;
  // a circular queue mapping between callback funtion and rx descriptor
  struct e1000e_map_ring *rx_map;
  // the size of receive buffers
  uint64_t rx_buffer_size;
  // interrupt mark set
  uint32_t ims_reg;

#if TIMING
  volatile iteration_t measure;
#endif
};

// list of discovered devices
static struct list_head dev_list;


// initialize the tx ring buffer to store transmit descriptors
static int e1000e_init_transmit_ring(struct e1000e_state *state)
{
  TXMAP = malloc(sizeof(struct e1000e_map_ring));
  if (!TXMAP) {
    ERROR("Cannot allocate txmap\n");
    return -1;
  }
  memset(TXMAP, 0, sizeof(struct e1000e_map_ring));
  TXMAP->ring_len = TX_DSC_COUNT;

  TXMAP->map_ring = malloc(sizeof(struct e1000e_fn_map) * TX_DSC_COUNT);
  if (!TXMAP->map_ring) {
    ERROR("Cannot allocate txmap->ring\n");
    return -1;
  }
  memset(TXMAP->map_ring, 0, sizeof(struct e1000e_fn_map) * TX_DSC_COUNT);

  TXD_RING = malloc(sizeof(struct e1000e_desc_ring));
  if (!TXD_RING) {
    ERROR("Cannot allocate TXD_RING\n");
    return -1;
  }
  memset(TXD_RING, 0, sizeof(struct e1000e_desc_ring));

  // allocate TX_DESC_COUNT transmit descriptors in the ring buffer.
  TXD_RING_BUFFER = malloc(sizeof(struct e1000e_tx_desc)*TX_DSC_COUNT);
  if (!TXD_RING_BUFFER) {
    ERROR("Cannot allocate TXD_RING_BUFFER\n");
    return -1;
  }
  memset(TXD_RING_BUFFER, 0, sizeof(struct e1000e_tx_desc)*TX_DSC_COUNT);
  TXD_COUNT = TX_DSC_COUNT;

  // store the address of the memory in TDBAL/TDBAH
  WRITE_MEM(state, E1000E_TDBAL_OFFSET,
            (uint32_t)( 0x00000000ffffffff & (uint64_t) TXD_RING_BUFFER));
  WRITE_MEM(state, E1000E_TDBAH_OFFSET,
            (uint32_t)((0xffffffff00000000 & (uint64_t) TXD_RING_BUFFER) >> 32));
  DEBUG("TXD_RING_BUFFER=0x%p, TDBAH=0x%08x, TDBAL=0x%08x\n",
        TXD_RING_BUFFER, 
        READ_MEM(state, E1000E_TDBAH_OFFSET),
        READ_MEM(state, E1000E_TDBAL_OFFSET));

  // write tdlen: transmit descriptor length
  WRITE_MEM(state, E1000E_TDLEN_OFFSET,
            sizeof(struct e1000e_tx_desc) * TX_DSC_COUNT);

  // write the tdh, tdt with 0
  WRITE_MEM(state, E1000E_TDT_OFFSET, 0);
  WRITE_MEM(state, E1000E_TDH_OFFSET, 0);
  DEBUG("init tx fn: TDLEN = 0x%08x, TDH = 0x%08x, TDT = 0x%08x\n",
        READ_MEM(state, E1000E_TDLEN_OFFSET),
        READ_MEM(state, E1000E_TDH_OFFSET),
        READ_MEM(state, E1000E_TDT_OFFSET));

  TXD_PREV_HEAD = 0;
  TXD_TAIL = 0;
  
  // TCTL Reg: EN = 1b, PSP = 1b, CT = 0x0f (16d), COLD = 0x3F (63d)
  uint32_t tctl_reg = E1000E_TCTL_EN | E1000E_TCTL_PSP | E1000E_TCTL_CT | E1000E_TCTL_COLD_FD;
  WRITE_MEM(state, E1000E_TCTL_OFFSET, tctl_reg);
  DEBUG("init tx fn: TCTL = 0x%08x expects 0x%08x\n",
        READ_MEM(state, E1000E_TCTL_OFFSET), 
        tctl_reg);

  // TXDCTL Reg: set WTHRESH = 1b, GRAN = 1b,
  // other fields = 0b except bit 22th = 1b
  WRITE_MEM(state, E1000E_TXDCTL_OFFSET,
            E1000E_TXDCTL_GRAN | E1000E_TXDCTL_WTHRESH | (1<<22));
  // write tipg register
  // 00,00 0000 0110,0000 0010 00,00 0000 1010 = 0x0060200a
  // will be zero when emulating hardware
  WRITE_MEM(state, E1000E_TIPG_OFFSET,
            E1000E_TIPG_IPGT | E1000E_TIPG_IPGR1 | E1000E_TIPG_IPGR2);
  DEBUG("init tx fn: TIPG = 0x%08x expects 0x%08x\n",
        READ_MEM(state, E1000E_TIPG_OFFSET),
        E1000E_TIPG_IPGT | E1000E_TIPG_IPGR1 | E1000E_TIPG_IPGR2);
  return 0;
}

static int e1000e_set_receive_buffer_size(struct e1000e_state* state, uint32_t buffer_size)
{
  if (state->rx_buffer_size != buffer_size) {
    uint32_t rctl = READ_MEM(state, E1000E_RCTL_OFFSET) & E1000E_RCTL_BSIZE_MASK;
    switch(buffer_size) {
      case E1000E_RECV_BSIZE_256:
        rctl |= E1000E_RCTL_BSIZE_256;
        break;
      case E1000E_RECV_BSIZE_512:
        rctl |= E1000E_RCTL_BSIZE_512;
        break;
      case E1000E_RECV_BSIZE_1024:
        rctl |= E1000E_RCTL_BSIZE_1024;
        break;
      case E1000E_RECV_BSIZE_2048:
        rctl |= E1000E_RCTL_BSIZE_2048;
        break;
      case E1000E_RECV_BSIZE_4096:
        rctl |= E1000E_RCTL_BSIZE_4096;
        break;
      case E1000E_RECV_BSIZE_8192:
        rctl |= E1000E_RCTL_BSIZE_8192;
        break;
      case E1000E_RECV_BSIZE_16384:
        rctl |= E1000E_RCTL_BSIZE_16384;
        break;
      default:
        ERROR("e1000e receive pkt fn: unmatch buffer size\n");
        return -1;
    }
    WRITE_MEM(state, E1000E_RCTL_OFFSET, rctl);
    state->rx_buffer_size = buffer_size;
  }
  return 0;
}

// initialize the rx ring buffer to store receive descriptors
static int e1000e_init_receive_ring(struct e1000e_state *state)
{
  RXMAP = malloc(sizeof(struct e1000e_map_ring));
  if (!RXMAP) {
    ERROR("Cannot allocate rxmap\n");
    return -1;
  }
  memset(RXMAP, 0, sizeof(struct e1000e_map_ring));

  RXMAP->map_ring = malloc(sizeof(struct e1000e_fn_map) * RX_DSC_COUNT);
  if (!RXMAP->map_ring) {
    ERROR("Cannot allocate rxmap->ring\n");
    return -1;
  }
  memset(RXMAP->map_ring, 0, sizeof(struct e1000e_fn_map) * RX_DSC_COUNT);
  RXMAP->ring_len = RX_DSC_COUNT;

  RXD_RING = malloc(sizeof(struct e1000e_desc_ring));
  if (!RXD_RING) {
    ERROR("Cannot allocate rxd_ring buffer\n");
    return -1;
  }
  memset(RXD_RING, 0, sizeof(struct e1000e_desc_ring));

  // the number of the receive descriptor in the ring
  RXD_COUNT = RX_DSC_COUNT;

  // allocate the receive descriptor ring buffer
  uint32_t rx_desc_size = sizeof(struct e1000e_rx_desc) * RX_DSC_COUNT;
  RXD_RING_BUFFER = malloc(rx_desc_size);
  if (!RXD_RING_BUFFER) {
    ERROR("Cannot allocate RXD_RING_BUFFER\n");
    return -1;
  }
  memset(RXD_RING_BUFFER, 0, rx_desc_size);

  // store the address of the memory in TDBAL/TDBAH
  WRITE_MEM(state, E1000E_RDBAL_OFFSET,
            (uint32_t)(0x00000000ffffffff & (uint64_t) RXD_RING_BUFFER));
  WRITE_MEM(state, E1000E_RDBAH_OFFSET,
            (uint32_t)((0xffffffff00000000 & (uint64_t) RXD_RING_BUFFER) >> 32));
  DEBUG("init rx fn: RDBAH = 0x%08x, RDBAL = 0x%08x = rd_buffer\n",
        READ_MEM(state, E1000E_RDBAH_OFFSET),
        READ_MEM(state, E1000E_RDBAL_OFFSET));
  DEBUG("init rx fn: rd_buffer = 0x%016lx\n", RXD_RING_BUFFER);

  // write rdlen
  WRITE_MEM(state, E1000E_RDLEN_OFFSET, rx_desc_size);
  DEBUG("init rx fn: RDLEN=0x%08x should be 0x%08x\n",
        READ_MEM(state, E1000E_RDLEN_OFFSET), rx_desc_size);

  // write the rdh, rdt with 0
  WRITE_MEM(state, E1000E_RDH_OFFSET, 0);
  WRITE_MEM(state, E1000E_RDT_OFFSET, 0);
  DEBUG("init rx fn: RDH=0x%08x, RDT=0x%08x expects 0\n",
        READ_MEM(state, E1000E_RDH_OFFSET),
        READ_MEM(state, E1000E_RDT_OFFSET));
  RXD_PREV_HEAD = 0;
  RXD_TAIL = 0;

  WRITE_MEM(state, E1000E_RXDCTL_OFFSET,
            E1000E_RXDCTL_GRAN | E1000E_RXDCTL_WTHRESH);
  DEBUG("init rx fn: RXDCTL=0x%08x expects 0x%08x\n",
        READ_MEM(state, E1000E_RXDCTL_OFFSET),
        E1000E_RXDCTL_GRAN | E1000E_RXDCTL_WTHRESH);

  // write rctl register specifing the receive mode
  uint32_t rctl_reg = E1000E_RCTL_EN | E1000E_RCTL_SBP | E1000E_RCTL_UPE | E1000E_RCTL_LPE | E1000E_RCTL_DTYP_LEGACY | E1000E_RCTL_BAM | E1000E_RCTL_RDMTS_HALF | E1000E_RCTL_PMCF;
   
  // receive buffer threshold and size
  WRITE_MEM(state, E1000E_RCTL_OFFSET, rctl_reg);

  // set the receive packet buffer size
  e1000e_set_receive_buffer_size(state, RX_PACKET_BUFFER_SIZE);
  
  DEBUG("init rx fn: RCTL=0x%08x expects  0x%08x\n",
        READ_MEM(state, E1000E_RCTL_OFFSET),
        rctl_reg);
  return 0;
}

static int e1000e_send_packet(uint8_t* packet_addr,
                              uint64_t packet_size,
                              struct e1000e_state *state)
{
  DEBUG("send pkt fn: pkt_addr 0x%p pkt_size: %d\n", packet_addr, packet_size);

  DEBUG("send pkt fn: before sending TDH = %d TDT = %d tail_pos = %d\n",
        READ_MEM(state, E1000E_TDH_OFFSET),
        READ_MEM(state, E1000E_TDT_OFFSET),
        TXD_TAIL);
  DEBUG("send pkt fn: tpt total packet transmit: %d\n",
        READ_MEM(state, E1000E_TPT_OFFSET));

  if (packet_size > MAX_TU) {
    ERROR("send pkt fn: packet is too large.\n");
    return -1;
  }

  memset(((struct e1000e_tx_desc *)TXD_RING_BUFFER + TXD_TAIL),
         0, sizeof(struct e1000e_tx_desc));
  TXD_ADDR(TXD_TAIL) = (uint64_t*) packet_addr;
  TXD_LENGTH(TXD_TAIL) = packet_size;
  // set up send flags
  // TXD_CMD(TXD_TAIL).bit.dext = 0;
  // TXD_CMD(TXD_TAIL).bit.vle = 0;
  // TXD_CMD(TXD_TAIL).bit.ifcs = 1;
  // // set the end of packet flag if this is the last fragment
  // TXD_CMD(TXD_TAIL).bit.eop = 1;
  // // interrupt delay enable
  // // if ide = 0 and rs = 1, the transmit interrupt will occur immediately
  // // after the packet is sent.
  // TXD_CMD(TXD_TAIL).bit.ide = 0;
  // // report the status of the descriptor
  // TXD_CMD(TXD_TAIL).bit.rs = 1;
  TXD_CMD(TXD_TAIL).byte = E1000E_TXD_CMD_EOP | E1000E_TXD_CMD_IFCS | E1000E_TXD_CMD_RS; 

  // increment transmit descriptor list tail by 1
  DEBUG("send pkt fn: moving the tail\n");
  TXD_TAIL = TXD_INC(1, TXD_TAIL);
  WRITE_MEM(state, E1000E_TDT_OFFSET, TXD_TAIL);
  DEBUG("send pkt fn: after moving tail TDH = %d TDT = %d tail_pos = %d\n",
        READ_MEM(state, E1000E_TDH_OFFSET),
        READ_MEM(state, E1000E_TDT_OFFSET),
        TXD_TAIL);

  DEBUG("send pkt fn: transmit error %d\n",
        TXD_STATUS(TXD_PREV_HEAD).ec | TXD_STATUS(TXD_PREV_HEAD).lc);

  DEBUG("send pkt fn: txd cmd.rs = %d status.dd = %d\n",
        TXD_CMD(TXD_PREV_HEAD).bit.rs, 
        TXD_STATUS(TXD_PREV_HEAD).dd);

  DEBUG("send pkt fn: tpt total packet transmit: %d\n",
        READ_MEM(state, E1000E_TPT_OFFSET));

  // uint32_t status_pci = pci_cfg_readw(state->bus_num, state->pci_dev->num,
  //                                     0, E1000E_PCI_STATUS_OFFSET);
  // DEBUG("send pkt fn: status_pci 0x%04x int %d\n",
  //       status_pci, status_pci & E1000E_PCI_STATUS_INT);
  return 0;
}

static void e1000e_disable_receive(struct e1000e_state* state)
{
  uint32_t rctl_reg = READ_MEM(state, E1000E_RCTL_OFFSET);
  rctl_reg &= ~E1000E_RCTL_EN;
  WRITE_MEM(state, E1000E_RCTL_OFFSET, rctl_reg); 
  return;
}

static void e1000e_enable_receive(struct e1000e_state* state)
{
  uint32_t rctl_reg = READ_MEM(state, E1000E_RCTL_OFFSET);
  rctl_reg |= E1000E_RCTL_EN;
  WRITE_MEM(state, E1000E_RCTL_OFFSET, rctl_reg);
  return;
}

static int e1000e_receive_packet(uint8_t* buffer,
                                 uint64_t buffer_size,
                                 struct e1000e_state *state)
{
  DEBUG("e1000e receive packet fn: buffer = 0x%p, len = %lu\n",
        buffer, buffer_size);
  DEBUG("e1000e receive pkt fn: before moving tail head: %d, tail: %d\n",
        READ_MEM(state, E1000E_RDH_OFFSET), 
        READ_MEM(state, E1000E_RDT_OFFSET)); 

  memset(((struct e1000e_rx_desc *) RXD_RING_BUFFER + RXD_TAIL),
         0, sizeof(struct e1000e_rx_desc));
  
  RXD_ADDR(RXD_TAIL) = (uint64_t*) buffer;

  RXD_TAIL = RXD_INC(RXD_TAIL, 1);
  WRITE_MEM(state, E1000E_RDT_OFFSET, RXD_TAIL);

  DEBUG("e1000e receive pkt fn: after moving tail head: %d, prev_head: %d tail: %d\n",
        READ_MEM(state, E1000E_RDH_OFFSET), 
        RXD_PREV_HEAD, 
        READ_MEM(state, E1000E_RDT_OFFSET)); 

  return 0;
}

static uint64_t e1000e_packet_size_to_buffer_size(uint64_t sz)
{
  if (sz < E1000E_RECV_BSIZE_MIN) {
    // set the minimum packet size to 256 bytes
    return E1000E_RECV_BSIZE_MIN;
  }
  // Round up the number to the buffer size with power of two
  // In E1000E, the packet buffer is the power of two.
  // citation: https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
  sz--;
  sz |= sz >> 1;
  sz |= sz >> 2;
  sz |= sz >> 4;
  sz |= sz >> 8;
  sz |= sz >> 16;
  sz |= sz >> 32;
  sz++;
  // if the size is larger than the maximun buffer size, return the largest size.
  if (sz >= E1000E_RECV_BSIZE_MAX) {
    return E1000E_RECV_BSIZE_MAX;
  } else {
    return sz;
  }
}

static int e1000e_get_characteristics(void *vstate, struct nk_net_dev_characteristics *c)
{
  if (!vstate) {
    ERROR("The device state pointer is null\n");
    return -1;
  }

  if (!c) {
    ERROR("The characteristics pointer is null\n");
    return -1;
  }

  struct e1000e_state *state=(struct e1000e_state*)vstate;
  memcpy(c->mac, (void *) state->mac_addr, MAC_LEN);
  // minimum and the maximum transmission unit
  c->min_tu = MIN_TU;
  c->max_tu = MAX_TU;
  c->packet_size_to_buffer_size = e1000e_packet_size_to_buffer_size;
  return 0;
}


static int e1000e_unmap_callback(struct e1000e_map_ring* map,
                                 uint64_t** callback,
                                 void** context)
{
  // callback is a function pointer
  DEBUG("unmap callback fn head_pos %d tail_pos %d\n",
        map->head_pos, map->tail_pos);

  if (map->head_pos == map->tail_pos) {
    // if there is an empty mapping ring buffer, do not unmap the callback
    ERROR("Try to unmap an empty queue\n");
    return -1;
  }

  uint64_t i = map->head_pos;
  DEBUG("unmap callback fn: before unmap head_pos %d tail_pos %d\n",
        map->head_pos, map->tail_pos);  

  *callback = (uint64_t *) map->map_ring[i].callback;
  *context =  map->map_ring[i].context;
  map->map_ring[i].callback = NULL;
  map->map_ring[i].context = NULL;
  map->head_pos = (1 + map->head_pos) % map->ring_len;

  DEBUG("unmap callback fn: callback 0x%p, context 0x%p\n",
        *callback, *context);
  DEBUG("end unmap callback fn: after unmap head_pos %d tail_pos %d\n",
        map->head_pos, map->tail_pos);
  return 0;
}

static int e1000e_map_callback(struct e1000e_map_ring* map,
                               void (*callback)(nk_net_dev_status_t, void*),
                               void* context)
{
  DEBUG("map callback fn: head_pos %d tail_pos %d\n", map->head_pos, map->tail_pos);
  if (map->head_pos == ((map->tail_pos + 1) % map->ring_len)) {
    // when the mapping callback queue is full
    ERROR("map callback fn: Callback mapping queue is full.\n");
    return -1;
  }

  DEBUG("map callback fn: map head_pos: %d, tail_pos: %d\n",
        map->head_pos, map->tail_pos);
  uint64_t i = map->tail_pos;
  struct e1000e_fn_map *fnmap = (map->map_ring + i);
  fnmap->callback = callback;
  fnmap->context = (uint64_t *)context;
  map->tail_pos = (1 + map->tail_pos) % map->ring_len;
  DEBUG("map callback fn: callback 0x%p, context 0x%p\n",
        callback, context);
  DEBUG("end map callback fn: after map head_pos: %d, tail_pos: %d\n",
        map->head_pos, map->tail_pos);
  return 0;
}

static void e1000e_interpret_int(struct e1000e_state* state, uint32_t int_reg)
{
  if (int_reg & E1000E_ICR_TXDW) INFO("\t TXDW triggered\n");
  if (int_reg & E1000E_ICR_TXQE) INFO("\t TXQE triggered\n");
  if (int_reg & E1000E_ICR_LSC)  INFO("\t LSC triggered\n");
  if (int_reg & E1000E_ICR_RXO)  INFO("\t RX0 triggered\n");
  if (int_reg & E1000E_ICR_RXT0) INFO("\t RXT0 triggered\n");
  if (int_reg & E1000E_ICR_TXD_LOW) INFO("\t TXD_LOW triggered\n");
  if (int_reg & E1000E_ICR_SRPD) {
    INFO("\t SRPD triggered\n");
    INFO("\t RSRPD.size %u\n", READ_MEM(state, E1000E_RSRPD_OFFSET));
  }
  if (int_reg & E1000E_ICR_RXQ0) INFO("\t RXQ0 triggered\n");
  if (int_reg & E1000E_ICR_RXQ1) INFO("\t RXQ1 triggered\n");
  if (int_reg & E1000E_ICR_TXQ0) INFO("\t TXQ0 triggered\n");
  if (int_reg & E1000E_ICR_TXQ1) INFO("\t TXQ1 triggered\n");
  if (int_reg & E1000E_ICR_OTHER) INFO("\t Other \n");
  if (int_reg & E1000E_ICR_INT_ASSERTED) INFO("\t INT_ASSERTED triggered\n");
  INFO("interpret int: end --------------------\n");    
} 

static void e1000e_interpret_ims(struct e1000e_state* state)
{
  uint32_t ims_reg = READ_MEM(state, E1000E_IMS_OFFSET);
  INFO("interpret ims: IMS 0x%08x\n", ims_reg);
  e1000e_interpret_int(state, ims_reg);  
}

static void e1000e_interpret_icr(struct e1000e_state* state)
{
  uint32_t icr_reg = READ_MEM(state, E1000E_ICR_OFFSET);
  INFO("interpret icr: ICR 0x%08x\n", icr_reg);
  e1000e_interpret_int(state, icr_reg);
}

static int e1000e_post_send(void *vstate,
			    uint8_t *src,
			    uint64_t len,
			    void (*callback)(nk_net_dev_status_t, void *),
			    void *context)
{
  // always map callback
  struct e1000e_state* state = (struct e1000e_state*) vstate;
  DEBUG("post tx fn: callback 0x%p context 0x%p\n", callback, context);

  // #measure
  TIMING_GET_TSC(state->measure.tx.postx_map.start);
  int result = e1000e_map_callback(state->tx_map, callback, context);
  TIMING_GET_TSC(state->measure.tx.postx_map.end);

  // #measure
  TIMING_GET_TSC(state->measure.tx.xpkt.start);
  if (!result) {
    result = e1000e_send_packet(src, len, (struct e1000e_state*) state);
  }
  TIMING_GET_TSC(state->measure.tx.xpkt.end);

  DEBUG("post tx fn: end\n");
  return result;
}

static int e1000e_post_receive(void *vstate,
			       uint8_t *src,
			       uint64_t len,
			       void (*callback)(nk_net_dev_status_t, void *),
			       void *context)
{
  // mapping the callback always
  // if result != -1 receive packet
  struct e1000e_state* state = (struct e1000e_state*) vstate;
  DEBUG("post rx fn: callback 0x%p, context 0x%p\n", callback, context);

  // #measure
  TIMING_GET_TSC(state->measure.rx.postx_map.start);
  int result = e1000e_map_callback(state->rx_map, callback, context);
  TIMING_GET_TSC(state->measure.rx.postx_map.end);

  // #measure
  TIMING_GET_TSC(state->measure.rx.xpkt.start);
  if (!result) {
    result = e1000e_receive_packet(src, len, state);
  }
  TIMING_GET_TSC(state->measure.rx.xpkt.end);

  DEBUG("post rx fn: end --------------------\n");
  return result;
}

enum pkt_op { op_unknown, op_tx, op_rx };

static int e1000e_irq_handler(excp_entry_t * excp, excp_vec_t vec, void *s)
{
  DEBUG("irq_handler fn: vector: 0x%x rip: 0x%p s: 0x%p\n",
        vec, excp->rip, s);

  // #measure
  uint64_t irq_start = 0;
  uint64_t irq_end = 0;
  uint64_t callback_start = 0;
  uint64_t callback_end = 0;
  enum pkt_op which_op = op_unknown; 
  
  TIMING_GET_TSC(irq_start);
  struct e1000e_state* state = s;
  uint32_t icr = READ_MEM(state, E1000E_ICR_OFFSET);
  uint32_t mask_int = icr & state->ims_reg;
  DEBUG("irq_handler fn: ICR: 0x%08x IMS: 0x%08x mask_int: 0x%08x\n",
        icr, state->ims_reg, mask_int);

  void (*callback)(nk_net_dev_status_t, void*) = NULL;
  void *context = NULL;
  nk_net_dev_status_t status = NK_NET_DEV_STATUS_SUCCESS;

  if (mask_int & (E1000E_ICR_TXDW | E1000E_ICR_TXQ0)) {
    
    which_op = op_tx;
    // transmit interrupt
    DEBUG("irq_handler fn: handle the txdw interrupt\n");
    TIMING_GET_TSC(state->measure.tx.irq_unmap.start);
    e1000e_unmap_callback(state->tx_map,
                          (uint64_t **)&callback,
                          (void **)&context);
    TIMING_GET_TSC(state->measure.tx.irq_unmap.end);
    
    // if there is an error while sending a packet, set the error status
    if (TXD_STATUS(TXD_PREV_HEAD).ec || TXD_STATUS(TXD_PREV_HEAD).lc) {
      ERROR("irq_handler fn: transmit errors\n");
      status = NK_NET_DEV_STATUS_ERROR;
    }

    // update the head of the ring buffer
    TXD_PREV_HEAD = TXD_INC(1, TXD_PREV_HEAD);
    DEBUG("irq_handler fn: total packet transmitted = %d\n",
          READ_MEM(state, E1000E_TPT_OFFSET));
  }

  if (mask_int & (E1000E_ICR_RXT0 | E1000E_ICR_RXO | E1000E_ICR_RXQ0)) {
    which_op = op_rx;
    // receive interrupt
    /* if (mask_int & E1000E_ICR_RXT0) { */
    /*   DEBUG("irq_handler fn: handle the rxt0 interrupt\n"); */
    /* } */

    /* if (mask_int & E1000E_ICR_RXO) { */
    /*   DEBUG("irq_handler fn: handle the rx0 interrupt\n"); */
    /* } */
   
    // INFO("rx length %d\n", RXD_LENGTH(RXD_PREV_HEAD));
    TIMING_GET_TSC(state->measure.rx.irq_unmap.start);
    e1000e_unmap_callback(state->rx_map,
                          (uint64_t **)&callback,
                          (void **)&context);
    TIMING_GET_TSC(state->measure.rx.irq_unmap.end);

    // e1000e_interpret_ims(state);
    // TODO: check if we need this line
    // WRITE_MEM(state, E1000E_IMC_OFFSET, E1000E_ICR_RXO);
    
    // checking errors
    if (RXD_ERRORS(RXD_PREV_HEAD)) {
      ERROR("irq_handler fn: receive an error packet\n");
      status = NK_NET_DEV_STATUS_ERROR;
    }

    // in the irq, update only the head of the buffer
    RXD_PREV_HEAD = RXD_INC(1, RXD_PREV_HEAD);
  }

  TIMING_GET_TSC(callback_start);
  if (callback) {
    DEBUG("irq_handler fn: invoke callback function callback: 0x%p\n", callback);
    callback(status, context);
  }
  TIMING_GET_TSC(callback_end);

  DEBUG("irq_handler fn: end irq\n\n\n");
  // DO NOT DELETE THIS LINE.
  // must have this line at the end of the handler
  IRQ_HANDLER_END();

#if TIMING
  // #measure
  irq_end = rdtsc();
  
  if (which_op == op_tx) {
    state->measure.tx.irq_callback.start = callback_start;
    state->measure.tx.irq_callback.end = callback_end;
    state->measure.tx.irq.start = irq_start;
    state->measure.tx.irq.end = irq_end;
  } else {
    state->measure.rx.irq_callback.start = callback_start;
    state->measure.rx.irq_callback.end = callback_end;
    state->measure.rx.irq.start = irq_start;
    state->measure.rx.irq.end = irq_end;
  }
#endif
  
  return 0;
}

uint32_t e1000e_read_speed_bit(uint32_t reg, uint32_t mask, uint32_t shift) {
  uint32_t speed = (reg & mask) >> shift;
  if (speed == E1000E_SPEED_ENCODING_1G_V1 || speed == E1000E_SPEED_ENCODING_1G_V2) {
    return E1000E_SPEED_ENCODING_1G_V1;
  }
  return speed;
}

char* e1000e_read_speed_char(uint32_t reg, uint32_t mask, uint32_t shift) {
  uint32_t encoding = e1000e_read_speed_bit(reg, mask, shift);
  char* res = NULL;
  switch(encoding) {
    case E1000E_SPEED_ENCODING_10M:
      res = "10M";
      break;
    case E1000E_SPEED_ENCODING_100M:
      res = "100M";
      break;
    case E1000E_SPEED_ENCODING_1G_V1:
    case E1000E_SPEED_ENCODING_1G_V2:
      res = "1G";
      break;
  }
  return res;
}

static struct nk_net_dev_int ops = {
  .get_characteristics = e1000e_get_characteristics,
  .post_receive        = e1000e_post_receive,
  .post_send           = e1000e_post_send,
};


int e1000e_pci_init(struct naut_info * naut)
{
  struct pci_info *pci = naut->sys.pci;
  struct list_head *curbus, *curdev;
  uint16_t num = 0;

  INFO("init\n");

  if (!pci) {
    ERROR("No PCI info\n");
    return -1;
  }
 
  INIT_LIST_HEAD(&dev_list);
  
  DEBUG("Finding e1000e devices\n");

  list_for_each(curbus,&(pci->bus_list)) {
    struct pci_bus *bus = list_entry(curbus,struct pci_bus,bus_node);

    DEBUG("Searching PCI bus %u for E1000E devices\n", bus->num);

    list_for_each(curdev, &(bus->dev_list)) {
      struct pci_dev *pdev = list_entry(curdev,struct pci_dev,dev_node);
      struct pci_cfg_space *cfg = &pdev->cfg;

      DEBUG("Device %u is a 0x%x:0x%x\n", pdev->num, cfg->vendor_id, cfg->device_id);
      // intel vendor id and e1000e device id
      if (cfg->vendor_id==INTEL_VENDOR_ID && cfg->device_id==E1000E_DEVICE_ID) {
	int foundio=0, foundmem=0;
	
        DEBUG("Found E1000E Device\n");
        struct e1000e_state *state = malloc(sizeof(struct e1000e_state));
	
        if (!state) {
          ERROR("Cannot allocate device\n");
          return -1;
        }

        memset(state,0,sizeof(*state));
	
	// We will only support MSI for now

        // find out the bar for e1000e
        for (int i=0;i<6;i++) {
          uint32_t bar = pci_cfg_readl(bus->num, pdev->num, 0, 0x10 + i*4);
          uint32_t size;
          DEBUG("bar %d: 0x%0x\n",i, bar);
          // go through until the last one, and get out of the loop
          if (bar==0) {
            break;
          }
          // get the last bit and if it is zero, it is the memory
          // " -------------------------"  one, it is the io
          if (!(bar & 0x1)) {
            uint8_t mem_bar_type = (bar & 0x6) >> 1;
            if (mem_bar_type != 0) { // 64 bit address that we do not handle it
              ERROR("Cannot handle memory bar type 0x%x\n", mem_bar_type);
              return -1;
            }
          }

          // determine size
          // write all 1s, get back the size mask
          pci_cfg_writel(bus->num, pdev->num, 0, 0x10 + i*4, 0xffffffff);
          // size mask comes back + info bits
          // write all ones and read back. if we get 00 (negative size), size = 4.
          size = pci_cfg_readl(bus->num, pdev->num, 0, 0x10 + i*4);

          // mask all but size mask
          if (bar & 0x1) { // I/O
            size &= 0xfffffffc;
          } else { // memory
            size &= 0xfffffff0;
          }
          // two complement, get back the positive size
          size = ~size;
          size++;

          // now we have to put back the original bar
          pci_cfg_writel(bus->num, pdev->num, 0, 0x10 + i*4, bar);

          if (!size) { // size = 0 -> non-existent bar, skip to next one
            continue;
          }

          uint32_t start = 0;
          if (bar & 0x1) { // IO
            start = state->ioport_start = bar & 0xffffffc0;
            state->ioport_end = state->ioport_start + size;
	    foundio=1;
          }

          if (!(bar & 0xe) && (i == 0)) { //MEM
            start = state->mem_start = bar & 0xfffffff0;
            state->mem_end = state->mem_start + size;
	    foundmem=1;
          }

          DEBUG("bar %d is %s address=0x%x size=0x%x\n", i,
                bar & 0x1 ? "io port":"memory", start, size);
        }

        INFO("Adding e1000e device: bus=%u dev=%u func=%u: ioport_start=%p ioport_end=%p mem_start=%p mem_end=%p\n",
             bus->num, pdev->num, 0,
             state->ioport_start, state->ioport_end,
             state->mem_start, state->mem_end);

        uint16_t pci_cmd = E1000E_PCI_CMD_MEM_ACCESS_EN | E1000E_PCI_CMD_IO_ACCESS_EN | E1000E_PCI_CMD_LANRW_EN; // | E1000E_PCI_CMD_INT_DISABLE;
        DEBUG("init fn: new pci cmd: 0x%04x\n", pci_cmd);
        pci_cfg_writew(bus->num,pdev->num,0,E1000E_PCI_CMD_OFFSET, pci_cmd);
        DEBUG("init fn: pci_cmd 0x%04x expects 0x%04x\n",
              pci_cfg_readw(bus->num,pdev->num, 0, E1000E_PCI_CMD_OFFSET),
              pci_cmd);
        DEBUG("init fn: pci status 0x%04x\n",
              pci_cfg_readw(bus->num,pdev->num, 0, E1000E_PCI_STATUS_OFFSET));
	
        list_add(&dev_list, &state->node);
        sprintf(state->name, "e1000e-%d", num);
        num++;
        
        state->pci_dev = pdev;
	
        state->netdev = nk_net_dev_register(state->name, 0, &ops, (void *)state);
	
        if (!state->netdev) {
          ERROR("init fn: Cannot register the e1000e device \"%s\"", state->name);
          return -1;
        }

	if (!foundmem) {
	    ERROR("init fn: ignoring device %s as it has no memory access method\n",state->name);
	    continue;
	}
	
	// now bring up the device / interrupts

	// disable interrupts
	WRITE_MEM(state, E1000E_IMC_OFFSET, 0xffffffff);
	DEBUG("init fn: device reset\n");
	WRITE_MEM(state, E1000E_CTRL_OFFSET, E1000E_CTRL_RST);
	// delay about 5 us (manual suggests 1us)
	udelay(RESTART_DELAY);
	// disable interrupts again after reset
	WRITE_MEM(state, E1000E_IMC_OFFSET, 0xffffffff);
	
	uint32_t mac_high = READ_MEM(state, E1000E_RAH_OFFSET);
	uint32_t mac_low = READ_MEM(state, E1000E_RAL_OFFSET);
	uint64_t mac_all = ((uint64_t)mac_low+((uint64_t)mac_high<<32)) & 0xffffffffffff;
	DEBUG("init fn: mac_all = 0x%lX\n", mac_all);
	DEBUG("init fn: mac_high = 0x%x mac_low = 0x%x\n", mac_high, mac_low);

	memcpy((void*)state->mac_addr, &mac_all, MAC_LEN);
	
	// set the bit 22th of GCR register
	uint32_t gcr_old = READ_MEM(state, E1000E_GCR_OFFSET);
	WRITE_MEM(state, E1000E_GCR_OFFSET, gcr_old | E1000E_GCR_B22);
	uint32_t gcr_new = READ_MEM(state, E1000E_GCR_OFFSET);
	DEBUG("init fn: GCR = 0x%08x old gcr = 0x%08x tested %s\n",
	      gcr_new, gcr_old,
	      gcr_new & E1000E_GCR_B22 ? "pass":"fail");
	WRITE_MEM(state, E1000E_GCR_OFFSET, E1000E_GCR_B22);
	
	WRITE_MEM(state, E1000E_FCAL_OFFSET, 0);
	WRITE_MEM(state, E1000E_FCAH_OFFSET, 0);
	WRITE_MEM(state, E1000E_FCT_OFFSET, 0);

	// uint32_t ctrl_reg = (E1000E_CTRL_FD | E1000E_CTRL_FRCSPD | E1000E_CTRL_FRCDPLX | E1000E_CTRL_SLU | E1000E_CTRL_SPEED_1G) & ~E1000E_CTRL_ILOS;
	// p50 manual
	uint32_t ctrl_reg = E1000E_CTRL_SLU | E1000E_CTRL_RFCE | E1000E_CTRL_TFCE;
  
	WRITE_MEM(state, E1000E_CTRL_OFFSET, ctrl_reg);

	DEBUG("init fn: e1000e ctrl = 0x%08x expects 0x%08x\n",
	      READ_MEM(state, E1000E_CTRL_OFFSET),
	      ctrl_reg);
	
	uint32_t status_reg = READ_MEM(state, E1000E_STATUS_OFFSET);
	DEBUG("init fn: e1000e status = 0x%08x\n", status_reg);
	DEBUG("init fn: does status.fd = 0x%01x? %s\n",
	     E1000E_STATUS_FD, 
	     status_reg & E1000E_STATUS_FD ? "full deplex":"halt deplex");
	DEBUG("init fn: status.speed 0x%02x %s\n",
	      e1000e_read_speed_bit(status_reg, E1000E_STATUS_SPEED_MASK, 
				    E1000E_STATUS_SPEED_SHIFT), 
	      e1000e_read_speed_char(status_reg, E1000E_STATUS_SPEED_MASK, 
				     E1000E_STATUS_SPEED_SHIFT));
	
	DEBUG("init fn: status.asdv 0x%02x %s\n",
	     e1000e_read_speed_bit(status_reg, 
				   E1000E_STATUS_ASDV_MASK, 
				   E1000E_STATUS_ASDV_SHIFT),
	     e1000e_read_speed_char(status_reg, 
				    E1000E_STATUS_ASDV_MASK,
				    E1000E_STATUS_ASDV_SHIFT));
	
	if (e1000e_read_speed_bit(status_reg, E1000E_STATUS_SPEED_MASK, E1000E_STATUS_SPEED_SHIFT) !=  e1000e_read_speed_bit(status_reg, E1000E_STATUS_ASDV_MASK, E1000E_STATUS_ASDV_SHIFT)) {
	    ERROR("init fn: setting speed and detecting speed do not match!!!\n");
	}
	
	DEBUG("init fn: status.lu 0x%01x %s\n",
	      (status_reg & E1000E_STATUS_LU) >> 1,
	      (status_reg & E1000E_STATUS_LU) ? "link is up.":"link is down.");
	DEBUG("init fn: status.phyra %s Does the device require PHY initialization?\n",
	      status_reg & E1000E_STATUS_PHYRA ? "1 Yes": "0 No");
	
	DEBUG("init fn: init receive ring\n");
	e1000e_init_receive_ring(state);
	DEBUG("init fn: init transmit ring\n");
	e1000e_init_transmit_ring(state);
	
	WRITE_MEM(state, E1000E_IMC_OFFSET, 0);
	DEBUG("init fn: IMC = 0x%08x expects 0x%08x\n",
	      READ_MEM(state, E1000E_IMC_OFFSET), 0);
	
	DEBUG("init fn: interrupt driven\n");

	
	if (pdev->msi.type == PCI_MSI_NONE) {
	    ERROR("Device %s does not support MSI - skipping\n", state->name);
	    continue;
	}

	uint64_t num_vecs = pdev->msi.num_vectors_needed;
	uint64_t base_vec = 0;

	if (idt_find_and_reserve_range(num_vecs,1,&base_vec)) {
	    ERROR("Cannot find %d vectors for %s - skipping\n",num_vecs,state->name);
	    continue;
	}

	DEBUG("%s vectors are %d..%d\n",state->name,base_vec,base_vec+num_vecs-1);

	if (pci_dev_enable_msi(pdev, base_vec, num_vecs, 0)) {
	    ERROR("Failed to enable MSI for device %s - skipping\n", state->name);
	    continue;
	}

	int i;
	int failed=0;

	for (i=base_vec;i<(base_vec+num_vecs);i++) {
	    if (register_int_handler(i, e1000e_irq_handler, state)) {
		ERROR("Failed to register handler for vector %d on device %s - skipping\n",i,state->name);
		failed=1;
		break;
	    }
	}

	if (!failed) { 
	    for (i=base_vec; i<(base_vec+num_vecs);i++) {
		if (pci_dev_unmask_msi(pdev, i)) {
		    ERROR("Failed to unmask interrupt %d for device %s\n",i,state->name);
		    failed = 1;
		    break;
		}
	    }
	}


	if (!failed) { 
	    // interrupts should now be occuring
	    
	    // now configure device
	    // interrupt delay value = 0 -> does not delay
	    WRITE_MEM(state, E1000E_TIDV_OFFSET, 0);
	    // receive interrupt delay timer = 0
	    // -> interrupt when the device receives a package
	    WRITE_MEM(state, E1000E_RDTR_OFFSET_NEW, E1000E_RDTR_FPD);
	    DEBUG("init fn: RDTR new 0x%08x alias 0x%08x expect 0x%08x\n",
		  READ_MEM(state, E1000E_RDTR_OFFSET_NEW),
		  READ_MEM(state, E1000E_RDTR_OFFSET_ALIAS),
		  E1000E_RDTR_FPD);
	    WRITE_MEM(state, E1000E_RADV_OFFSET, 0);
	    
	    // enable only transmit descriptor written back, receive interrupt timer
	    // rx queue 0
	    uint32_t ims_reg = 0;
	    ims_reg = E1000E_ICR_TXDW | E1000E_ICR_TXQ0 | E1000E_ICR_RXT0 | E1000E_ICR_RXQ0;
	    WRITE_MEM(state, E1000E_IMS_OFFSET, ims_reg);
	    state->ims_reg = ims_reg;
	    e1000e_interpret_ims(state);
	    // after the interrupt is turned on, the interrupt handler is called
	    // due to the transmit descriptor queue empty.
	    
	    uint32_t icr_reg = READ_MEM(state, E1000E_ICR_OFFSET);
	    // e1000e_interpret_ims(state);
	    DEBUG("init fn: ICR before writting with 0xffffffff\n");
	    // e1000e_interpret_icr(state);
	    WRITE_MEM(state, E1000E_ICR_OFFSET, 0xffffffff);
	    // e1000e_interpret_icr(state);
	    DEBUG("init fn: finished writting ICR with 0xffffffff\n");
	    
	    // optimization 
	    WRITE_MEM(state, E1000E_AIT_OFFSET, 0);
	    WRITE_MEM(state, E1000E_TADV_OFFSET, 0);
	    DEBUG("init fn: end init fn --------------------\n");

	    INFO("%s operational\n",state->name);
	}
      }
    }
  }

  return 0;

}

int e1000e_pci_deinit() {
  INFO("deinited and leaking\n");
  return 0;
}

//
// DEBUGGING AND TIMING ROUTINES FOLLOW
//
//

#if TIMING
static void e1000e_read_write(struct e1000e_state *state)
{
  INFO("Testing Read Write Delay\n");
  volatile uint64_t mem_var = 0x55;
  uint64_t tsc_sta = 0;
  uint64_t tsc_end = 0;
  uint64_t tsc_delay = 0;

  INFO("Reading Delay from MEM\n");
  TIMING_GET_TSC(tsc_sta);
  uint64_t read_var = mem_var;
  TIMING_GET_TSC(tsc_end);
  TIMING_DIFF_TSC(tsc_delay, tsc_sta, tsc_end);
  INFO("Reading Delay %lu\n\n", tsc_delay);

  INFO("Writing Delay from MEM\n");
  TIMING_GET_TSC(tsc_sta);
  mem_var = 0x66;
  TIMING_GET_TSC(tsc_end);
  TIMING_DIFF_TSC(tsc_delay, tsc_sta, tsc_end);
  INFO("Writing Delay %lu\n\n", tsc_delay);

  INFO("Reading Delay from NIC\n");
  TIMING_GET_TSC(tsc_sta);
  uint32_t tctl_reg = READ_MEM(state, E1000E_TCTL_OFFSET);
  TIMING_GET_TSC(tsc_end);
  TIMING_DIFF_TSC(tsc_delay, tsc_sta, tsc_end);
  INFO("Reading Delay %lu\n\n", tsc_delay);
  
  INFO("Writing Delayi from NIC\n");
  TIMING_GET_TSC(tsc_sta);
  WRITE_MEM(state, E1000E_AIT_OFFSET, 0);
  TIMING_GET_TSC(tsc_end);
  TIMING_DIFF_TSC(tsc_delay, tsc_sta, tsc_end);
  INFO("Writing Delay, %lu\n\n", tsc_delay);

  INFO("Reading pci_cfg_readw Delay\n");
  TIMING_GET_TSC(tsc_sta);
  uint32_t status_pci = pci_dev_cfg_readw(state->pci_dev, E1000E_PCI_STATUS_OFFSET);
  TIMING_GET_TSC(tsc_end);
  TIMING_DIFF_TSC(tsc_delay, tsc_sta, tsc_end);
  INFO("Reading pci_dev_cfg_readw Delay, %lu\n\n", tsc_delay);
}
#endif

/*
static void e1000e_enable_psp(struct e1000e_state* state)
{
  uint32_t tctl_reg = READ_MEM(state, E1000E_TCTL_OFFSET);
  WRITE_MEM(state, E1000E_TCTL_OFFSET, tctl_reg | E1000E_TCTL_PSP);
  tctl_reg = READ_MEM(state, E1000E_TCTL_OFFSET);
  INFO("%s tctl psp %s\n", __func__, tctl_reg & E1000E_TCTL_PSP ? "on" : "off");
  return;
}

static void e1000e_disable_psp(struct e1000e_state* state)
{
  uint32_t tctl_reg = READ_MEM(state, E1000E_TCTL_OFFSET);
  WRITE_MEM(state, E1000E_TCTL_OFFSET, tctl_reg & ~E1000E_TCTL_PSP);
  tctl_reg = READ_MEM(state, E1000E_TCTL_OFFSET);
  INFO("%s tctl psp %s\n", __func__, tctl_reg & E1000E_TCTL_PSP ? "on" : "off");
  return;
}

static void e1000e_enable_psp_shell(struct e1000e_state* state)
{
  e1000e_enable_psp(state);
  return;
}

static void e1000e_disable_psp_shell(struct e1000e_state* state)
{
  e1000e_disable_psp(state);
  return;
}

static void e1000e_set_tipg(struct e1000e_state* state,
                            uint32_t ipgt,
                            uint32_t ipgr1,
                            uint32_t ipgr2)
{
  uint32_t tipg_reg = ipgt;
  tipg_reg |= (ipgr1 << E1000E_TIPG_IPGR1_SHIFT);
  tipg_reg |= (ipgr2 << E1000E_TIPG_IPGR2_SHIFT);
  WRITE_MEM(state, E1000E_TIPG_OFFSET, tipg_reg);
  DEBUG("e1000e_set_tipg fn: tipg 0x08x\n",
        READ_MEM(state, E1000E_TIPG_OFFSET));
  return; 
}

static void e1000e_opt(struct e1000e_state* state)
{
  WRITE_MEM(state, E1000E_CTRL_OFFSET,
            E1000E_CTRL_SLU | E1000E_CTRL_RFCE | E1000E_CTRL_TFCE);
  WRITE_MEM(state, E1000E_RCTL_OFFSET,
            E1000E_RCTL_EN | E1000E_RCTL_SBP | E1000E_RCTL_UPE | E1000E_RCTL_LPE | E1000E_RCTL_DTYP_LEGACY | E1000E_RCTL_BAM | E1000E_RCTL_RDMTS_HALF | E1000E_RCTL_BSIZE_2048 | E1000E_RCTL_SECRC);
  e1000e_disable_psp(state);
  e1000e_set_tipg(state, 6, 8, 6);
  return;
}

static void e1000e_opt_shell(struct e1000e_state* state)
{
  e1000e_opt(state);
}
 
static void e1000e_no_opt(struct e1000e_state* state)
{
  WRITE_MEM(state, E1000E_CTRL_OFFSET, E1000E_CTRL_SLU);
  WRITE_MEM(state, E1000E_RCTL_OFFSET,
            E1000E_RCTL_EN | E1000E_RCTL_SBP | E1000E_RCTL_UPE | E1000E_RCTL_LPE | E1000E_RCTL_DTYP_LEGACY | E1000E_RCTL_BAM | E1000E_RCTL_RDMTS_HALF | E1000E_RCTL_BSIZE_2048 | E1000E_RCTL_PMCF);
  e1000e_enable_psp(state);
  e1000e_set_tipg(state, 8, 10, 20);
  return;
}
 
static void e1000e_no_opt_shell(struct e1000e_state* state)
{
  e1000e_no_opt(state);
}


// TODO: panitan cleanup -> delete these functions
static void e1000e_enable_srpd_int(struct e1000e_state* state, uint32_t size)
{
  WRITE_MEM(state, E1000E_RSRPD_OFFSET, E1000E_RSPD_MASK & size);
  INFO("RSRPD 0x%04X\n", READ_MEM(state, E1000E_RSRPD_OFFSET));

  uint32_t ims_reg = READ_MEM(state, E1000E_IMS_OFFSET);
  WRITE_MEM(state, E1000E_IMS_OFFSET, ims_reg | E1000E_ICR_SRPD); 
  ims_reg = READ_MEM(state, E1000E_IMS_OFFSET);
  INFO("%s ims_reg & SRPD %s\n", __func__, ims_reg & E1000E_ICR_SRPD ? "on" : "off"); 
  return;
}

static void e1000e_disable_srpd_int(struct e1000e_state* state)
{
  WRITE_MEM(state, E1000E_RSRPD_OFFSET, 0);
  INFO("RSRPD 0x%04X\n", READ_MEM(state, E1000E_RSRPD_OFFSET));

  WRITE_MEM(state, E1000E_IMC_OFFSET, E1000E_ICR_SRPD); 
  uint32_t ims_reg = READ_MEM(state, E1000E_IMS_OFFSET);
  INFO("%s ims_reg & SRPD %s\n", __func__, ims_reg & E1000E_ICR_SRPD ? "on" : "off"); 
  return;
}

static void e1000e_enable_srpd_int_shell(struct e1000e_state* state, uint32_t size)
{
  e1000e_enable_srpd_int(state, size);
  return;
}

static void e1000e_disable_srpd_int_shell(struct e1000e_state* state)
{
  e1000e_disable_srpd_int(state);
  return;
}

static void e1000e_interpret_rctl(struct e1000e_state* state)
{
  uint32_t rctl_reg = READ_MEM(state, E1000E_RCTL_OFFSET);
  INFO("RCTL (Receive Ctrl Register)\n");
  INFO("\tReceiver:\t\t%s\n", 
      rctl_reg & E1000E_RCTL_EN ? "enabled" : "disabled");
  INFO("\tStore bad packets:\t\t%s\n", 
      rctl_reg & E1000E_RCTL_SBP ? "enabled" : "disabled");
  INFO("\tUnicast promicuous:\t\t%s\n", 
      rctl_reg & E1000E_RCTL_UPE ? "enabled" : "disabled");
  INFO("\tMulticast promicuous:\t\t%s\n", 
      rctl_reg & E1000E_RCTL_MPE ? "enabled" : "disabled");
  INFO("\tLong packet:\t\t%s\n", 
      rctl_reg & E1000E_RCTL_LPE ? "enabled" : "disabled");
  
  uint32_t rdmts = rctl_reg & E1000E_RCTL_RDMTS_MASK;
  switch(rdmts) {
    case E1000E_RCTL_RDMTS_HALF:
      INFO("\tDescriptor minimum threshold size:\t\t1/2\n");
      break;
    case E1000E_RCTL_RDMTS_QUARTER:
      INFO("\tDescriptor minimum threshold size:\t\t1/4\n");
      break;
    case E1000E_RCTL_RDMTS_EIGHTH:
      INFO("\tDescriptor minimum threshold size:\t\t1/8\n");
      break;
  }

  INFO("\tBroadcast accept mode:\t\t%s\n", 
      rctl_reg & E1000E_RCTL_BAM ? "accept" : "does't accept");
  INFO("\tVLAN filter:\t\t%s\n", 
      rctl_reg & E1000E_RCTL_VFE ? "enabled" : "disabled");
  INFO("\tPass MAC control frames:\t\t%s\n", 
      rctl_reg & E1000E_RCTL_PMCF ? "pass" : "doesn't pass");
  INFO("\tDiscard pause frames:\t\t%s\n", 
      rctl_reg & E1000E_RCTL_DPF ? "enabled" : "disabled");
  INFO("\tStrip Ethernet CRC:\t\t%s\n", 
      rctl_reg & E1000E_RCTL_SECRC ? "strip" : "does't strip");

  return;
}

static void e1000e_interpret_rctl_shell(struct e1000e_state* state)
{
  e1000e_interpret_rctl(state);
  return;
}

static void e1000e_interpret_tctl(struct e1000e_state* state)
{
  uint32_t tctl_reg = READ_MEM(state, E1000E_TCTL_OFFSET);
  INFO("TCTL (Transmit Ctrl Register)\n");
  INFO("\tTransmitter:\t\t%s\n", 
      tctl_reg & E1000E_TCTL_EN ? "enabled" : "disabled");
  INFO("\tPad short packets:\t\t%s\n", 
      tctl_reg & E1000E_TCTL_PSP ? "enabled" : "disabled");
  INFO("\tSoftware XOFF Transmission:\t\t%s\n", 
      tctl_reg & E1000E_TCTL_SWXOFF ? "enabled" : "disabled");
  INFO("\tRe-trasmit on late collision:\t\t%s\n", 
      tctl_reg & E1000E_TCTL_RTLC ? "enabled" : "disabled");
  return;
}

static void e1000e_interpret_tctl_shell(struct e1000e_state* state)
{
  e1000e_interpret_tctl(state);
  return;
}

static void e1000e_interpret_int_shell(struct e1000e_state* state)
{
  e1000e_interpret_icr(state);
  e1000e_interpret_ims(state);
}

static void e1000e_interpret_rxd(struct e1000e_state* state)
{
  INFO("interpret rxd: head %d tail %d\n",
        READ_MEM(state, E1000E_RDH_OFFSET),
        READ_MEM(state, E1000E_RDT_OFFSET));
  
  uint64_t* data_addr = RXD_ADDR(RXD_PREV_HEAD);
  INFO("interpret rxd: data buffer addr: 0x%p\n", data_addr);
  INFO("interpret rxd: data length: %d\n", RXD_LENGTH(RXD_PREV_HEAD));
  INFO("interpret rxd: status: 0x%02x dd: %d\n",
       RXD_STATUS(RXD_PREV_HEAD),
       RXD_STATUS(RXD_PREV_HEAD).dd);
  INFO("interpret rxd: error: 0x%02x\n", RXD_ERRORS(RXD_PREV_HEAD));
}

static void e1000e_interpret_rxd_shell(struct e1000e_state* state)
{
  e1000e_interpret_rxd(state);
}

static void e1000e_read_stat(struct e1000e_state* state)
{
  INFO("total packet transmitted %d\n", READ_MEM(state, E1000E_TPT_OFFSET));
  INFO("total packet received %d\n", READ_MEM(state, E1000E_TPR_OFFSET));
  uint64_t temp64 = 0;
  // this tor register must be read using two independent 32-bit accesses.
  temp64 = READ_MEM(state, E1000E_TORL_OFFSET);
  temp64 |= (((uint64_t) READ_MEM(state, E1000E_TORH_OFFSET)) << 32);
  INFO("total octets received %d\n", temp64);

  // this tot register must be read using two independent 32-bit accesses.
  temp64 = READ_MEM(state, E1000E_TOTL_OFFSET);
  temp64 |= (((uint64_t) READ_MEM(state, E1000E_TOTH_OFFSET)) << 32);
  INFO("total octets transmitted %d\n", temp64);

  INFO("receive error count %d\n",
      READ_MEM(state, E1000E_RXERRC_OFFSET));
  INFO("receive no buffer count %d\n",
      READ_MEM(state, E1000E_RNBC_OFFSET));
  INFO("good packet receive count %d\n",
      READ_MEM(state, E1000E_GPRC_OFFSET));
  INFO("good packet transmitted count %d\n",
      READ_MEM(state, E1000E_GPTC_OFFSET));
  INFO("collision count %d\n",
      READ_MEM(state, E1000E_COLC_OFFSET));
  INFO("receive length error %d\n", 
      READ_MEM(state, E1000E_RLEC_OFFSET));
  INFO("receive undersize count %d\n",
      READ_MEM(state, E1000E_RUC_OFFSET));
  INFO("receive oversize count %d\n",
      READ_MEM(state, E1000E_ROC_OFFSET));
}

static void e1000e_read_stat_shell(struct e1000e_state* state)
{
  e1000e_read_stat(state);
}

static void e1000e_disable_all_int(struct e1000e_state* state)
{
  WRITE_MEM(state, E1000E_IMC_OFFSET, 0xffffffff);
  DEBUG("disable all int fn: end --------------------\n");
  return;
}

static void e1000e_trigger_int(struct e1000e_state* state)
{
  // imc is a written only register.
  DEBUG("trigger int fn: reset all ints by writing to IMC with 0xffffffff\n");
  WRITE_MEM(state, E1000E_IMC_OFFSET, 0xffffffff);
  // enable only transmit descriptor written back and receive interrupt timer
  DEBUG("trigger int fn: set IMS with TXDW RXT0\n");
  WRITE_MEM(state, E1000E_IMS_OFFSET, E1000E_ICR_TXDW | E1000E_ICR_RXT0);

  if ((E1000E_ICR_TXDW | E1000E_ICR_RXT0) != READ_MEM(state, E1000E_IMS_OFFSET)) {
    ERROR("trigger int fn: the written and read values of IMS reg do not match\n");
    return;
  }
  // after the interrupt is turned on, the interrupt handler is called
  // due to the transmit descriptor queue empty.
  // set an interrupt on ICS register
  // ICS is w reg which means its value cannot be read back.
  DEBUG("trigger int fn: writing to ICS reg\n");
  WRITE_MEM(state, E1000E_ICS_OFFSET, E1000E_ICR_TXDW | E1000E_ICR_RXT0);
  uint32_t status_pci = pci_dev_cfg_readw(state->pci_dev, E1000E_PCI_STATUS_OFFSET);
  DEBUG("trigger int fn: status_pci 0x%04x int %d\n",
        status_pci, status_pci & E1000E_PCI_STATUS_INT);
  DEBUG("trigger int fn: end -------------------- \n");
  return;
}

static void e1000e_trigger_int_num(struct e1000e_state* state, uint32_t int_num)
{
  // imc is a written only register.
  WRITE_MEM(state, E1000E_IMC_OFFSET, 0xffffffff);
  // enable only transmit descriptor written back and receive interrupt timer
  WRITE_MEM(state, E1000E_IMS_OFFSET, int_num);
  if (int_num != READ_MEM(state, E1000E_IMS_OFFSET)) {
    ERROR("IMS reg: the written and read values do not match\n");
    return;
  }
  // after the interrupt is turned on, the interrupt handler is called
  // due to the transmit descriptor queue empty.
  // set an interrupt on ICS register
  WRITE_MEM(state, E1000E_ICS_OFFSET, int_num);

  uint32_t status_pci = pci_dev_cfg_readw(state->pci_dev, E1000E_PCI_STATUS_OFFSET);
  DEBUG("trigger int num fn: status_pci 0x%04x int %d\n",
        status_pci, status_pci & E1000E_PCI_STATUS_INT);
  DEBUG("trigger int num fn: end -------------------- \n");
  return;
}

static void e1000e_legacy_int_off(struct e1000e_state* state)
{
  uint16_t pci_cmd = E1000E_PCI_CMD_MEM_ACCESS_EN | E1000E_PCI_CMD_IO_ACCESS_EN | E1000E_PCI_CMD_LANRW_EN | E1000E_PCI_CMD_INT_DISABLE;
  DEBUG("legacy int off fn: pci cmd: 0x%04x\n", pci_cmd);
  pci_dev_cfg_writew(state->pci_dev, E1000E_PCI_CMD_OFFSET, pci_cmd);
  DEBUG("legacy int off fn: pci_cmd 0x%04x expects  0x%04x\n",
        pci_dev_cfg_readw(state->pci_dev, E1000E_PCI_CMD_OFFSET),pci_cmd);
  uint32_t status_pci = pci_dev_cfg_readw(state->pci_dev, E1000E_PCI_STATUS_OFFSET);
  DEBUG("legacy int off fn: status_pci 0x%04x int %d\n",
        status_pci, status_pci & E1000E_PCI_STATUS_INT);
  DEBUG("legacy int off fn: end -------------------- \n");
  return;
}

static void e1000e_legacy_int_on(struct e1000e_state* state)
{
  uint16_t pci_cmd = E1000E_PCI_CMD_MEM_ACCESS_EN | E1000E_PCI_CMD_IO_ACCESS_EN | E1000E_PCI_CMD_LANRW_EN;
  DEBUG("legacy int on: pci cmd: 0x%04x\n", pci_cmd);
  pci_dev_cfg_writew(state->pci_dev, E1000E_PCI_CMD_OFFSET, pci_cmd);
  DEBUG("legacy int on: pci_cmd 0x%04x expects  0x%04x\n",
        pci_dev_cfg_readw(state->pci_dev, E1000E_PCI_CMD_OFFSET), pci_cmd);
  uint32_t status_pci = pci_dev_cfg_readw(state->pci_dev, E1000E_PCI_STATUS_OFFSET);
  DEBUG("legacy int on fn: status_pci 0x%04x int %d\n",
        status_pci, status_pci & E1000E_PCI_STATUS_INT);
  DEBUG("legacy int on: end --------------------\n");
  return;
}

static void e1000e_reset_all_int(struct e1000e_state* state)
{
  e1000e_interpret_icr(state);
  WRITE_MEM(state, E1000E_ICR_OFFSET, 0xffffffff);
  e1000e_interpret_icr(state);
}

static void e1000e_reset_rxo(struct e1000e_state* state)
{
  e1000e_interpret_icr(state);
  WRITE_MEM(state, E1000E_ICR_OFFSET, E1000E_ICR_RXO);
  e1000e_interpret_icr(state);
}


*/
