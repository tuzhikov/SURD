/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __LWIPLIB_H__
#define __LWIPLIB_H__

#include "../tnkernel/tn.h"
#include "netif/etharp.h"
#include "lwip/udp.h"

enum net_mode
{
    NET_MODE_STATIC_IP,
    NET_MODE_DHCP
};

void ethernet_init(enum net_mode mode, struct ip_addr* ipaddr, struct ip_addr* netmask, struct ip_addr* gw);
enum net_mode   get_net_mode();

// timeout == 1 .. TN_WAIT_INFINITE
int iface_ready_wait(unsigned long timeout);
int iface_ready_wait_polling();
int iface_down_wait(unsigned long timeout);
int iface_down_wait_polling();

void get_hwaddr(struct eth_addr* hwaddr);
void get_ipaddr(struct ip_addr* ipaddr);
void get_ipmask(struct ip_addr* ipaddr);
void get_ipgw(struct ip_addr* ipaddr);
BOOL arp_find_addr(struct ip_addr* ipaddr, struct eth_addr* hwaddr_out);

BOOL hwaddr_load(struct eth_addr* hwaddr);
BOOL hwaddr_save(struct eth_addr* hwaddr);
void setNetParamerts(struct ip_addr* ipaddr, struct ip_addr* netmask, struct ip_addr* gw);

// call from ethernet interrupt
void lwip_eth_int_processing();

#define ETH_TIMER_EVENT_TICKS   50  // do not change, need for lwip stack

void lwip_timer_event_iset(); // call from sys tick interrupt every ETH_TIMER_EVENT_TICKS ms

#endif // __LWIPLIB_H__
