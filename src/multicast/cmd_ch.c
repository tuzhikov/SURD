/*****************************************************************************
*
* � 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include <string.h> // memset(), memcpy()
#include <stdio.h>
#include "../tnkernel/tn.h"
#include "../pref.h"
#include "cmd_fn.h"
#include "cmd_ch.h"
#include "../pins.h"
#include "../dk/dk.h"
#include "../vpu.h"
#include "../event/evt_fifo.h"
#include "../debug/debug.h"
#include "../dk/dk.h"

/*----------------------------------------------------------------------------*/
/**/
/*----------------------------------------------------------------------------*/
// cmd's fifo
static TN_DQUE g_cmd_dque;
static void* g_cmd_fifo[CMD_DQUE_SZ];
// cmd's list item info
struct cmd_nfo g_cmd_nfo[CMD_NFO_SZ]; // global, used in cmd_fn.c in cmd_help_func()
// cmd's argument vector
static char* g_argv[MAX_ARGV_SZ];
// command channel task
static TN_TCB task_cmd_tcb;
static TN_TCB task_cmd_LED_tcb;

#pragma location = ".noinit"
#pragma data_alignment=8

static unsigned int task_cmd_stk[TASK_CMD_STK_SZ];
static unsigned int task_cmd_LED_stk[TASK_CMD_STK_SZ];

static void task_cmd_func(void* param);
static void task_cmd_LED_func(void* param);

static struct udp_pcb*  g_cmd_ch_pcb;
static u16_t            g_cmd_ch_port;
static struct ip_addr   g_security_ipaddr;

// local functions -----------------------------------------------------------//
static err_t udp_native_sendbuf(struct udp_pcb* pcb, char const* buf, int buf_sz, struct ip_addr* raddr, u16_t rport);
static void create_cmd_nfo_list();

static struct cmd_raw*  cmd_alloc(struct udp_pcb* upcb, struct pbuf* buf_p, struct ip_addr* addr, u16_t port);
static void             cmd_free(struct cmd_raw* cmd_p);

static void cmd_recv(void* arg, struct udp_pcb* upcb, struct pbuf* buf_p, struct ip_addr* addr, u16_t port);
static void cmd_processing(struct cmd_raw* cmd_p);
static BOOL cmd_nfo_push_back(char* cmd_txt, void (*cmd_func)(struct cmd_raw* cmd_p, int argc, char** argv), char* cmd_help);
static struct cmd_nfo* find_cmd_nfo(char* cmd_txt);
static BOOL ip_security_check(struct ip_addr* srv_addr);

BOOL ETH_RECV_FLAG;

#define CMD_NFO_PUSH_BACK(t, fn, help)    \
    if (!cmd_nfo_push_back(t, fn, help))  \
    {                               \
        dbg_printf(emsg, t);        \
        goto err;                   \
    }
/*----------------------------------------------------------------------------*/
/* */
/*----------------------------------------------------------------------------*/
// ������� ���� ������
static void create_cmd_nfo_list()
{
    static char const emsg[] = "cmd_nfo_push_back(\"%s\") error";

    // fill supported cmd's list
    // max cmd's count == CMD_NFO_SZ

    //CMD_NFO_PUSH_BACK("debugee", &cmd_debugee_func, "");
    //commands net udp
    CMD_NFO_PUSH_BACK("allpollingdk",&cmd_polling_func,"");
    CMD_NFO_PUSH_BACK("setphaseudp",&cmd_setphase_func,"");
    CMD_NFO_PUSH_BACK("getstatussurd",&cmd_getsatus_func,"");
    CMD_NFO_PUSH_BACK("SUCCESS:",&cmd_answer_surd_func,""); // ��������� ������
    //commands configurator
    CMD_NFO_PUSH_BACK("ifconfig", &cmd_ifconfig_func,"");
    CMD_NFO_PUSH_BACK("config", &cmd_config_func, "");

    CMD_NFO_PUSH_BACK("get", &cmd_get_func, " [ver] [sensors] [lines] [linePWM] [event] [project] [state], [tvp], [gps_time], [gps_info], [ds1390_time], [light], [info], [lineADC]\n");

    CMD_NFO_PUSH_BACK("set", &cmd_set_func, " [init] ALL/memory"
                                            " [tvp] X"
                                            " [test] on/off"
                                            " [rele] on/off"
                                            " [ip] XX.XX.XX.XX\n"
                                            " [mask] XX.XX.XX.XX, [gw] XX.XX.XX.XX\n"
                                            " [cmdSrvIP] XX.XX.XX.XX\n"
                                            " [cmdPort] X, [dhcp] on/off\n"
                                            " [hwaddr] XX:XX:XX:XX:XX:XX\n"
                                            " [ds1390_time] XX.XX.XX-XX:XX:XX\n"
                                            " [work] start/stop\n"
                                            " [light_r], [light_y], [light_g]"
                                            " [VPU] ��\n"  );

    CMD_NFO_PUSH_BACK("light", &cmd_light_func, " [faza] XX, [OS], [YF], [KK],[undo]");
    //
    CMD_NFO_PUSH_BACK("reboot", &cmd_reboot_func, "");
    CMD_NFO_PUSH_BACK("flash_reset", &cmd_prefReset_func, " 255 ");

    CMD_NFO_PUSH_BACK("flashwr", &cmd_flashwr_func, " addr len data");  // ������ �� FLASH �� ������������� ������
    CMD_NFO_PUSH_BACK("pflashwr", &cmd_pflashwr_func, " addr len data");  // ������ ������� �� FLASH �� ������������� ������
    CMD_NFO_PUSH_BACK("proj_flashwr", &cmd_proj_flashwr_func, " addr len data");  // ������ ������� �� FLASH �� ���������� ������
    CMD_NFO_PUSH_BACK("progs_flashwr", &cmd_progs_flashwr_func, " addr len data");  // ������ ������� �� FLASH �� ���������� ������
    //
    CMD_NFO_PUSH_BACK("proj_flashrd", &cmd_proj_flashrd_func, " addr len");  // ������ ������� �� FLASH �� ���������� ������
    CMD_NFO_PUSH_BACK("progs_flashrd", &cmd_progs_flashrd_func, " addr len");  // ������ ������� �� FLASH �� ���������� ������
    //

    CMD_NFO_PUSH_BACK("flashrd", &cmd_flashrd_func, " addr len");       // ������ �� FLASH �� ������������� ������

    CMD_NFO_PUSH_BACK("flash", &cmd_flash_func, " addr len data");
    CMD_NFO_PUSH_BACK("flashNFO", &cmd_flashNFO_func, " crc32 len");

    CMD_NFO_PUSH_BACK("test", &cmd_test_func, ""); // DEBUG
    CMD_NFO_PUSH_BACK("event", &cmd_event_func, ""); // DEBUG
    //
    CMD_NFO_PUSH_BACK("channel", &cmd_chan_func, ""); // DEBUG
    // last cmd
    CMD_NFO_PUSH_BACK("help", &cmd_help_func, "");

    return;

err:
    dbg_trace();
    tn_halt();
}
/*----------------------------------------------------------------------------*/
// ������������� ������ CMD
void cmd_ch_init()
{
    dbg_printf("Initializing UDP command console...");
    //
    pin_on(OPIN_TEST2);
    pin_off(OPIN_TEST2);
    pin_on(OPIN_TEST2);
    // load command server IP address
    g_security_ipaddr.addr = pref_get_long(PREF_L_CMD_SRV_IP);
    g_cmd_ch_port = (u16_t)pref_get_long(PREF_L_CMD_PORT);

    create_cmd_nfo_list(); // �������� ���� ������

    if (tn_queue_create(&g_cmd_dque, (void**)&g_cmd_fifo, CMD_DQUE_SZ) != TERR_NO_ERR)
    {
        dbg_puts("tn_queue_create(&g_cmd_dque) error");
        goto err;
    }
    //
    if (tn_task_create(&task_cmd_tcb, &task_cmd_func, TASK_CMD_PRI,
        &task_cmd_stk[TASK_CMD_STK_SZ - 1], TASK_CMD_STK_SZ, 0,
        TN_TASK_START_ON_CREATION) != TERR_NO_ERR)
    {
        dbg_puts("tn_task_create(&task_cmd_tcb) error");
        goto err;
    }
    //
    if (tn_task_create(&task_cmd_LED_tcb, &task_cmd_LED_func, TASK_CMD_LED_PRI,
        &task_cmd_LED_stk[TASK_CMD_STK_LED_SZ - 1], TASK_CMD_STK_LED_SZ, 0,
        TN_TASK_START_ON_CREATION) != TERR_NO_ERR)
    {
        dbg_puts("tn_task_create(&task_cmd_tcb) error");
        goto err;
    }
    // command's channel
    g_cmd_ch_pcb = udp_new();

    if (g_cmd_ch_pcb == NULL)
    {
        dbg_puts("udp_new() error");
        goto err;
    }
    //������ ���� � ����������������� �����
    if (udp_bind(g_cmd_ch_pcb, IP_ADDR_ANY, g_cmd_ch_port) != ERR_OK)
    {
        dbg_puts("udp_bind() error");
        goto err;
    }
    // ����������� �����������.
    udp_recv(g_cmd_ch_pcb, &cmd_recv, 0);
    dbg_puts("[done]");
    return;

err:
    dbg_trace();
    tn_halt();
}
/*----------------------------------------------------------------------------*/
// �������� ������ ����������������
err_t udp_sendStrBroadcast(char const* str,u16_t rport)
{
return udp_native_sendbuf(g_cmd_ch_pcb,str,strlen(str),IP_ADDR_BROADCAST,rport);// ����������������� ������
}
// ��������� ������ �� UDP ---------------------------------------------------//
err_t udp_sendstr(struct cmd_raw* cmd_p, char const* s)
{
    return udp_sendbuf(cmd_p, s, strlen(s));
}
// ��������� ������ �� UDP ---------------------------------------------------//
err_t udp_sendbuf(struct cmd_raw* cmd_p, char const* buf, int len)
{
    return udp_native_sendbuf(cmd_p->upcb, buf, len, cmd_p->raddr, cmd_p->rport);
}
//----------------------------------------------------------------------------//
err_t udp_cmd_sendbuf(char const* buf, int len)
{
    if (g_security_ipaddr.addr == IP_ADDR_ANY->addr)
    {
        return ERR_CONN; // not connected
    }
    else
        return udp_native_sendbuf(g_cmd_ch_pcb, buf, len, &g_security_ipaddr, g_cmd_ch_port);
}
//----------------------------------------------------------------------------//
err_t udp_cmd_sendstr(char const* s)
{
    return udp_cmd_sendbuf(s, strlen(s));
}
// ������ ������ -------------------------------------------------------------//
void get_cmd_ch_ip(struct ip_addr* ipaddr)
{
    *ipaddr = g_security_ipaddr;
}
// ������ �����---------------------------------------------------------------//
unsigned get_cmd_ch_port()
{
    return (unsigned)g_cmd_ch_port;
}
// �������� �� UDP -----------------------------------------------------------//
static err_t udp_native_sendbuf(struct udp_pcb* pcb, char const* buf, int len, struct ip_addr* raddr, u16_t rport)
{
    static unsigned long delay=0;
    static unsigned long sub_time;
    static BOOL f = FALSE;

    while (f == TRUE)
        tn_task_sleep(2);
    f = TRUE;

    sub_time = hw_time_span(delay);
    if (sub_time < 5)
        tn_task_sleep(5-sub_time);

    struct pbuf* buf_p = pbuf_alloc(PBUF_RAW, len, PBUF_ROM);

    buf_p->payload = (void*)buf;
    err_t err = udp_sendto(pcb, buf_p, raddr, rport);

    pbuf_free(buf_p);
    delay = hw_time();
    f = FALSE;

    return err;
}
// ������ ������ ��� ������ --------------------------------------------------//
static struct cmd_raw* cmd_alloc(struct udp_pcb* upcb, struct pbuf* buf_p, struct ip_addr* addr, u16_t port)
{
    if (upcb == NULL || buf_p == NULL || addr == NULL || port == 0)
        return NULL;

    struct pbuf* __buf_p = pbuf_alloc(PBUF_RAW, sizeof(struct pbuf*) + sizeof(struct cmd_raw), PBUF_RAM);

    if (__buf_p == NULL)
        return NULL;

    // save real pbuf ptr
    *(struct pbuf**)__buf_p->payload = __buf_p;

    struct cmd_raw* cmd_p = (struct cmd_raw*)((struct pbuf**)__buf_p->payload + 1);

    cmd_p->upcb = upcb;
    cmd_p->buf_p = buf_p;
    cmd_p->raddr = addr;
    cmd_p->rport = port;

    return cmd_p;
}
// ������ ������ -------------------------------------------------------------//
static void cmd_free(struct cmd_raw* cmd_p)
{
    if (cmd_p != NULL)
    {
        if (cmd_p->buf_p == NULL)
        {
            dbg_puts("cmd_p->buf_p == 0 error");
            goto err;
        }

        pbuf_free(cmd_p->buf_p);

        // restore real pbuf ptr
        struct pbuf* __buf_p = *((struct pbuf**)cmd_p - 1);

        if (__buf_p == NULL)
        {
            dbg_puts("real pbuf ptr == 0 error");
            goto err;
        }

        pbuf_free(__buf_p);
    }

    return;

err:
    dbg_trace();
    tn_halt();
}
// ���������� � ������ ������ ------------------------------------------------//
static void cmd_recv(void* arg, struct udp_pcb* upcb, struct pbuf* p, struct ip_addr* addr, u16_t port)
{
    if (ip_security_check(addr) == FALSE)
    {
        pbuf_free(p);
        return;
    }

    // port for answers on server is always equal pref_get_long(PREF_L_CMD_PORT), default 11990
    struct cmd_raw* cmd_p = cmd_alloc(upcb, p, addr, g_cmd_ch_port);
    ////////////////////
    ETH_RECV_FLAG = TRUE;
    if (cmd_p == NULL)
    {
        dbg_puts("cmd_alloc() error");
        goto err;
    }

    if (tn_queue_send_polling(&g_cmd_dque, cmd_p) != TERR_NO_ERR)
    {
        static char const msg[] = "ERR: Command's queue is full\n";
        udp_sendstr(cmd_p, msg);
        cmd_free(cmd_p);
    }

    return;

err:
    dbg_trace();
    tn_halt();
}
/*----------------------------------------------------------------------------*/
// ����� ���������� �����������
/*----------------------------------------------------------------------------*/
static void task_cmd_LED_func(void* param)
{
    for (;;)
    {
       tn_task_sleep(100);
       ///
       if (ETH_RECV_FLAG)
       {
          pin_off(OPIN_TEST2);
          ETH_RECV_FLAG = FALSE;
          tn_task_sleep(100);
          pin_on(OPIN_TEST2);

       }
    }
}
/*----------------------------------------------------------------------------*/
//
/*----------------------------------------------------------------------------*/
/*������� ������*/
static bool CollectCmd(char *pStr,const BYTE typeCmd)
{
if(typeCmd==ONE_DK){
  strcpy(pStr,"allpollingdk\r");
  return true;
  }
if(typeCmd==GET_STATUS){
  strcpy(pStr,"getstatussurd\r");
  return true;
  }
if(typeCmd==SET_PHASE){
  strcpy(pStr,"setphaseudp");

  strcat(pStr,"\r");
  return true;
  }
return false;
}
/*ret IP addres---------------------------------------------------------------*/
Type_Return retSurdIP(const BYTE nDk,struct ip_addr *addr)
{
struct ip_addr ipaddr;
const TPROJECT *prj = retPointPROJECT();

if(nDk>prj->maxDK)return retNull;
const char *pStrIP = (char*)prj->surd.ip[nDk];
unsigned ip[4];
if(sscanf(pStrIP,"%u.%u.%u.%u",&ip[0],&ip[1],&ip[2],&ip[3])==4){
  IP4_ADDR(&ipaddr,ip[0],ip[1],ip[2],ip[3]);
  memcpy(addr,&ipaddr,sizeof(addr));
  return retOk;
  }
return retNull;
}
/* master DK------------------------------------------------------------------*/
Type_Return masterDk(const TPROJECT *ret)
{
const WORD IPPORT = ret->surd.PORT;
char BuffCommand[30];
static int stepMaster = Null;
static BYTE indeDk = 1,countsend; // ���� �� ������� ��
struct ip_addr ipaddr;

switch(stepMaster)
  {
  case Null: //������
    clearStatusDk(indeDk);// �����
    setStatusDk(0); // status master
    countsend =0;
  case One:
    CollectCmd(BuffCommand,ONE_DK);
    if(retSurdIP(indeDk,&ipaddr)==retOk){
      udp_native_sendbuf(g_cmd_ch_pcb,BuffCommand,strlen(BuffCommand),&ipaddr,IPPORT);
      stepMaster=Two;
      }else stepMaster=Null;
    return retNull;
  case Two: // answer
    stepMaster=One;
    if(getStatusDk(indeDk)){//����� �������
      if(indeDk<ret->maxDK)indeDk++;
                  else {indeDk = 1;stepMaster = Three;}
      }
    if(++countsend>3){ // ������ �����
      stepMaster = Null;return retError;}
    return retNull;
  case Three: // ���������

    return retNull;
  default:
    stepMaster = Null;return retError;
  }
}
/*slave DK -------------------------------------------------------------------*/
Type_Return slaveDk(const TPROJECT *ret)
{
const WORD IPPORT = ret->surd.PORT;
char BuffCommand[30];
static BYTE contQuery=0,stepSlave = Null;
static BOOL fMessageErr = false,fMessageOk = false;
struct ip_addr ipaddr;

switch(stepSlave)
  {
  case Null:
      if(retRequestsVPU())stepSlave = One;
                     else stepSlave = Three;
      contQuery=0;
      return retOk;
  // ���� ������� �� VPU �������� UDP
  case One:
      CollectCmd(BuffCommand,SET_PHASE);
      retSurdIP(0,&ipaddr);
      udp_native_sendbuf(g_cmd_ch_pcb,BuffCommand,strlen(BuffCommand),&ipaddr,IPPORT);
      contQuery++;
      stepSlave = Two;
  case Two:
      if(contQuery>=MAX_QURY_SURD)stepSlave = Null;
                             else stepSlave = One;
      return retOk;
  //������ ������� ����
  case Three:
      CollectCmd(BuffCommand,GET_STATUS);
      retSurdIP(0,&ipaddr);
      udp_native_sendbuf(g_cmd_ch_pcb,BuffCommand,strlen(BuffCommand),&ipaddr,IPPORT);
      contQuery++;
      stepSlave = For;
      return retOk;
  case For:
      stepSlave = Three;
      if((getAllDk())&&(contQuery>MAX_QURY_SURD)){ // all OK
          stepSlave = Null;
          fMessageErr = false;
          if(!fMessageOk){Event_Push_Str("ERROR: ��� �� � ����");fMessageOk = true;}
          }
      if((!getAllDk())&&(contQuery>MAX_QURY_SURD)){ // error
          stepSlave = Null;
          fMessageOk = false;
          if(!fMessageErr){Event_Push_Str("ERROR: ������ ������ ��");fMessageErr = true;}
          }
      return retOk;
  default:stepSlave = Null;
      return retError;
  }
}
/*��������� ������ �� �����---------------------------------------------------*/
static void PollingDkLine(const TPROJECT *ret)
{
if(ret->maxDK>1){ // � ������� ������ ������ ��
   if(ret->surd.ID_DK_CUR==0){
      masterDk(ret);// ������
      }else{
      slaveDk(ret); // slave
      }
  }
}
/*�������� ������� ������ CMD ------------------------------------------------*/
static void task_cmd_func(void* param)
{
  const int TimeDelay = 100; // 100 ms
  for (;;)
    {
        struct cmd_raw* cmd_p;
        // �������� ������� ����� �� �������
        const int answer = tn_queue_receive(&g_cmd_dque,(void**)&cmd_p,TimeDelay);//TN_WAIT_INFINITE
        // clear WD
        hw_watchdog_clear();
        // �������
        if(answer==TERR_TIMEOUT){
          if((!DK[CUR_DK].OSHARD)&&
             (!DK[CUR_DK].OSSOFT)&&
             (!DK[CUR_DK].AUTO)){// ��� ������ ���������������� � �� ������� ����� ����
               const TPROJECT *pPr = retPointPROJECT();   // �������� ������
               PollingDkLine(pPr);
              }else{clearAllStatusDk();}
          // ������� ip ���������
          /*if(getFlagDefaultIP()){
            saveDatePorojectIP(); // ��������� �� ��������� ������
            } */
          }else{ // ���� ��� �� � �������
          // ���� ������
          if(answer != TERR_NO_ERR){
            dbg_puts("tn_queue_receive(&g_cmd_dque) error");
            break; // ������� �� ������
            }
          // ��������� ����
          if (cmd_p == NULL){
            dbg_puts("cmd_p == 0 error");
            break; // ������� �� ������
            }
          cmd_processing(cmd_p); // ��������� �������� ������
          cmd_free(cmd_p);
          }
    }
// ����� ���������� �� �����
dbg_trace();
tn_halt();
}
/*----------------------------------------------------------------------------*/
// ��������� ���������� �� ����� ��������� �����������������
static bool retBroadcastIP(struct cmd_raw* cmd_p)
{
struct ip_addr gw = {pref_get_long(PREF_L_NET_GW)}; // mask net
struct ip_addr ip;
memcpy(&ip,&cmd_p->raddr,sizeof(ip));
if(gw.addr!=0x00FFFFFF)return false; // �������� �� ����� ����� ������������ �������
long result =  gw.addr|ip.addr; // �������� ������
if(result!=-1)return false; // ������ �����������������
return true;
}
/*----------------------------------------------------------------------------*/
// ������������ ������� �� UDP
static void cmd_processing(struct cmd_raw* cmd_p)
{
    static char const ctrl[] = " \t\n\r";

    char*       buf = (char*)cmd_p->buf_p->payload;
    int const   len = cmd_p->buf_p->len < PBUF_POOL_BUFSIZE ? cmd_p->buf_p->len : PBUF_POOL_BUFSIZE - 1;
    buf[len] = '\0';

    struct cmd_nfo* nfo_p = find_cmd_nfo(strtok(buf, ctrl));
    // �� ���� �������
    if (nfo_p == NULL || (nfo_p->cmd_fl & CMD_FL_ACT) == 0){
      // ��������� ������ � ������ ip
      if(!retBroadcastIP(cmd_p)){// ����� ���� �� ����������������� ������ IP
        //static char const msg[] = "ERR: Unknown command\n";
        //udp_sendstr(cmd_p, msg); ������ ��������
        }
      return;
      }
    // ������� ���������� ����?
    if(nfo_p->cmd_func == NULL)
    {
        dbg_printf("\"%s\" nfo_p->cmd_func == NULL error", nfo_p->cmd_txt);
        dbg_trace();
        tn_halt();
    }
    //
    int argc;
    for (argc = 0; argc < MAX_ARGV_SZ - 1; ++argc)
    {
        char* tk = strtok(NULL, ctrl);
        if (tk == NULL)
            break;

        g_argv[argc] = tk;
    }

    g_argv[argc] = NULL;
    (*nfo_p->cmd_func)(cmd_p, argc, g_argv);
}
/*---------------------------------------------------------------------------*/
// �������� �������
static BOOL cmd_nfo_push_back(char* cmd_txt,
                              void (*cmd_func)(struct cmd_raw* cmd_p, int argc, char** argv),
                              char* cmd_help)
{
    static int idx;
    struct cmd_nfo* nfo_p;

    if (idx == CMD_NFO_SZ)
        return FALSE;

    nfo_p = &g_cmd_nfo[idx++];
    nfo_p->cmd_fl |= CMD_FL_ACT;
    nfo_p->cmd_txt = cmd_txt;
    nfo_p->cmd_func = cmd_func;
    nfo_p->cmd_help = cmd_help;
    nfo_p->next = 0;

    if (idx > 1)
        g_cmd_nfo[idx - 2].next = nfo_p;

    return TRUE;
}
// ������ ������� �� �������� ------------------------------------------------//
static struct cmd_nfo* find_cmd_nfo(char* cmd_txt)
{
    if (cmd_txt == 0 || cmd_txt[0] == 0)
        return 0;

    struct cmd_nfo* nfo_p = g_cmd_nfo;
    while (nfo_p != 0)
    {
        if (strcmp(nfo_p->cmd_txt, cmd_txt) == 0)
            return nfo_p;

        nfo_p = nfo_p->next;
    }
    return 0;
}
// �������� IP ������ --------------------------------------------------------//
static BOOL ip_security_check(struct ip_addr* srv_addr)
{
    if (g_security_ipaddr.addr == IP_ADDR_ANY->addr)
        return TRUE;

    return g_security_ipaddr.addr == srv_addr->addr;
}
/*----------------------------------------------------------------------------*/
// ������� ��� ������ � ������
/*----------------------------------------------------------------------------*/
// ��������� ��� �� �� �����.
BOOL getAllDk(void)
{
const DWORD sattusDk = DK[CUR_DK].StatusSurd.StatusDK;
const int maxDk = PROJ[CUR_DK].maxDK;

for(int i=0;i<maxDk;i++)
  {
  if(PROJ[CUR_DK].surd.ID_DK_CUR==i)continue; // ��� ��� id
  if((sattusDk&(1<<i))==0)return false; // �� �� �������
  }
return true;
}
// ��������� ����� ������ ��
void setStatusDk(const BYTE nDk)
{
DK[CUR_DK].StatusSurd.StatusDK|=1<<nDk;
}
// ��������� ������
BOOL getStatusDk(const BYTE nDk)
{
return (BOOL)(DK[CUR_DK].StatusSurd.StatusDK)&(1<<nDk);
}
// ������� ������ ��
void clearStatusDk(const BYTE nDk)
{
DK[CUR_DK].StatusSurd.StatusDK&=~(1<<nDk);
}
// �������� ���� ������
void clearAllStatusDk(void)
{
DK[CUR_DK].StatusSurd.StatusDK=NULL;
}
// ������������� ������ DK
BOOL checkMessageDk(const BYTE id,const DWORD pass,const DWORD idp)
{
const TPROJECT *prj = retPointPROJECT();
//�������  IDP
const long idp_calc = retCRC32();
if(idp==idp_calc){// ������ ���
  //�������� ������

  if(pass==prj->surd.Pass){
    if(id<prj->maxDK){
      setStatusDk(id);//�� �������
      return true;
      }
    }
  }
return false;
}
/*----------------------------------------------------------------------------*/
/*static void PollingDkLine(const TPROJECT *ret)
{
char BuffCommand[30];
const WORD IPPORT = ret->surd.PORT;
static Type_Step_Machine step = Null;
static BYTE count = 0,countError = 0,countOk = 0,countClear = 0;
static bool fMessageErr = false,fMessageOk = false;

// ������� ���� �������

switch(step)
  {
  case Null: // ����������
    if(!countClear){// ����� ���� ������
        countClear=ret->surd.Count;
        clearAllStatusDk();
        }else
        countClear--;
    CollectCmdBroadCast(BuffCommand,ALL_DK); //
    DK[0].StatusSurd.CountRequests++; // ������� ���������� ��������
    udp_sendStrBroadcast(BuffCommand,IPPORT);
    step = One;
    return;
  case One:
    count = 0;
    if(retRequestsVPU()==0){ // �������� ������� �� ���
      // �������� �� ��� ���,
      if(getAllDk()){
        if(++countOk>=ret->surd.Count){
          countOk = countError = 0;
          fMessageErr = false;
          if(!fMessageOk){Event_Push_Str("��� �� � ����");fMessageOk = true;}
          }
        }else{
         if(++countError>=ret->surd.Count){
          countOk = countError = 0;
          fMessageOk = false;
          if(!fMessageErr){Event_Push_Str("ERROR: ������ ������ ��");fMessageErr = true;}
          }
        }
      step = Null;
      return;
      }
  case Two:   // �������� ��� ���� ��������� ������
    clearAllStatusDk(); // ����� ���� ������
    CollectCmdBroadCast(BuffCommand,SET_PHASE);
    udp_sendStrBroadcast(BuffCommand,IPPORT);
    step = Three;
    return;
  case Three: // �������� ��� ������
    if(getAllDk()){step = Null;return;}
    if(++count>=3){step = Null;count=0;Event_Push_Str("������ ������� ��");return;}// ���� �������� � ������ �������!
    step = Two;
    return;
  default:
    DK[0].StatusSurd.CountRequests = step = Null;
    count=countError=countOk = 0;
    return;
  }
}*/