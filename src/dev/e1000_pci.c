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
 * Copyright (c) 2017, Panitan Wongse-ammat, Marc Warrior, Galen Lansbury
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
#include <dev/e1000_pci.h>
#include <nautilus/irq.h>             // interrupt register
#include <nautilus/naut_string.h>     // memset, memcpy




#ifndef NAUT_CONFIG_DEBUG_E1000_PCI
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define INFO(fmt, args...)     INFO_PRINT("e1000_pci: " fmt, ##args)
#define DEBUG(fmt, args...)    DEBUG_PRINT("e1000_pci: " fmt, ##args)
#define ERROR(fmt, args...)    ERROR_PRINT("e1000_pci: " fmt, ##args)


#define READ_MEM(d, o)         (*((volatile uint32_t*)(((d)->mem_start)+(o))))
#define WRITE_MEM(d, o, v)     ((*((volatile uint32_t*)(((d)->mem_start)+(o))))=(v))

#define RXD_RING (state->rxd_ring) // e1000_ring *
#define RXD_PREV_HEAD (RXD_RING->head_prev)
#define RXD_TAIL (RXD_RING->tail_pos)
#define RX_PACKET_BUFFER (RXD_RING->packet_buffer) // void *, packet buff addr
#define RXD_RING_BUFFER (RXD_RING->ring_buffer) // void *, ring buff addr
#define RX_DESC(i) (((struct e1000_rx_desc*)RXD_RING_BUFFER)[(i)])
#define RXD_ERRORS(i) (((struct e1000_rx_desc*)RXD_RING_BUFFER)[(i)].errors)
#define RXD_STATUS(i) (((struct e1000_rx_desc*)RXD_RING_BUFFER)[(i)].status)
#define RXD_LENGTH(i) (((struct e1000_rx_desc*)RXD_RING_BUFFER)[(i)].length)
#define RXD_ADDR(i) (((struct e1000_rx_desc*)RXD_RING_BUFFER)[(i)].addr)

#define RXD_COUNT (RXD_RING->count)
#define RXD_INC(a,b) (((a) + (b)) % RXD_RING->count) // modular increment rxd index

#define TXD_RING (state->tx_ring) // e1000_ring *
#define TXD_PREV_HEAD (TXD_RING->head_prev)
#define TXD_TAIL (TXD_RING->tail_pos)
#define TX_PACKET_BUFFER (TXD_RING->packet_buffer) // void *, packet buff addr
#define TXD_RING_BUFFER (TXD_RING->ring_buffer) // void *, ring buff addr
#define TXD_CMD(i) (((struct e1000_tx_desc*)TXD_RING_BUFFER)[(i)].cmd)
#define TXD_STATUS(i) (((struct e1000_tx_desc*)TXD_RING_BUFFER)[(i)].status)
#define TXD_LENGTH(i) (((struct e1000_tx_desc*)TXD_RING_BUFFER)[(i)].length)
#define TXD_ADDR(i) (((struct e1000_tx_desc*)TXD_RING_BUFFER)[(i)].addr)

#define TXD_COUNT (TXD_RING->count)
#define TXD_INC(a,b) (((a) + (b)) % TXD_RING->count) // modular increment txd index

// a is old index, b is incr amount, c is queue size
#define TXMAP (state->tx_map)
#define RXMAP (state->rx_map)




// Constants
// These are configurable, and they will change the number of descriptors
// and the packet buffer pointed by the descriptors.
// The number of descriptors is always a multiple of eight.

#define TX_DSC_COUNT          128
#define TX_BLOCKSIZE          256 // bytes available per DMA block
#define RX_DSC_COUNT          128 // equal to DMA block count
#define RX_BLOCKSIZE          256 // bytes available per DMA block

// After this line, PLEASE DO NOT CHANGE ANYTHING.
// transmission unit
// Data sheet from page 36
// 16384 bytes is from the maximum packet buffer size that e1000 can receive.
// 16288 bytes is the maximum packet size that e1000 can send in theory.
// Ethernet standard MTU is 1500 bytes.
#define MAX_TU                16384    /* maximum transmission unit */
#define MIN_TU                48       /* minimum transmission unit */

// PCI CONFIG SPACE ************************************
#define INTEL_VENDOR_ID       0x8086
#define E1000_DEVICE_ID       0x100E
#define E1000_CTRL_OFFSET     0x00000  /* Device Control - RW */
#define E1000_STATUS_OFFSET   0x00008  /* Device Status - RO */

// REGISTER OFFSETS ************************************
#define TDBAL_OFFSET          0x3800   // transmit descriptor base address low (64b)
#define TDBAH_OFFSET          0x3804   // transmit descriptor base address high
#define TDLEN_OFFSET          0x3808   // transmit descriptor list length
#define TDH_OFFSET            0x3810   // transmit descriptor head
#define TDT_OFFSET            0x3818   // transmit descriptor tail
#define TCTL_OFFSET           0x0400   // transmit control
#define TIPG_OFFSET           0x0410   // transmit interpacket gap
#define E1000_TXDCTL_OFFSET   0x03828  // transmit descriptor control r/w
 
#define E1000_RAL_OFFSET      0x5400   // receive address (64b)
#define E1000_RAH_OFFSET      0x5404   //  

#define RDBAL_OFFSET          0x2800   // receive descriptor base address low
#define RDBAH_OFFSET          0x2804   // receive descriptor base address high
#define RDLEN_OFFSET          0x2808   // receive descriptor list length
#define RDH_OFFSET            0x2810   // receive descriptor head
#define RDT_OFFSET            0x2818   // receive descriptor tail
#define RCTL_OFFSET           0x0100   // receive control
#define E1000_RDTR_OFFSET     0x2820   // receive delay timer
#define E1000_RSRPD_OFFSET    0x02C00  // receive small packet detect interrupt r/w

#define E1000_TPT_OFFSET      0x40D4   // total package transmit
#define E1000_TPR_OFFSET      0x40D0   // total package receive
#define E1000_COLC_OFFSET     0x04028  // collision count
#define E1000_RXERRC_OFFSET   0x0400C  // rx error count

// 4 interrupt register offset
#define E1000_ICR_OFFSET      0x000C0  /* interrupt cause read register */
#define E1000_ICS_OFFSET      0x000C8  /* interrupt cause set register */
#define E1000_IMS_OFFSET      0x000D0  /* interrupt mask set/read register */
#define E1000_IMC_OFFSET      0x000D8  /* interrupt mask clear */
#define E1000_TIDV_OFFSET     0x03820  /* transmit interrupt delay value r/w */

// REGISTER BIT MASKS **********************************
// E1000 Transmit Control Register
#define E1000_TCTL_EN               (1 << 1)      // transmit enable
#define E1000_TCTL_PSP              (1 << 3)      // pad short packet
#define E1000_TCTL_CT               (0x0f << 4)   // collision threshold
#define E1000_TCTL_COLD_FD          (0x40 << 12)  // collision distance full duplex
#define E1000_TCTL_COLD_HD          (0x200 << 12) // collision distance half duplex
// IPG = inter packet gap
#define E1000_TIPG_IPGT             0x0a          // IPG transmit time
#define E1000_TIPG_IPGR1_IEEE8023   (0x8 << 10)   // TIPG1 for IEEE802.3
#define E1000_TIPG_IPGR2_IEEE8023   (0x6 << 20)   // TIPG2 for IEEE802.3

// E1000 Receive Control Register 
#define E1000_RCTL_EN               (1 << 1)      // Receiver Enable
#define E1000_RCTL_SBP              (1 << 2)      // Store Bad Packets
#define E1000_RCTL_UPE              (1 << 3)      // Unicast Promiscuous Enabled
#define E1000_RCTL_MPE              (1 << 4)      // Multicast Promiscuous Enabled
#define E1000_RCTL_LPE              (1 << 5)      // Long Packet Reception Enable
#define E1000_RCTL_LBM_NONE         (0 << 6)      // No Loopback
#define E1000_RCTL_LBM_PHY          (3 << 6)      // PHY or external SerDesc loopback
#define E1000_RCTL_RDMTS_HALF       (0 << 8)      // Free Buffer Threshold is 1/2 of RDLEN
#define E1000_RCTL_RDMTS_QUARTER    (1 << 8)      // Free Buffer Threshold is 1/4 of RDLEN
#define E1000_RCTL_RDMTS_EIGHTH     (2 << 8)    // Free Buffer Threshold is 1/8 of RDLEN
#define E1000_RCTL_MO_36            (0 << 12)   // Multicast Offset - bits 47:36
#define E1000_RCTL_MO_35            (1 << 12)   // Multicast Offset - bits 46:35
#define E1000_RCTL_MO_34            (2 << 12)   // Multicast Offset - bits 45:34
#define E1000_RCTL_MO_32            (3 << 12)   // Multicast Offset - bits 43:32
#define E1000_RCTL_BAM              (1 << 15)   // Broadcast Accept Mode
#define E1000_RCTL_VFE              (1 << 18)   // VLAN Filter Enable
#define E1000_RCTL_CFIEN            (1 << 19)   // Canonical Form Indicator Enable
#define E1000_RCTL_CFI              (1 << 20)   // Canonical Form Indicator Bit Value
#define E1000_RCTL_DPF              (1 << 22)   // Discard Pause Frames
#define E1000_RCTL_PMCF             (1 << 23)   // Pass MAC Control Frames
#define E1000_RCTL_SECRC            (1 << 26)   // Strip Ethernet CRC
 
// Buffer Sizes
// RCTL.BSEX = 0
#define E1000_RCTL_BSIZE_256        (3 << 16)
#define E1000_RCTL_BSIZE_512        (2 << 16)
#define E1000_RCTL_BSIZE_1024       (1 << 16)
#define E1000_RCTL_BSIZE_2048       (0 << 16)
// RCTL.BSEX = 1
#define E1000_RCTL_BSIZE_4096       ((3 << 16) | (1 << 25))
#define E1000_RCTL_BSIZE_8192       ((2 << 16) | (1 << 25))
#define E1000_RCTL_BSIZE_16384      ((1 << 16) | (1 << 25))
#define E1000_RCTL_BSIZE_MASK       ~((3 << 16) | (1 << 25))

#define E1000_RECV_BSIZE_256        256
#define E1000_RECV_BSIZE_512        512
#define E1000_RECV_BSIZE_1024       1024
#define E1000_RECV_BSIZE_2048       2048
#define E1000_RECV_BSIZE_4096       4096
#define E1000_RECV_BSIZE_8192       8192
#define E1000_RECV_BSIZE_16384      16384
#define E1000_RECV_BSIZE_MAX        16384

// interrupt bits of icr register
#define E1000_ICR_TXDW              1          // transmit descriptor written back 
#define E1000_ICR_TXQE              (1 << 1)   // transmit queue empty 
#define E1000_ICR_TXD_LOW           (1 << 15)  // transmit descriptor low threshold hit 

#define E1000_ICR_SRPD              (1 << 16)  // small receive packet detected 
#define E1000_ICR_RXT0              (1 << 7)   // receiver timer interrupt 
#define E1000_ICR_RXO               (1 << 6)   // receive overrun 
#define E1000_ICR_LSC               (1 << 2)   // link state change 


struct e1000_desc_ring {
  void *ring_buffer;
  uint32_t head_prev;
  uint32_t tail_pos;
  uint32_t count;
  void *packet_buffer;
  uint32_t blocksize;
};

struct e1000_rx_desc {
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
struct e1000_tx_desc {
  uint64_t* addr;
  uint16_t length;
  uint8_t  cso;
  struct {
    uint8_t eop : 1;
    uint8_t ifcs: 1;
    uint8_t ic  : 1;
    uint8_t rs  : 1;
    uint8_t rsvd: 1;
    uint8_t dext: 1;
    uint8_t vle : 1;
    uint8_t ide : 1;
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

struct e1000_fn_map {
  void (*callback)(nk_net_dev_status_t, void *);
  uint64_t *context;
};

struct e1000_map_ring {
  struct e1000_fn_map *map_ring;
  // head and tail positions of the fn_map ring queue
  uint64_t head_pos;
  uint64_t tail_pos;
  // the number of elements in the map ring buffer
  uint64_t ring_len;
};
  
struct e1000_state {
  // a pointer to the base class
  struct nk_net_dev *netdev;
  //  for our device list
  struct list_head e1000_node;
  // pci interrupt and interupt vector
  struct pci_dev *pci_dev;
  uint8_t   pci_intr;  // IRQ number on bus
  uint8_t   intr_vec;  // IRQ we will see
  // Where registers are mapped into the I/O address space
  uint16_t  ioport_start;
  uint16_t  ioport_end;  
  // Where registers are mapped into the physical memory address space
  uint64_t  mem_start;
  uint64_t  mem_end;

  char name[DEV_NAME_LEN];
  uint8_t mac_addr[6];

  struct e1000_desc_ring *tx_ring;
  struct e1000_desc_ring *rxd_ring;
  // a circular queue mapping between callback function and tx descriptor
  struct e1000_map_ring *tx_map;
  // a circular queue mapping between callback funtion and rx descriptor
  struct e1000_map_ring *rx_map;
  uint64_t rx_buffer_size;
};

static struct list_head dev_list;


// initialize ring buffer to hold transmit descriptors
static int e1000_init_transmit_ring(struct e1000_state *state) 
{
  TXMAP = malloc(sizeof(struct e1000_map_ring));
  if (!TXMAP) {
    ERROR("Cannot allocate txmap\n");
    return -1;
  }
  memset(TXMAP, 0, sizeof(struct e1000_map_ring));
  TXMAP->ring_len = TX_DSC_COUNT;

  TXMAP->map_ring = malloc(sizeof(struct e1000_fn_map) * TX_DSC_COUNT);
  if (!TXMAP->map_ring) {
    ERROR("Cannot allocate txmap->ring\n");
    return -1;
  }
  memset(TXMAP->map_ring, 0, sizeof(struct e1000_fn_map) * TX_DSC_COUNT);

  TXD_RING = malloc(sizeof(struct e1000_desc_ring));
  if (!TXD_RING) {
    ERROR("Cannot allocate TXD_RING\n");
    return -1;
  }
  memset(TXD_RING, 0, sizeof(struct e1000_desc_ring));

  // allocate TX_DESC_COUNT transmit descriptors in the ring buffer.
  TXD_RING_BUFFER = malloc(sizeof(struct e1000_tx_desc)*TX_DSC_COUNT);
  if (!TXD_RING_BUFFER) {
    ERROR("Cannot allocate TXD_RING_BUFFER\n");
    return -1;
  }
  memset(TXD_RING_BUFFER, 0, sizeof(struct e1000_tx_desc)*TX_DSC_COUNT);
  TXD_COUNT = TX_DSC_COUNT;

  DEBUG("TX RING BUFFER at 0x%p\n", TXD_RING_BUFFER);
  // store the address of the memory in TDBAL/TDBAH
  WRITE_MEM(state, TDBAL_OFFSET,
            (uint32_t)( 0x00000000ffffffff & (uint64_t) TXD_RING_BUFFER));
  WRITE_MEM(state, TDBAH_OFFSET,
            (uint32_t)((0xffffffff00000000 & (uint64_t) TXD_RING_BUFFER) >> 32));
  DEBUG("TXD_RING_BUFFER=0x%016lx, TDBAH=0x%08x, TDBAL=0x%08x\n",
        TXD_RING_BUFFER, READ_MEM(state, TDBAH_OFFSET),
        READ_MEM(state, TDBAL_OFFSET));
  // write tdlen: transmit descriptor length
  WRITE_MEM(state, TDLEN_OFFSET, sizeof(struct e1000_tx_desc)*TX_DSC_COUNT);
  // write the tdh, tdt with 0
  WRITE_MEM(state, TDT_OFFSET, 0);
  WRITE_MEM(state, TDH_OFFSET, 0);
  TXD_PREV_HEAD = 0;
  TXD_TAIL = 0;
  DEBUG("TDLEN = 0x%08x, TDH = 0x%08x, TDT = 0x%08x\n",
        READ_MEM(state, TDLEN_OFFSET), READ_MEM(state, TDH_OFFSET),
        READ_MEM(state, TDT_OFFSET));
  // write tctl register
  WRITE_MEM(state, TCTL_OFFSET,
            E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_COLD_FD);
  // write tipg regsiter     00,00 0000 0110,0000 0010 00,00 0000 1010 = 0x0060200a
  // will be zero when emulating hardware
  WRITE_MEM(state, TIPG_OFFSET,
            E1000_TIPG_IPGT | E1000_TIPG_IPGR1_IEEE8023 | E1000_TIPG_IPGR2_IEEE8023); 
  DEBUG("TCTL = 0x%08x, TIPG = 0x%08x\n",
        READ_MEM(state, TCTL_OFFSET), READ_MEM(state, TIPG_OFFSET));
  return 0;
}

// initialize a ring buffer to hold receive descriptor
static int e1000_init_receive_ring(struct e1000_state *state) 
{
  RXMAP = malloc(sizeof(struct e1000_map_ring));
  if (!RXMAP) {
    ERROR("Cannot allocate rxmap\n");
    return -1;
  }
  memset(RXMAP, 0, sizeof(struct e1000_map_ring));

  RXMAP->map_ring = malloc(sizeof(struct e1000_fn_map) * RX_DSC_COUNT);
  if (!RXMAP->map_ring) {
    ERROR("Cannot allocate rxmap->ring\n");
    return -1;
  }
  memset(RXMAP->map_ring, 0, sizeof(struct e1000_fn_map) * RX_DSC_COUNT);
  RXMAP->ring_len = RX_DSC_COUNT;

  RXD_RING = malloc(sizeof(struct e1000_desc_ring));
  if (!RXD_RING) {
    ERROR("Cannot allocate rxd_ring buffer\n");
    return -1;
  }
  memset(RXD_RING, 0, sizeof(struct e1000_desc_ring));
  RXD_RING->tail_pos = 0;

  // the number of the receive descriptor in the ring
  RXD_COUNT = RX_DSC_COUNT;

  // allocate the receive descriptor ring buffer
  uint32_t rx_desc_size = sizeof(struct e1000_rx_desc) * RX_DSC_COUNT;  
  RXD_RING_BUFFER = malloc(rx_desc_size);
  if (!RXD_RING_BUFFER) {
    ERROR("Cannot allocate RXD_RING_BUFFER\n");
    return -1;
  }
  memset(RXD_RING_BUFFER, 0, rx_desc_size);

  // store the address of the memory in TDBAL/TDBAH
  WRITE_MEM(state, RDBAL_OFFSET,
            (uint32_t)(0x00000000ffffffff & (uint64_t) RXD_RING_BUFFER));
  WRITE_MEM(state, RDBAH_OFFSET,
            (uint32_t)((0xffffffff00000000 & (uint64_t) RXD_RING_BUFFER) >> 32));
  DEBUG("rd_buffer = 0x%016lx, RDBAL = 0x%08x, RDBAH = 0x%08x\n",
        RXD_RING_BUFFER, READ_MEM(state, RDBAL_OFFSET),
        READ_MEM(state, RDBAH_OFFSET));
  
  // write rdlen
  WRITE_MEM(state, RDLEN_OFFSET, rx_desc_size);
  // write the rdh, rdt with 0
  WRITE_MEM(state, RDH_OFFSET, 0);
  WRITE_MEM(state, RDT_OFFSET, 0);
  RXD_PREV_HEAD = 0;
  RXD_TAIL = 0;
  // write rctl register specifing the receive mode
  uint32_t rctl_reg = E1000_RCTL_EN | E1000_RCTL_SBP | E1000_RCTL_UPE | E1000_RCTL_LPE | E1000_RCTL_BAM | E1000_RCTL_PMCF | E1000_RCTL_RDMTS_HALF | E1000_RCTL_BSIZE_2048;
  
  // receive buffer threshold and size
  DEBUG("rctl_reg = 0x%08x\n", rctl_reg);
  // WRITE_MEM(state, RCTL_OFFSET, 0x0083832e);
  WRITE_MEM(state, RCTL_OFFSET, rctl_reg);
  DEBUG("RDLEN=0x%08x, RDH=0x%08x, RDT=0x%08x, RCTL=0x%08x\n",
        READ_MEM(state, RDLEN_OFFSET),
        READ_MEM(state, RDH_OFFSET),
        READ_MEM(state, RDT_OFFSET),
        READ_MEM(state, RCTL_OFFSET));
  return 0;
}

static int e1000_send_packet(uint8_t* packet_addr,
                             uint64_t packet_size,
                             struct e1000_state *state) 
{
  DEBUG("e1000_send_packet fn\n");
  DEBUG("packet_addr 0x%p packet_size: %d\n", packet_addr, packet_size);
  TXD_TAIL = READ_MEM(state, TDT_OFFSET);
  DEBUG("status before sending a packet: TDH = %d TDT = %d tail_pos = %d\n",
        READ_MEM(state, TDH_OFFSET),
        READ_MEM(state, TDT_OFFSET),
        TXD_TAIL);
  DEBUG("tpt total packet transmit: %d\n", READ_MEM(state, E1000_TPT_OFFSET));
  if(packet_size > MAX_TU) {
    ERROR("packet is too large.\n");
    return -1;
  }

  // e1000_init_single_txd(TXD_TAIL, state); // make new descriptor
  memset(((struct e1000_tx_desc *)TXD_RING_BUFFER + TXD_TAIL),
         0, sizeof(struct e1000_tx_desc));
  TXD_ADDR(TXD_TAIL) = (uint64_t*) packet_addr;
  TXD_LENGTH(TXD_TAIL) = packet_size;
  // set up send flags
  TXD_CMD(TXD_TAIL).dext = 0;
  TXD_CMD(TXD_TAIL).vle = 0;
  TXD_CMD(TXD_TAIL).ifcs = 1;
  // set the end of packet flag if this is the last fragment
  TXD_CMD(TXD_TAIL).eop = 1;
  // interrupt delay enable
  // if ide = 0 and rs = 1, the transmit interrupt will occur immediately
  // after the packet is sent.
  // TXD_CMD(TXD_TAIL).ide = 1;
  // report the status of the descriptor
  TXD_CMD(TXD_TAIL).rs = 1;
  
  // increment transmit descriptor list tail by 1
  DEBUG("moving the tail\n");
  TXD_TAIL = TXD_INC(1, TXD_TAIL);
  WRITE_MEM(state, TDT_OFFSET, TXD_TAIL);
  DEBUG("status after moving tail: TDH = %d TDT = %d tail_pos = %d\n",
        READ_MEM(state, TDH_OFFSET),
        READ_MEM(state, TDT_OFFSET),
        TXD_TAIL);
  DEBUG("transmit error: %d\n",
        TXD_STATUS(TXD_PREV_HEAD).ec | TXD_STATUS(TXD_PREV_HEAD).lc);
  DEBUG("tpt total packet transmit: %d\n", READ_MEM(state, E1000_TPT_OFFSET));
  DEBUG("e1000_send_packet fn end\n");
  return 0;
}

static int e1000_receive_packet(uint8_t* buffer,
                                uint64_t buffer_size,
                                struct e1000_state *state) 
{
  DEBUG("e1000 receive packet fn buffer = 0x%p, len = %lu\n", buffer, buffer_size);
  DEBUG("before moving tail head: %d, tail: %d\n",
        READ_MEM(state, RDH_OFFSET),
        READ_MEM(state, RDT_OFFSET));
  // if the buffer size is changed,
  // let the network adapter know the new buffer size
  if(state->rx_buffer_size != buffer_size) {
    uint32_t rctl = READ_MEM(state, RCTL_OFFSET) & E1000_RCTL_BSIZE_MASK;
    switch(buffer_size) {
      case E1000_RECV_BSIZE_256:
        rctl |= E1000_RCTL_BSIZE_256;
        break;
      case E1000_RECV_BSIZE_512:
        rctl |= E1000_RCTL_BSIZE_512;
        break;
      case E1000_RECV_BSIZE_1024:
        rctl |= E1000_RCTL_BSIZE_1024;
        break;
      case E1000_RECV_BSIZE_2048:
        rctl |= E1000_RCTL_BSIZE_2048;
        break;
      case E1000_RECV_BSIZE_4096:
        rctl |= E1000_RCTL_BSIZE_4096;
        break;
      case E1000_RECV_BSIZE_8192:
        rctl |= E1000_RCTL_BSIZE_8192;
        break;
      case E1000_RECV_BSIZE_16384:
        rctl |= E1000_RCTL_BSIZE_16384;
        break;
      default:
        ERROR("Unmatch buffer size\n");
        return -1;
    }
    WRITE_MEM(state, RCTL_OFFSET, rctl);
    state->rx_buffer_size = buffer_size;
  }
        
  RXD_PREV_HEAD = READ_MEM(state, RDH_OFFSET);
  RXD_TAIL = READ_MEM(state, RDT_OFFSET);
  // e1000_init_single_rxd(RXD_TAIL, state);
  memset(((struct e1000_rx_desc *)RXD_RING_BUFFER + RXD_TAIL),
         0, sizeof(struct e1000_rx_desc));
  RXD_ADDR(RXD_TAIL) = (uint64_t*) buffer;
  WRITE_MEM(state, RDT_OFFSET, RXD_INC(RXD_TAIL, 1));
  RXD_TAIL = RXD_INC(RXD_TAIL, 1);
  DEBUG("after moving tail head: %d, prev_head: %d tail: %d\n",
        READ_MEM(state, RDH_OFFSET), RXD_PREV_HEAD,
        READ_MEM(state, RDT_OFFSET));
  
  DEBUG("end e1000 receive paket fn -----------------------\n");
  return 0;
}

uint64_t e1000_packet_size_to_buffer_size(uint64_t sz) 
{
  // Round up the number to the buffer size with power of two
  // In E1000, the packet buffer is the power of two.
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
  if(sz >= E1000_RECV_BSIZE_MAX) {
    return E1000_RECV_BSIZE_MAX;
  } else {
    return sz;
  }
}

int e1000_get_characteristics(void *vstate, struct nk_net_dev_characteristics *c) 
{
  if (!vstate) {
    ERROR("The device state pointer is null\n");
    return -1;
  }
  
  if(!c) {
    ERROR("The characteristics pointer is null\n");
    return -1;
  }
  
  struct e1000_state *state=(struct e1000_state*)vstate;
  memcpy(c->mac, (void *) state->mac_addr, ETHER_MAC_LEN);
  // minimum and the maximum transmission unit
  c->min_tu = MIN_TU; 
  c->max_tu = MAX_TU;
  c->packet_size_to_buffer_size = e1000_packet_size_to_buffer_size;
  return 0;
}


static int e1000_unmap_callback(struct e1000_map_ring* map,
                                uint64_t** callback,
                                void** context) 
{
  // callback is a function pointer
  DEBUG("unmap callback fn head_pos %d tail_pos %d\n", map->head_pos, map->tail_pos);
  if(map->head_pos == map->tail_pos) {
    // if there is an empty mapping ring buffer, do not unmap the callback
    ERROR("Try to unmap an empty queue\n");
    return -1;
  }

  uint64_t i = map->head_pos;
  // TODO(panitan)
  *callback = (uint64_t *) map->map_ring[i].callback;
  *context =  map->map_ring[i].context;
  map->map_ring[i].callback = NULL;
  map->map_ring[i].context = NULL;
  map->head_pos = (1 + map->head_pos) % map->ring_len;
  DEBUG("end unmap callback fn head_pos %d tail_pos %d\n", map->head_pos, map->tail_pos);
  return 0;
}

static int e1000_map_callback(struct e1000_map_ring* map,
                              void (*callback)(nk_net_dev_status_t, void*),
                              void* context) 
{
  DEBUG("map callback head_pos %d tail_pos %d\n", map->head_pos, map->tail_pos);
  if(map->head_pos == ((map->tail_pos + 1) % map->ring_len)) {
    // when the mapping callback queue is full
    ERROR("Callback mapping queue is full.\n");
    return -1;
  }

  uint64_t i = map->tail_pos;
  struct e1000_fn_map *fnmap = (map->map_ring + i);
  fnmap->callback = callback;
  fnmap->context = (uint64_t *)context;
  map->tail_pos = (1 + map->tail_pos) % map->ring_len;
  DEBUG("mapped callback head_pos: %d, tail_pos: %d\n", map->head_pos, map->tail_pos);
  return 0;
}

static int e1000_post_send(void *state,
			   uint8_t *src,
			   uint64_t len,
			   void (*callback)(nk_net_dev_status_t, void *),
			   void *context) 
{
  // always map callback
  int result = 0;
  DEBUG("post send fn callback 0x%p\n", callback);
  result = e1000_map_callback(((struct e1000_state*)state)->tx_map, callback, context);

  if (!result) {
    result = e1000_send_packet(src, len, (struct e1000_state*) state);
  }
  DEBUG("post send fn end\n");
  return result;
}

static int e1000_post_receive(void *state,
			      uint8_t *src,
			      uint64_t len,
			      void (*callback)(nk_net_dev_status_t, void *),
			      void *context) 
{
  // mapping the callback always
  // if result != -1 receive packet
  int result = 0;
  result  = e1000_map_callback(((struct e1000_state*)state)->rx_map, callback, context);

  if(!result) {
    result = e1000_receive_packet(src, len, (struct e1000_state*) state);
  }
  return result;
}

static int e1000_irq_handler(excp_entry_t * excp, excp_vec_t vec, void *s) 
{
  DEBUG("e1000_irq_handler fn vector: 0x%x rip: 0x%p\n", vec, excp->rip);

  struct e1000_state* state = (struct e1000_state *)s;

  uint32_t icr = READ_MEM(state, E1000_ICR_OFFSET);
  uint32_t ims = READ_MEM(state, E1000_IMS_OFFSET);
  uint32_t mask_int = icr & ims;
  DEBUG("ICR: 0x%08x IMS: 0x%08x mask_int: 0x%08x\n", icr, ims, mask_int);
  DEBUG("ICR: 0x%08x icr should be zero.\n",
        READ_MEM(state, E1000_ICR_OFFSET));
  
  void (*callback)(nk_net_dev_status_t, void*) = NULL;
  void *context = NULL;
  nk_net_dev_status_t status = NK_NET_DEV_STATUS_SUCCESS;
  
  if(mask_int & E1000_ICR_TXDW) {
    // transmit interrupt
    DEBUG("handle the txdw interrupt\n");
    e1000_unmap_callback(state->tx_map, (uint64_t **)&callback, (void **)&context);
    // if there is an error while sending a packet, set the error status
    if(TXD_STATUS(TXD_PREV_HEAD).ec || TXD_STATUS(TXD_PREV_HEAD).lc) {
      ERROR("transmit errors\n");
      status = NK_NET_DEV_STATUS_ERROR;
    }

    // update the head of the ring buffer
    TXD_PREV_HEAD = TXD_INC(1, TXD_PREV_HEAD);
    DEBUG("total packet transmitted = %d\n",
          READ_MEM(state, E1000_TPT_OFFSET));    
  }

  if(mask_int & E1000_ICR_RXT0) {
    // receive interrupt
    DEBUG("handle the rxt0 interrupt\n");
    e1000_unmap_callback(state->rx_map, (uint64_t **)&callback, (void **)&context);
    // checking errors
    if(RXD_ERRORS(RXD_PREV_HEAD)) {
      ERROR("receive an error packet\n");
      status = NK_NET_DEV_STATUS_ERROR;
    }
    DEBUG("RDLEN=0x%08x, RDH=0x%08x, RDT=0x%08x, RCTL=0x%08x\n",
		    READ_MEM(state, RDLEN_OFFSET),
		    READ_MEM(state, RDH_OFFSET),
		    READ_MEM(state, RDT_OFFSET),
		    READ_MEM(state, RCTL_OFFSET));
    
    // in the irq, update only the head of the buffer
    RXD_PREV_HEAD = RXD_INC(1, RXD_PREV_HEAD);    
    DEBUG("total packet received = %d\n",
          READ_MEM(state, E1000_TPR_OFFSET));
  }

  if(callback) {
    DEBUG("invoke callback function callback: 0x%p\n", callback);
    callback(status, context);
  }

  DEBUG("end irq\n\n\n");
  // must have this line at the end of the handler
  IRQ_HANDLER_END();
  return 0;
}


static struct nk_net_dev_int ops = {
  .get_characteristics = e1000_get_characteristics,
  .post_receive        = e1000_post_receive,
  .post_send           = e1000_post_send,
};


// This is a gruesome hack... 
#define IRQ_NUMBER       11

int map_pci_irq_to_vec(struct pci_bus *bus, struct pci_dev *pdev)
{
    return IRQ_NUMBER;
}

int e1000_pci_init(struct naut_info * naut) 
{
  struct pci_info *pci = naut->sys.pci;
  struct list_head *curbus, *curdev;
  uint32_t num = 0;

  INFO("init\n");

  INIT_LIST_HEAD(&dev_list);

  if (!pci) {
    ERROR("No PCI info\n");
    return -1;
  }

  DEBUG("Finding e1000 devices\n");

  list_for_each(curbus,&(pci->bus_list)) {
    struct pci_bus *bus = list_entry(curbus,struct pci_bus,bus_node);

    DEBUG("Searching PCI bus %u for E1000 devices\n", bus->num);

    list_for_each(curdev, &(bus->dev_list)) {
      struct pci_dev *pdev = list_entry(curdev,struct pci_dev,dev_node);
      struct pci_cfg_space *cfg = &pdev->cfg;

      DEBUG("Device %u is a 0x%x:0x%x\n", pdev->num, cfg->vendor_id, cfg->device_id);
      // intel vendor id and e1000 device id
      if (cfg->vendor_id==INTEL_VENDOR_ID && cfg->device_id==E1000_DEVICE_ID) {
        DEBUG("E1000 Device Found\n");
        struct e1000_state *state = malloc(sizeof(struct e1000_state));
        if (!state) {
          ERROR("Cannot allocate device\n");
          return -1;
        }

        memset(state,0,sizeof(*state));
        state->pci_dev = pdev;

        // PCI Interrupt (A..D)
        state->pci_intr = cfg->dev_cfg.intr_pin;

	// GRUESOME HACK
	state->intr_vec = map_pci_irq_to_vec(bus,pdev);

        // find out the bar for e1000
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

          uint32_t start;
          if (bar & 0x1) {
            start = state->ioport_start = bar & 0xffffffc0;
            state->ioport_end = state->ioport_start + size;
          } else {
            start = state->mem_start = bar & 0xfffffff0;
            state->mem_end = state->mem_start + size;
          }

          DEBUG("bar %d is %s address=0x%x size=0x%x\n", i,
                bar & 0x1 ? "io port":"memory", start, size);
        }

        INFO("Adding e1000 device: bus=%u dev=%u func=%u: pci_intr=%u intr_vec=%u ioport_start=%p ioport_end=%p mem_start=%p mem_end=%p\n",
             bus->num, pdev->num, 0,
             state->pci_intr, state->intr_vec,
             state->ioport_start, state->ioport_end,
             state->mem_start, state->mem_end);

        uint16_t old_cmd = pci_cfg_readw(bus->num,pdev->num,0,0x4);
        DEBUG("Old PCI CMD: 0x%04x\n",old_cmd);

        old_cmd |= 0x7;  // make sure bus master is enabled
        old_cmd &= ~0x40;

        DEBUG("New PCI CMD: 0x%04x\n",old_cmd);

        pci_cfg_writew(bus->num,pdev->num,0,0x4,old_cmd);

        uint16_t stat = pci_cfg_readw(bus->num,pdev->num,0,0x6);
        DEBUG("PCI STATUS: 0x%04x\n",stat);

        // read the status register at void ptr + offset
        uint32_t status = READ_MEM(state, E1000_STATUS_OFFSET);
        DEBUG("e1000 status = 0x%x\n", status);
        uint32_t mac_low = READ_MEM(state, E1000_RAL_OFFSET);
        uint32_t mac_high = READ_MEM(state, E1000_RAH_OFFSET);
        uint64_t mac_all = ((uint64_t)mac_low+((uint64_t)mac_high<<32)) & 0xffffffffffff;
        DEBUG("e1000 mac_all = 0x%lX\n", mac_all);
        DEBUG("e1000 mac_high = 0x%x mac_low = 0x%x\n", mac_high, mac_low);
        memcpy(state->mac_addr, &mac_all, ETHER_MAC_LEN);

        list_add(&dev_list, &state->e1000_node);
        sprintf(state->name, "e1000-%d", num);
        num++;

        state->netdev = nk_net_dev_register(state->name, 0, &ops, (void *)state);

        if(!state->netdev) {
          ERROR("Cannot register the device");
          return -1;
        }

	// register the interrupt handler
	register_irq_handler(state->intr_vec, e1000_irq_handler, state);
	nk_unmask_irq(state->intr_vec);

	// interrupt delay value = 0 -> does not delay
	WRITE_MEM(state, E1000_TIDV_OFFSET, 0);
	// receive interrupt delay timer = 0
	// -> interrupt when the device receives a package
	WRITE_MEM(state, E1000_RDTR_OFFSET, 0);
	// enable only transmit descriptor written back and receive interrupt timer
	WRITE_MEM(state, E1000_IMS_OFFSET, E1000_ICR_TXDW | E1000_ICR_RXT0);
	// after the interrupt is turned on, the interrupt handler is called
	// due to the transmit descriptor queue empty.
	
	uint32_t icr = READ_MEM(state, E1000_ICR_OFFSET);
	DEBUG("IMS: 0x%08x ICR = 0x%08x\n",
	      READ_MEM(state, E1000_IMS_OFFSET), icr);
	DEBUG("TXQE 0x%08x TXD_LOW 0x%08x TXDW 0x%08x\n",
	      icr, E1000_ICR_TXQE & icr, E1000_ICR_TXD_LOW & icr, E1000_ICR_TXDW & icr);
	if (e1000_init_transmit_ring(state)) { 
	    ERROR("Failed to init transmit descriptor ring\n");
	    return -1;
	} 
	if (e1000_init_receive_ring(state)) {
	    ERROR("Failed to init receive descriptor ring\n");
	    return -1;
	}
      }
    }
  }
  
  return 0;
}

int e1000_pci_deinit() 
{
  INFO("deinited\n");
  return 0;
}
