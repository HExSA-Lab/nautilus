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
 * Copyright (c) 2018, Kyle Hale <khale@cs.iit.edu>
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle Hale <khale@cs.iit.edu>
 *         
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/shell.h>

#ifdef NAUT_CONFIG_NET_ETHERNET
#include <net/ethernet/ethernet_agent.h>
#include <net/ethernet/ethernet_arp.h>
#endif

#ifdef NAUT_CONFIG_NET_LWIP
#include <net/lwip/lwip.h>
#endif

static int 
handle_net (char * buf, void * priv)
{
    char agentname[100], intname[100];
    uint32_t type;
    uint32_t ip[4];
    uint32_t gw[4];
    uint32_t netmask[4];
    uint32_t dns[4];
    int defaults=0;

#ifdef NAUT_CONFIG_NET_ETHERNET
    if (sscanf(buf,"net agent create %s %s",agentname, intname)==2) {
        nk_vc_printf("Attempt to create ethernet agent %s for device %s\n",agentname, intname);
        struct nk_net_dev *netdev = nk_net_dev_find(intname);
        struct nk_net_ethernet_agent *agent;

        if (!netdev) {
            nk_vc_printf("Cannot find device %s\n",intname);
            return 0;
        }

        agent = nk_net_ethernet_agent_create(netdev, agentname, 64, 64);

        if (!agent) {
            nk_vc_printf("Cannot create agent %s\n",agentname);
            return 0;
        }

        nk_vc_printf("agent created\n");
        return 0;
    }


    if (sscanf(buf,"net agent add %s %x",agentname, &type)==2) {
        nk_vc_printf("Attempt to create interface for agent %s type %us\n", agentname,type);
        struct nk_net_ethernet_agent *agent = nk_net_ethernet_agent_find(agentname);
        struct nk_net_dev *netdev;
        if (!agent) {
            nk_vc_printf("Cannot find agent %s\n", agentname);
            return 0;
        }
        netdev = nk_net_ethernet_agent_register_type(agent,type);
        if (!netdev) {
            nk_vc_printf("Failed to register for type\n");
            return 0;
        }
        nk_vc_printf("New netdev is named: %s\n", netdev->dev.name);
        return 0;
    }

    if (sscanf(buf,"net agent s%s %s",buf,agentname)==2) {
        struct nk_net_ethernet_agent *agent = nk_net_ethernet_agent_find(agentname);
        if (!agent) {
            nk_vc_printf("Cannot find agent %s\n", agentname);
            return 0;
        }
        if (buf[0]=='t' && buf[1]=='o') {
            if (nk_net_ethernet_agent_stop(agent)) {
                nk_vc_printf("Failed to to stop agent %s\n", agentname);
            } else {
                nk_vc_printf("Stopped agent %s\n", agentname);
            }
        } else if (buf[0]=='t' && buf[1]=='a') {
            if (nk_net_ethernet_agent_start(agent)) {
                nk_vc_printf("Failed to to start agent %s\n", agentname);
            } else {
                nk_vc_printf("Started agent %s\n", agentname);
            }
        } else {
            nk_vc_printf("Unknown agent command\n");
        }
        return 0;
    }



    if (sscanf(buf,"net arp create %s %u.%u.%u.%u",agentname,&ip[0],&ip[1],&ip[2],&ip[3])==5) {
        ipv4_addr_t ipv4 = ip[0]<<24 | ip[1]<<16 | ip[2]<<8 | ip[3];
        struct nk_net_ethernet_agent *agent = nk_net_ethernet_agent_find(agentname);
        if (!agent) {
            nk_vc_printf("Cannot find agent %s\n", agentname);
            return 0;
        }
        struct nk_net_dev *netdev = nk_net_ethernet_agent_get_underlying_device(agent);
        struct nk_net_dev_characteristics c;
        struct nk_net_ethernet_arper *arper = nk_net_ethernet_arper_create(agent);
        if (!arper) {
            nk_vc_printf("Failed to create arper\n");
            return 0;
        }

        nk_net_dev_get_characteristics(netdev,&c);

        if (nk_net_ethernet_arper_add(arper,ipv4,c.mac)) {
            nk_vc_printf("Failed to add ip->mac mapping\n");
            return 0;
        }
        nk_vc_printf("Added ip->mac mapping\n");
        return 0;
    }      

    if (sscanf(buf,"net arp ask %u.%u.%u.%u",&ip[0],&ip[1],&ip[2],&ip[3])==4) {
        ipv4_addr_t ipv4 = ip[0]<<24 | ip[1]<<16 | ip[2]<<8 | ip[3];
        ethernet_mac_addr_t mac;
        int rc = nk_net_ethernet_arp_lookup(0,ipv4,mac);
        if (rc<0) {
            nk_vc_printf("Lookup failed\n");
        } else if (rc==0) {
            nk_vc_printf("Lookup of %u.%u.%u.%u is %02x:%02x:%02x:%02x:%02x:%02x\n",
                    ip[0],ip[1],ip[2],ip[3], mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
        } else {
            nk_vc_printf("Lookup is in progress\n");
        }
        return 0;
    }

#endif

#ifdef NAUT_CONFIG_NET_LWIP
    if (sscanf(buf,"net lwip start %s", intname)==1 && !strncasecmp(intname,"defaults",8)) {

        struct nk_net_lwip_config conf = { .dns_ip = 0x08080808 } ;

        if (nk_net_lwip_start(&conf)) {
            nk_vc_printf("Failed to start lwip\n");
            return 0;
        }

        struct nk_net_lwip_interface inter = { .name = "virtio-net0",
            .ipaddr = 0x0a0a0a0a,
            .netmask = 0xffffff00,
            .gateway = 0x0a0a0a01 };


        if (nk_net_lwip_add_interface(&inter)) {
            nk_vc_printf("Failed to add interface\n");
            return 0;
        }

        nk_vc_printf("LWIP started with defaults and default interface added\n");
        return 0;
    }

    if (sscanf(buf,"net lwip start %u.%u.%u.%u", &dns[0], &dns[1], &dns[2], &dns[3])==4) {

        struct nk_net_lwip_config conf = { .dns_ip = dns[0]<<24 | dns[1]<<16 | dns[2]<<8 | dns[3] } ;

        if (nk_net_lwip_start(&conf)) {
            nk_vc_printf("Failed to start lwip\n");
            return 0;
        }

        nk_vc_printf("LWIP started\n");
        return 0;

    }


    if (sscanf(buf,"net lwip add %s %u.%u.%u.%u %u.%u.%u.%u %u.%u.%u.%u\n",
                &intname,
                &ip[0], &ip[1], &ip[2], &ip[3],
                &netmask[0], &netmask[1], &netmask[2], &netmask[3],
                &gw[0], &gw[1], &gw[2], &gw[3])==13) {

        struct nk_net_lwip_interface inter;

        strncpy(inter.name,intname,DEV_NAME_LEN); inter.name[DEV_NAME_LEN-1]=0;
        inter.ipaddr = ip[0]<<24 | ip[1]<<16 | ip[2]<<8 | ip[3];
        inter.netmask = netmask[0]<<24 | netmask[1]<<16 | netmask[2]<<8 | netmask[3];
        inter.gateway = gw[0]<<24 | gw[1]<<16 | gw[2]<<8 | gw[3];


        if (nk_net_lwip_add_interface(&inter)) {
            nk_vc_printf("Failed to add interface\n");
            return 0;
        }

        nk_vc_printf("added interface %s ip=%u.%u.%u.%u netmask=%u.%u.%u.%u gw=%u.%u.%u.%u\n",
                intname,ip[0],ip[1],ip[2],ip[3],netmask[0],netmask[1],netmask[2],netmask[3],
                gw[0],gw[1],gw[2],gw[3]);

        return 0;
    }

#ifdef NAUT_CONFIG_NET_LWIP_APP_ECHO
    if (!strcasecmp(buf,"net lwip echo")) {
        void echo_init();
        nk_vc_printf("Starting echo server (port 7)\n");
        echo_init();
        return 0;
    }
#endif

#ifdef NAUT_CONFIG_NET_LWIP_APP_HTTPD
    if (!strcasecmp(buf,"net lwip httpd")) {
        void httpd_init();
        nk_vc_printf("Starting httpd (port 80)\n");
        httpd_init();
        return 0;
    }
#endif

#ifdef NAUT_CONFIG_NET_LWIP_APP_LWIP_SHELL
    if (!strcasecmp(buf,"net lwip shell")) {
        void shell_init();
        nk_vc_printf("Starting shell server (port 23)\n");
        shell_init();
        return 0;
    }
#endif

#ifdef NAUT_CONFIG_NET_LWIP_APP_LWIP_SOCKET_ECHO
    if (!strcasecmp(buf,"net lwip socket_echo")) {
        void socket_echo_init();
        nk_vc_printf("Starting socket_echo (port 7)\n");
        socket_echo_init();
        return 0;
    }
#endif

#ifdef NAUT_CONFIG_NET_LWIP_APP_SOCKET_EXAMPLES
    if (!strcasecmp(buf,"net lwip socket_examples")) {
        void socket_examples_init();
        nk_vc_printf("Starting socket_examples (outgoing)\n");
        socket_examples_init();
        return 0;
    }
#endif

#ifdef NAUT_CONFIG_NET_LWIP_APP_LWIP_IPVCD
    if (!strcasecmp(buf,"net lwip ipvcd")) {
        void ipvcd_init();
        nk_vc_printf("Starting Virtual Console Daemon (port 23)\n");
        ipvcd_init();
        return 0;
    }
#endif

#endif

    nk_vc_printf("No relevant network functionality is configured or bad command\n");

    return 0;
}

static struct shell_cmd_impl net_impl = {
    .cmd      = "net",
    .help_str = "net ...",
    .handler  = handle_net,
};
nk_register_shell_cmd(net_impl);
