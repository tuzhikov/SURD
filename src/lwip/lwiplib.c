/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include <string.h> // memset(), memcpy()
#include "../stellaris.h"
#include "lwip/opt.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/ip.h"
#include "lwip/dhcp.h"
#include "lwip/tcp.h"
#include "lwip/autoip.h"
#include "lwip/ip_frag.h"
#include "lwip/dns.h"
#include "netif/etharp.h"
#include "netif/stellarisif.h"
#include "lwiplib.h"
#include "../pins.h"
#include "../pref.h"

#include "../debug/debug.h"

#define EVENT_SIG_STATE     TRUE
#define EVENT_ETH_TIMER     1
#define EVENT_ETH_DATA      2

static struct netif     g_iface1;
static TN_EVENT         g_eth_evt;
static TN_EVENT         g_iface_ready_evt;
static TN_EVENT         g_iface_down_evt;
static enum net_mode    g_net_mode = NET_MODE_STATIC_IP;

// ethernet task
static TN_TCB g_task_eth_tcb;
#pragma location = ".noinit"
#pragma data_alignment=8
static unsigned int task_eth_stk[TASK_ETH_STK_SZ];
static void task_eth_func(void* param);

static void internal_ethernet_init(struct netif* iface);
static err_t iface_init(struct netif* iface);
static void lwip_timers_processing(struct netif* iface, unsigned int time_step);
static BOOL is_eth_link_active();
static void eth_link_processing(struct netif* iface);
static void iface_up(struct netif* iface);
static void iface_down(struct netif* iface);
static void iface_ready_callback(struct netif* iface, BOOL ready);
static void debug_print_iface_info(struct netif* iface);

void ethernet_init(enum net_mode mode, struct ip_addr* ipaddr, struct ip_addr* netmask, struct ip_addr* gw)
{
    dbg_printf("Initializing Ethernet...");

    memset(&g_iface1, 0, sizeof(g_iface1));

    if (mode == NET_MODE_STATIC_IP)
    {
        if (ipaddr->addr == 0 || netmask->addr == 0)
        {
            dbg_puts("eth_mode == ETH_MODE_STATIC_IP, ip zero check error");
            goto err;
        }
    }
    else if (mode == NET_MODE_DHCP) // nothing
    {
    }
    else
    {
        dbg_puts("eth_mode == ETH_MODE_UNKNOWN");
        goto err;
    }

    g_net_mode = mode;

    if (g_net_mode == NET_MODE_STATIC_IP)
        netif_set_addr(&g_iface1, ipaddr, netmask, gw);
    else
        netif_set_addr(&g_iface1, 0, 0, 0);

    internal_ethernet_init(&g_iface1);

    if (tn_event_create(&g_eth_evt, TN_EVENT_ATTR_SINGLE | TN_EVENT_ATTR_CLR, 0) != TERR_NO_ERR)
    {
        dbg_puts("tn_event_create(&g_eth_evt) error");
        goto err;
    }

    if (tn_event_create(&g_iface_ready_evt, TN_EVENT_ATTR_MULTI, 0) != TERR_NO_ERR)
    {
        dbg_puts("tn_event_create(&g_iface_ready_evt) error");
        goto err;
    }

    if (tn_event_create(&g_iface_down_evt, TN_EVENT_ATTR_MULTI, EVENT_SIG_STATE) != TERR_NO_ERR)
    {
        dbg_puts("tn_event_create(&g_iface_down_evt) error");
        goto err;
    }

    if (tn_task_create(&g_task_eth_tcb, &task_eth_func, TASK_ETH_PRI, &task_eth_stk[TASK_ETH_STK_SZ - 1],
        TASK_ETH_STK_SZ, 0, TN_TASK_START_ON_CREATION) != TERR_NO_ERR)
    {
        dbg_puts("tn_task_create(&g_task_eth_tcb) error");
        goto err;
    }

    dbg_puts("[done]");
    return;

err:
    dbg_trace();
    tn_halt();
}

enum net_mode get_net_mode()
{
    return g_net_mode;
}

int iface_ready_wait(unsigned long timeout)
{
    unsigned int fpattern;
    return tn_event_wait(&g_iface_ready_evt, EVENT_SIG_STATE, TN_EVENT_WCOND_AND, &fpattern, timeout);
}

int iface_ready_wait_polling()
{
    unsigned int fpattern;
    return tn_event_wait_polling(&g_iface_ready_evt, EVENT_SIG_STATE, TN_EVENT_WCOND_AND, &fpattern);
}

int iface_down_wait(unsigned long timeout)
{
    unsigned int fpattern;
    return tn_event_wait(&g_iface_down_evt, EVENT_SIG_STATE, TN_EVENT_WCOND_AND, &fpattern, timeout);
}

int iface_down_wait_polling()
{
    unsigned int fpattern;
    return tn_event_wait_polling(&g_iface_down_evt, EVENT_SIG_STATE, TN_EVENT_WCOND_AND, &fpattern);
}

void get_hwaddr(struct eth_addr* hwaddr)
{
    // lock not need, g_iface1.hwaddr is not changing
    memcpy((void*)hwaddr->addr, (void*)g_iface1.hwaddr, ETHARP_HWADDR_LEN);
}

void get_ipaddr(struct ip_addr* ipaddr)
{
    TN_INTSAVE_DATA
    tn_disable_interrupt();
    *ipaddr = g_iface1.ip_addr;
    tn_enable_interrupt();
}

void get_ipmask(struct ip_addr* ipaddr)
{
    TN_INTSAVE_DATA
    tn_disable_interrupt();
    *ipaddr = g_iface1.netmask;
    tn_enable_interrupt();
}

void get_ipgw(struct ip_addr* ipaddr)
{
    TN_INTSAVE_DATA
    tn_disable_interrupt();
    *ipaddr = g_iface1.gw;
    tn_enable_interrupt();
}

BOOL arp_find_addr(struct ip_addr* ipaddr, struct eth_addr* hwaddr_out)
{
    struct eth_addr* hwaddr_o;
    struct ip_addr* ipaddr_o;

    TN_INTSAVE_DATA
    tn_disable_interrupt();

    if (etharp_find_addr(&g_iface1, ipaddr, &hwaddr_o, &ipaddr_o) == -1)
    {
        tn_enable_interrupt();
        return FALSE;
    }
    else
    {
        memcpy(hwaddr_out, hwaddr_o, ETHARP_HWADDR_LEN);
        tn_enable_interrupt();
        return TRUE;
    }
}

BOOL hwaddr_load(struct eth_addr* hwaddr)
{
    *((unsigned long*)&hwaddr->addr[0]) = pref_get_long(PREF_L_MAC_1);
    *((unsigned short*)&hwaddr->addr[4]) = (unsigned short)pref_get_long(PREF_L_MAC_2);

    if (*((unsigned long*)&hwaddr->addr[0]) != 0xFFFFFFFF || *((unsigned short*)&hwaddr->addr[4]) != 0xFFFF)
        return *((unsigned long*)&hwaddr->addr[0]) | *((unsigned short*)&hwaddr->addr[4]);

    return FALSE;

/*
    unsigned long user0;
    unsigned long user1;

    if (MAP_FlashUserGet(&user0, &user1) != 0)
        return FALSE;

    if (user0 == ~0 || user1 == ~0)
        return FALSE;

    hwaddr->addr[0] = user0 >> 0 & 0xFF;
    hwaddr->addr[1] = user0 >> 8 & 0xFF;
    hwaddr->addr[2] = user0 >> 16 & 0xFF;
    hwaddr->addr[3] = user1 >> 0 & 0xFF;
    hwaddr->addr[4] = user1 >> 8 & 0xFF;
    hwaddr->addr[5] = user1 >> 16 & 0xFF;

    return TRUE;
*/
}

BOOL hwaddr_save(struct eth_addr* hwaddr)
{
    unsigned long user0;
    unsigned long user1;

    if (MAP_FlashUserGet(&user0, &user1) != 0)
        return FALSE;

    if (user0 == ~0 && user1 == ~0)
    {
        user0 = hwaddr->addr[0] | hwaddr->addr[1] << 8 | hwaddr->addr[2] << 16;
        user1 = hwaddr->addr[3] | hwaddr->addr[4] << 8 | hwaddr->addr[5] << 16;
        return MAP_FlashUserSet(user0, user1) == 0 && MAP_FlashUserSave() == 0;
    }

    return FALSE;
}

// local functions

static void task_eth_func(void* param)
{
    dbg_puts("Waiting link...");

    for (;;)
    {
        unsigned int fpattern;
        tn_event_wait(&g_eth_evt, EVENT_ETH_TIMER | EVENT_ETH_DATA, TN_EVENT_WCOND_OR, &fpattern, TN_WAIT_INFINITE);

        if (fpattern & EVENT_ETH_TIMER)
        {
            eth_link_processing(&g_iface1);
            lwip_timers_processing(&g_iface1, ETH_TIMER_EVENT_TICKS);
        }

        if (fpattern & EVENT_ETH_DATA)
        {
            stellarisif_interrupt(&g_iface1);
            MAP_EthernetIntEnable(ETH_BASE, ETH_INT_RX | ETH_INT_TX);
        }
    }
}

void lwip_eth_int_processing()
{
    unsigned long const eth_int_state = MAP_EthernetIntStatus(ETH_BASE, 0);
    MAP_EthernetIntClear(ETH_BASE, eth_int_state);

    if (eth_int_state)
    {
        tn_event_iset(&g_eth_evt, EVENT_ETH_DATA);
        MAP_EthernetIntDisable(ETH_BASE, ETH_INT_RX | ETH_INT_TX);
    }
}

void lwip_timer_event_iset()
{
    tn_event_iset(&g_eth_evt, EVENT_ETH_TIMER);
}

static void internal_ethernet_init(struct netif* iface)
{
    TN_INTSAVE_DATA
    tn_disable_interrupt();

    lwip_init();
    netif_add(iface, &iface->ip_addr, &iface->netmask, &iface->gw, 0, &iface_init, &ip_input);
    netif_set_default(iface);

    tn_enable_interrupt();
}

static err_t iface_init(struct netif* iface)
{
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_ETH);
    MAP_SysCtlPeripheralReset(SYSCTL_PERIPH_ETH);

    // Ethernet LEDs configuration
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    MAP_GPIOPinConfigure(GPIO_PF2_LED1);
    MAP_GPIOPinConfigure(GPIO_PF3_LED0);
    MAP_GPIOPinTypeEthernetLED(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_3);
//    HWREG(ETH_BASE + MAC_O_LED) = 0x001;    // LED1{11:8} 0 - Link OK; LED0{3:0} 1 - RX or TX Activity
                                            // lm3s9b90 ethernet led's bug

    // loading real MAC from user flash registers if programmed
    if (!hwaddr_load((struct eth_addr*)iface->hwaddr))
    {
        // default MAC 00:1A:B6:00:00:00 (00:1A:B6 - Texas Instruments)
        iface->hwaddr[0] = 0x00;
        iface->hwaddr[1] = 0x1A;
        iface->hwaddr[2] = 0xB6;
        iface->hwaddr[3] = 0x00;
        iface->hwaddr[4] = 0x64;
        iface->hwaddr[5] = 0x00;
    }

    MAP_EthernetMACAddrSet(ETH_BASE, iface->hwaddr);
    iface->hwaddr_len = ETHARP_HWADDR_LEN;
    iface->mtu = 1500;
    iface->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    return stellarisif_init(iface);
}

static void lwip_timers_processing(struct netif* iface, unsigned int time_step)
{
    static unsigned tm = 0;

    if (tm == DHCP_COARSE_TIMER_MSECS) // max timeout
        tm = 0;

    tm += time_step;

#if LWIP_ARP
    if (tm % ARP_TMR_INTERVAL == 0)
        etharp_tmr(); // 5 sec
#endif // LWIP_ARP

#if LWIP_TCP
    if (tm % TCP_TMR_INTERVAL == 0)
        tcp_tmr(); // 250 ms
#endif // LWIP_TCP

#if LWIP_AUTOIP
    if (tm % AUTOIP_TMR_INTERVAL == 0)
        autoip_tmr(); // 100 ms
#endif // LWIP_AUTOIP

#if LWIP_DHCP
    if (tm % DHCP_COARSE_TIMER_MSECS == 0)
        dhcp_coarse_tmr(); // 60 sec

    if (tm % DHCP_FINE_TIMER_MSECS == 0)
        dhcp_fine_tmr(); // 500 ms
#endif // LWIP_DHCP

#if IP_REASSEMBLY
    if (tm % IP_TMR_INTERVAL == 0)
        ip_reass_tmr(); // 1 sec
#endif // IP_REASSEMBLY

#if LWIP_IGMP
    if (iface->flags & NETIF_FLAG_IGMP && tm % IGMP_TMR_INTERVAL == 0)
        igmp_tmr(); // 100 ms
#endif // LWIP_IGMP

#if LWIP_DNS
    if (tm % DNS_TMR_INTERVAL == 0)
        dns_tmr(); // 1 sec
#endif // LWIP_DNS

}

static BOOL is_eth_link_active()
{
    return (MAP_EthernetPHYRead(ETH_BASE, PHY_MR1) & PHY_MR1_LINK) == PHY_MR1_LINK;
}

static void eth_link_processing(struct netif* iface)
{
    TN_INTSAVE_DATA;

    static struct ip_addr last_ipaddr = {0};

    if (last_ipaddr.addr != iface->ip_addr.addr)
    {
        last_ipaddr.addr = iface->ip_addr.addr;
        debug_print_iface_info(iface);
    }

    static BOOL prev_link_active = FALSE;
    BOOL const link_active = is_eth_link_active();

    if (!prev_link_active && link_active)
    {
        tn_disable_interrupt();

        prev_link_active = TRUE;
        iface_up(iface);

        tn_enable_interrupt();

        dbg_puts("Link is established");
    }
    else if (prev_link_active && !link_active)
    {
        tn_disable_interrupt();

        prev_link_active = FALSE;
        iface_down(iface);

        tn_enable_interrupt();

        dbg_puts("Waiting link...");
    }

    // for iface_ready_wait() & iface_ready_callback() functions
    static BOOL iface_ready = FALSE;

    if (!iface_ready && link_active && iface->ip_addr.addr != 0)
    {
        // iface ready (link_active == TRUE && ip != 0)
        iface_ready_callback(iface, iface_ready = TRUE);
        tn_event_clear(&g_iface_down_evt, 0);
        tn_event_set(&g_iface_ready_evt, EVENT_SIG_STATE);
    }
    else if (iface_ready && (!link_active || iface->ip_addr.addr == 0))
    {
        // iface down (link_active == FALSE || ip == 0)
        iface_ready_callback(iface, iface_ready = FALSE);
        tn_event_clear(&g_iface_ready_evt, 0);
        tn_event_set(&g_iface_down_evt, EVENT_SIG_STATE);
    }
}

static void iface_up(struct netif* iface)
{
#if LWIP_DHCP
    if(g_net_mode == NET_MODE_DHCP)
        dhcp_start(iface);
#endif // LWIP_DHCP

    netif_set_up(iface);
}

static void iface_down(struct netif* iface)
{
#if LWIP_DHCP
    dhcp_stop(iface);
#endif // LWIP_DHCP

    netif_set_down(iface);

    if (g_net_mode == NET_MODE_DHCP)
        netif_set_addr(iface, 0, 0, 0);
}

static void iface_ready_callback(struct netif* iface, BOOL ready)
{

#if LWIP_IGMP

    // Управляем IGMP подсистемой вручную, для корректной регистрации в группах
    // после получения IP адреса по DHCP
    if (ready)
    {
        iface->flags |= NETIF_FLAG_IGMP;
        igmp_start(iface);
        igmp_report_groups(iface);
    }
    else
    {
        igmp_stop(iface);
        iface->flags &= ~NETIF_FLAG_IGMP;
    }

#endif // LWIP_IGMP

}

static void debug_print_iface_info(struct netif* iface)
{
    dbg_puts("--------------------");
    dbg_printf("IP : %d.%d.%d.%d\n", ip4_addr1(&iface->ip_addr), ip4_addr2(&iface->ip_addr), ip4_addr3(&iface->ip_addr), ip4_addr4(&iface->ip_addr));
    dbg_printf("MSK: %d.%d.%d.%d\n", ip4_addr1(&iface->netmask), ip4_addr2(&iface->netmask), ip4_addr3(&iface->netmask), ip4_addr4(&iface->netmask));
    dbg_printf("GW : %d.%d.%d.%d\n", ip4_addr1(&iface->gw), ip4_addr2(&iface->gw), ip4_addr3(&iface->gw), ip4_addr4(&iface->gw));
    dbg_puts("--------------------");
}
