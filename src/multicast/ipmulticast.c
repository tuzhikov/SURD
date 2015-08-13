/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include <stdio.h>
#include "../tnkernel/tn.h"
#include "../lwip/lwiplib.h"
#include "../version.h"
#include "../pref.h"
#include "../pins.h"
#include "cmd_ch.h"
#include "ipmulticast.h"
#include "../adcc.h"

#include "../debug/debug.h"

#define STAT_TIMEOUT    2000

static TN_TCB task_ipmulticast_tcb;
#pragma location = ".noinit"
#pragma data_alignment=8
static unsigned int task_ipmulticast_stk[TASK_IPMULTICAST_STK_SZ];
static void task_ipmulticast_func(void* param);

static struct ip_addr   g_multicast_ip;

BOOL if_ready = FALSE;

BOOL If_ready(void)
{
    return if_ready;
}

void ipmulticast_init()
{
    cmd_ch_init();


    dbg_printf("Initializing IPv4 Multicast...");

    if (tn_task_create(&task_ipmulticast_tcb, &task_ipmulticast_func, TASK_IPMULTICAST_PRI,
        &task_ipmulticast_stk[TASK_IPMULTICAST_STK_SZ - 1], TASK_IPMULTICAST_STK_SZ, 0,
        TN_TASK_START_ON_CREATION) != TERR_NO_ERR)
    {
        dbg_puts("tn_task_create(&task_ipmulticast_tcb) error");
        goto err;
    }

    dbg_puts("[done]");
    return;

err:
    dbg_trace();
    tn_halt();
}

void get_multicast_ip(struct ip_addr* ipaddr)
{
    *ipaddr = g_multicast_ip;
}

static void task_ipmulticast_func(void* param)
{
    struct eth_addr hwaddr;
    static char ver[96], hwstr[32];
    

    g_multicast_ip.addr = pref_get_long(PREF_L_MULTICAST_IP);

    get_version(ver, sizeof(ver));
    get_hwaddr(&hwaddr);

    snprintf(hwstr, sizeof(hwstr),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        (unsigned)hwaddr.addr[0], (unsigned)hwaddr.addr[1], (unsigned)hwaddr.addr[2], (unsigned)hwaddr.addr[3], (unsigned)hwaddr.addr[4], (unsigned)hwaddr.addr[5]);

    for (;;)
    {
        if_ready = FALSE;
        iface_ready_wait(TN_WAIT_INFINITE);

        struct ip_addr ifaceip;
        get_ipaddr(&ifaceip);

        igmp_joingroup(&ifaceip, &g_multicast_ip);
        
/*
        dbg_printf("igmp_joingroup(%u.%u.%u.%u, %u.%u.%u.%u)\n",
            ip4_addr1(&ifaceip), ip4_addr2(&ifaceip), ip4_addr3(&ifaceip), ip4_addr4(&ifaceip),
            ip4_addr1(&groupip), ip4_addr2(&groupip), ip4_addr3(&groupip), ip4_addr4(&groupip));
*/

        for (;;)
        {
            if_ready = TRUE;

            if (iface_down_wait(STAT_TIMEOUT) != TERR_TIMEOUT)
                break; 


            static char buf[128];
            snprintf(buf, sizeof(buf), "%s,%u,%u,%u,%u,%u,%s\n", hwstr, 1, 0, 100,100, 1, ver);
            udp_cmd_sendstr(buf);
        }
    }
}
