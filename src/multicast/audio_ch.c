/*****************************************************************************
*
* © 2011 BOLID Security Systems. Written by Alexander Kotikov.
*
*****************************************************************************/

#include "../tnkernel/tn.h"
#include "../ringbuf.h"
#include "../pref.h"
#include "audio_ch.h"
#include "../pins.h"

#include "../debug/debug.h"

#define AUDIO_CH_DEBUG              0
#define STREAM_BITRATE_MAX          160     // in kbps, max for vs1011 in stream mode
#define BITRATE_ADJUSTMENT_TIMEOUT  2000    // in milliseconds
#define BITRATE_ADJUSTMENT_STEP     100     // in bps
#define BITRATE_ADJUSTMENT_PCT      1       // 1 %

// ring buffer for caching data of audio stream
#define RING_BUF_SZ         (32 * 1024)                 // 32 KiB
#define RING_BUF_HALF_SZ    (RING_BUF_SZ / 2)           // 16 KiB

static unsigned char        g_rbuf_raw[RING_BUF_SZ];
static tRingBufObject       g_rbuf;

#define PLAY_BURST_SZ       1024
#define PLAY_STEP_SZ        128
#define PLAY_B_SZ           (RING_BUF_HALF_SZ + PLAY_BURST_SZ)

// audio task tcb and stack

static TN_TCB task_audio_tcb;
#pragma location = ".noinit"
#pragma data_alignment=8
static unsigned int task_audio_stk[TASK_AUDIO_STK_SZ];
static void task_audio_func(void* param);

// local variables

static TN_EVENT         g_timer_evt;
static enum audio_ch    g_act_audio_ch = CH0;
static BOOL             g_playing;
static struct ip_addr   g_security_ipaddr;
static struct ip_addr   g_audio_srv_ipaddr;
static unsigned         g_audio_srv_port_ch0;
static unsigned         g_audio_srv_port_ch1;
static unsigned         g_initial_bitrate;  // in bps (128000 e.t.c.)
static unsigned         g_bitrate;          // in bps (128000 e.t.c.)
static BOOL volatile    g_ch_enabled;

// local functions

static void stream_flush();
static void audio_ch_recv(void* arg, struct udp_pcb* upcb, struct pbuf* p, struct ip_addr* addr, u16_t port);
static BOOL ip_security_check(struct ip_addr* srv_addr);

void audio_ch_init()
{
    dbg_printf("Initializing mpeg stream subsystem...");
    g_ch_enabled = TRUE;    // default enabled

    struct udp_pcb* pcb;

    RingBufInit(&g_rbuf, g_rbuf_raw, RING_BUF_SZ);

    // load audio server IP for ip_security_check() func
    g_security_ipaddr.addr  = pref_get_long(PREF_L_AUDIO_SRV_IP);
    g_audio_srv_port_ch0    = pref_get_long(PREF_L_CH0_PORT);
    g_audio_srv_port_ch1    = pref_get_long(PREF_L_CH1_PORT);

    if (tn_event_create(&g_timer_evt, TN_EVENT_ATTR_SINGLE | TN_EVENT_ATTR_CLR, FALSE) != TERR_NO_ERR)
    {
        dbg_puts("tn_event_create(&g_timer_evt) error");
        goto err;
    }

    if (tn_task_create(&task_audio_tcb, &task_audio_func, TASK_AUDIO_PRI,
        &task_audio_stk[TASK_AUDIO_STK_SZ - 1], TASK_AUDIO_STK_SZ, 0,
        TN_TASK_START_ON_CREATION) != TERR_NO_ERR)
    {
        dbg_puts("tn_task_create(&task_audio_tcb) error");
        goto err;
    }

    // audio channel 0

    pcb = udp_new();

    if (pcb == 0)
    {
        dbg_puts("udp_new(CH0) error");
        goto err;
    }

    if (udp_bind(pcb, IP_ADDR_ANY, g_audio_srv_port_ch0) != ERR_OK)
    {
        dbg_puts("udp_bind(CH0) error");
        goto err;
    }

    udp_recv(pcb, &audio_ch_recv, (void*)CH0);

    // audio channel 1

    pcb = udp_new();

    if (pcb == 0)
    {
        dbg_puts("udp_new(CH1) error");
        goto err;
    }

    if (udp_bind(pcb, IP_ADDR_ANY, g_audio_srv_port_ch1) != ERR_OK)
    {
        dbg_puts("udp_bind(CH1) error");
        goto err;
    }

    udp_recv(pcb, &audio_ch_recv, (void*)CH1);

    hw_timer0a_init(&g_timer_evt, TRUE);
    hw_timer0a_set_timeout(8000); // initial value PLAY_STEP_SZ(128) / (bitrate(128000) / 8 / 1000) = 8 ms
    hw_timer0a_start();

    dbg_puts("[done]");
    return;

err:
    dbg_trace();
    tn_halt();
}

BOOL is_audio_ch_playing()
{
    return g_ch_enabled;
}

void audio_ch_disable()
{
    g_ch_enabled = FALSE;
    stream_flush();
}

BOOL audio_ch_is_enabled()
{
    return g_ch_enabled;
}

void audio_ch_enable()
{
    stream_flush();
    g_ch_enabled = TRUE;
}

enum audio_ch get_act_audio_ch()
{
    return g_act_audio_ch;
}


void get_bitrate(unsigned* bitrate, unsigned* cbitrate)
{
    TN_INTSAVE_DATA
    tn_disable_interrupt();

    if (bitrate != NULL)
        *bitrate = g_initial_bitrate;

    if (cbitrate != NULL)
        *cbitrate = g_bitrate;

    tn_enable_interrupt();
}

BOOL get_audio_srv_ip(struct ip_addr* srv_addr)
{
    BOOL const rc = g_playing || (/*g_ch_enabled == FALSE && */g_audio_srv_ipaddr.addr != IP_ADDR_ANY->addr);
    if (rc)
        *srv_addr = g_audio_srv_ipaddr;
    return rc;
}

unsigned get_audio_srv_port_ch0()
{
    return g_audio_srv_port_ch0;
}

unsigned get_audio_srv_port_ch1()
{
    return g_audio_srv_port_ch1;
}

// local functions

static void task_audio_func(void* param)
{

    for (;;)
    {
        unsigned pattern;
        int rc = tn_event_wait(&g_timer_evt, TRUE, TN_EVENT_WCOND_OR, &pattern, TN_WAIT_INFINITE);
        if (rc != TERR_NO_ERR)
        {
            dbg_puts("tn_event_wait(&g_timer_evt) != TERR_NO_ERR");
            dbg_trace();
            tn_halt();
        }
    }
}

static void stream_flush()
{
    RingBufFlush(&g_rbuf);
    g_initial_bitrate   = 0;
    g_bitrate           = 0;
    g_playing           = FALSE;
    dbg_puts("Stream stopped");
}



static void audio_ch_recv(void* arg, struct udp_pcb* upcb, struct pbuf* p, struct ip_addr* addr, u16_t port)
{
    if (g_ch_enabled == FALSE || (int)arg != g_act_audio_ch || ip_security_check(addr) == FALSE)
    {
        pbuf_free(p);
        return;
    }

    if (p->next != NULL)
    {
        pbuf_free(p);
        dbg_puts("Too long audio packet");
        dbg_trace();
        return;
    }

    // set audio server IP address
    if (g_audio_srv_ipaddr.addr != addr->addr)
        g_audio_srv_ipaddr.addr = addr->addr;

    pbuf_free(p);
}

static BOOL ip_security_check(struct ip_addr* srv_addr)
{
    if (g_security_ipaddr.addr == IP_ADDR_ANY->addr)
        return TRUE;

    return g_security_ipaddr.addr == srv_addr->addr;
}
