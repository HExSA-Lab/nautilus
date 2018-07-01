/**
 * @file
 * Ethernet Interface Skeleton
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/*
 * This file is a skeleton for developing Ethernet network interface
 * drivers for lwIP. Add code to the low_level functions and do a
 * search-and-replace for the word "ethernetif" to replace it with
 * something that better describes your network interface.
 */

#include "lwip/opt.h"
#include "nautilus/netdev.h"
#include "net/ethernet/ethernet_agent.h"

#if 1 /* don't build, this is only a skeleton, see previous comment */

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "netif/ppp/pppoe.h"

/* Define those to better describe your network interface. */
#define IFNAME0 'A'
#define IFNAME1 'P'
#define SEND_QUEUE_SIZE 15
#define RECEIVE_QUEUE_SIZE 15


/**
 * Helper struct to hold private data used to operate your ethernet interface.
 * Keeping the ethernet address of the MAC in this struct is not necessary
 * as it is already kept in the struct netif.
 * But this is only an example, anyway...
 */
struct ethernetif {
    //struct eth_addr *ethaddr;
    struct nk_net_dev* device;
    /* Add whatever per-interface state that is needed here. */
    char *name;
};

/* Forward declarations. */
static void  ethernetif_input(struct netif *netif, nk_ethernet_packet_t *pk);

static int type_netif_filter(nk_ethernet_packet_t *p, void *state)
{
    return 1;
}

static void parse_packet(nk_ethernet_packet_t *packet){
    u8_t *data = packet->raw;
    u8_t type0 = data[12];
    u8_t type1 = data[13];
    if(type0==0x08 && type1==0x06){
	packet->len=42;
    }
    else if(type0==0x08 && type1==0x00){
	u8_t l = data[16]*256;
	l += data[17];
	packet->len = l+14;
    }
    //printk("The len of packet is %d\n", packet->len);
}

static void recv_callback(nk_net_dev_status_t status,
		   nk_ethernet_packet_t *packet,
		   void *state)
{
    DEBUG("recv_callback\n");
    parse_packet(packet);
    /*
    int i=0;
    printk("%d\n",packet->len);
    uint8_t *t = packet->raw;
    for(i=0;i<100;i++)
	printk("%02x ", t[i]);
    printk("\n");
    */

    struct netif *netif = (struct netif *)state;
    struct ethernetif *ethernetif = netif->state;

    if (status) {
	ERROR("Bad packet receive - reissuing a receive\n");
	goto launch_receive;
    }
    //DEBUG("recv callback ipdev: %p\n", ethernetif->device);	
    ethernetif_input(netif, packet);
    nk_net_ethernet_release_packet(packet);//realse packet

launch_receive:


    if (nk_net_ethernet_agent_device_receive_packet(ethernetif->device,
						    0,
						    NK_DEV_REQ_CALLBACK,
						    recv_callback,
						    netif)) {
	ERROR("Failed to launch recurring receive - we are probably dead now\n");
    }
}

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void
low_level_init(struct netif *netif)
{
    struct ethernetif *ethernetif = netif->state;
    struct nk_net_dev *netDevice;
    struct nk_net_dev_characteristics c;
    char *name = ethernetif->name;
    char agent_name[32];

    snprintf(agent_name,32,"%s-agent-lwip",name);

    //printk("Looking for device named %s which will get agent %s\n", name, agent_name);
    
    netDevice = nk_net_dev_find(name);

    LWIP_ASSERT("Unable to find device", netDevice);

    nk_net_dev_get_characteristics(netDevice, &c);
    
    struct nk_net_ethernet_agent *agent = nk_net_ethernet_agent_create(netDevice, agent_name, SEND_QUEUE_SIZE, RECEIVE_QUEUE_SIZE);
    
    struct nk_net_dev *ipdev = nk_net_ethernet_agent_register_filter(agent, type_netif_filter, 0);//filter function to always return true
    
    ethernetif->device = ipdev;
    
    /* set MAC hardware address length */
    netif->hwaddr_len =ETHARP_HWADDR_LEN;

    memcpy(netif->hwaddr, c.mac, ETHER_MAC_LEN);
    
    netif->mtu = c.max_tu;

    /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

#if LWIP_IPV6 && LWIP_IPV6_MLD
    /*
     * For hardware/netifs that implement MAC filtering.
     * All-nodes link-local is handled by default, so we must let the hardware know
     * to allow multicast packets in.
     * Should set mld_mac_filter previously. */
    if (netif->mld_mac_filter != NULL) {
	ip6_addr_t ip6_allnodes_ll;
	ip6_addr_set_allnodes_linklocal(&ip6_allnodes_ll);
	netif->mld_mac_filter(netif, &ip6_allnodes_ll, NETIF_ADD_MAC_FILTER);
  }
#endif /* LWIP_IPV6 && LWIP_IPV6_MLD */
    
    nk_net_ethernet_agent_start(agent);
    if (nk_net_ethernet_agent_device_receive_packet(ipdev,
						    0,
						    NK_DEV_REQ_CALLBACK,
						    recv_callback,
						    netif)) {
	ERROR("Failed to launch recurring receive - we are probably dead now\n");
    }
  /* Do whatever else is needed to initialize interface. */
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become available since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
    struct ethernetif *ethernetif = netif->state;
    struct nk_net_dev *netDevice = ethernetif -> device;
    struct pbuf *q;
  DEBUG("low_level_output\n");
  //initiate transfer();

#if ETH_PAD_SIZE
  pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif
  
    nk_ethernet_packet_t *pk = nk_net_ethernet_alloc_packet(-1);
    u32_t len = 0;
    
  for (q = p; q != NULL; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    //send data from(q->payload, q->len);
      memcpy(pk->raw+len, q->payload, q->len);
      len+=q->len;
  }
    pk->len = len;

    if(nk_net_ethernet_agent_device_send_packet(ethernetif->device, pk, NK_DEV_REQ_NONBLOCKING, 0, 0)){
	ERROR("Fail to send a packet\n");
    	return ERR_MEM;	
    }

  //signal that packet should be sent();
  MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
  if (((u8_t*)p->payload)[0] & 1) {
    /* broadcast or multicast packet*/
    MIB2_STATS_NETIF_INC(netif, ifoutnucastpkts);
  } else {
    /* unicast packet */
    MIB2_STATS_NETIF_INC(netif, ifoutucastpkts);
  }
  /* increase ifoutdiscards or ifouterrors on error */

#if ETH_PAD_SIZE
  pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

  LINK_STATS_INC(link.xmit);

  return ERR_OK;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *
low_level_input(struct netif *netif, nk_ethernet_packet_t *pk)
{
  struct ethernetif *ethernetif = netif->state;
  struct nk_net_dev *netDevice = ethernetif -> device;
  struct pbuf *p, *q;
  u32_t len, now_index;
  now_index=0;
  /* Obtain the size of the packet and put it into the "len"
     variable. */

  len = pk->len;
  /*u32_t i;
  DEBUG("\n");
  for (i=0; i<len; i++){
      DEBUG("%c ",pk->raw[i]);
  }
  DEBUG("\n");*/
#if ETH_PAD_SIZE
  len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif
  //len += 42;
  /* We allocate a pbuf chain of pbufs from the pool. */
  p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);

  if (p != NULL) {

#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

    /* We iterate over the pbuf chain until we have read the entire
     * packet into the pbuf. */
    for (q = p; q != NULL; q = q->next) {
      /* Read enough bytes to fill this pbuf in the chain. The
       * available data in the pbuf is given by the q->len
       * variable.
       * This does not necessarily have to be a memcpy, you can also preallocate
       * pbufs for a DMA-enabled MAC and after receiving truncate it to the
       * actually received size. In this case, ensure the tot_len member of the
       * pbuf is the sum of the chained pbuf len members.
       */
      //read data into(q->payload, q->len);
        memcpy(q->payload, pk->raw+now_index, q->len);
        now_index+=q->len;
    }
      
    
    nk_net_ethernet_release_packet(pk);      


    MIB2_STATS_NETIF_ADD(netif, ifinoctets, p->tot_len);
    if (((u8_t*)p->payload)[0] & 1) {
      /* broadcast or multicast packet*/
      MIB2_STATS_NETIF_INC(netif, ifinnucastpkts);
    } else {
      /* unicast packet*/
      MIB2_STATS_NETIF_INC(netif, ifinucastpkts);
    }
#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

    LINK_STATS_INC(link.recv);
  } else {
    //drop packet();
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
    MIB2_STATS_NETIF_INC(netif, ifindiscards);
  }

  return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
static void
ethernetif_input(struct netif *netif, nk_ethernet_packet_t *pk)
{
  struct ethernetif *ethernetif;
  struct eth_hdr *ethhdr;
  struct pbuf *p;

  ethernetif = netif->state;

  /* move received packet into a new pbuf */
  p = low_level_input(netif, pk);

  /* if no packet could be read, silently ignore this */
  if (p != NULL) {
    /* pass all packets to ethernet_input, which decides what packets it supports */
    if (netif->input(p, netif) != ERR_OK) {
      LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
      pbuf_free(p);
      p = NULL;
    }
  }
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
ethernetif_init(struct netif *netif)
{
  struct ethernetif *ethernetif;

  LWIP_ASSERT("netif != NULL", (netif != NULL));
  
  ethernetif = mem_malloc(sizeof(struct ethernetif));
  
  if (ethernetif == NULL) {
    LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: out of memory\n"));
    return ERR_MEM;
  }
#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  /*
   * Initialize the snmp variables and counters inside the struct netif.
   * The last argument should be replaced with your link speed, in units
   * of bits per second.
   */
  
  MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

  ethernetif->name = (char*) netif->state;
  netif->state = ethernetif;
  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
  netif->output = etharp_output;
#if LWIP_IPV6
  netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */
  netif->linkoutput = low_level_output;
  //printk("netif addr in ethernetif: %p\n", netif);
  //ethernetif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);

  /* initialize the hardware */
  
  low_level_init(netif);
  return ERR_OK;
}

#endif /* 0 */
