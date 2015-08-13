/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include <string.h> // memset(), memcpy()
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
// создать лист команд
static void create_cmd_nfo_list()
{
    static char const emsg[] = "cmd_nfo_push_back(\"%s\") error";

    // fill supported cmd's list
    // max cmd's count == CMD_NFO_SZ

    //CMD_NFO_PUSH_BACK("debugee", &cmd_debugee_func, "");
    //commands net udp
    CMD_NFO_PUSH_BACK("allpollingdk",&cmd_polling_func,"");
    CMD_NFO_PUSH_BACK("setphaseudp",&cmd_setphase_func,"");
    CMD_NFO_PUSH_BACK("answersurd",&cmd_answer_surd_func,"");
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
                                            " [VPU] ХХ\n"  );

    CMD_NFO_PUSH_BACK("light", &cmd_light_func, " [faza] XX, [OS], [YF], [KK],[undo]");
    //
    CMD_NFO_PUSH_BACK("reboot", &cmd_reboot_func, "");
    CMD_NFO_PUSH_BACK("flash_reset", &cmd_prefReset_func, " 255 ");

    CMD_NFO_PUSH_BACK("flashwr", &cmd_flashwr_func, " addr len data");  // Запись во FLASH по произвольному адресу
    CMD_NFO_PUSH_BACK("pflashwr", &cmd_pflashwr_func, " addr len data");  // Запись проекта во FLASH по произвольному адресу
    CMD_NFO_PUSH_BACK("proj_flashwr", &cmd_proj_flashwr_func, " addr len data");  // Запись проекта во FLASH по известному адресу
    CMD_NFO_PUSH_BACK("progs_flashwr", &cmd_progs_flashwr_func, " addr len data");  // Запись проекта во FLASH по известному адресу
    //
    CMD_NFO_PUSH_BACK("proj_flashrd", &cmd_proj_flashrd_func, " addr len");  // Запись проекта во FLASH по известному адресу
    CMD_NFO_PUSH_BACK("progs_flashrd", &cmd_progs_flashrd_func, " addr len");  // Запись проекта во FLASH по известному адресу
    //

    CMD_NFO_PUSH_BACK("flashrd", &cmd_flashrd_func, " addr len");       // Чтение из FLASH по произвольному адресу

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
// инициализация модуля CMD
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

    create_cmd_nfo_list(); // создатть лист команд

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
    //задаем порт и широковещательный режим
    if (udp_bind(g_cmd_ch_pcb, IP_ADDR_ANY, g_cmd_ch_port) != ERR_OK)
    {
        dbg_puts("udp_bind() error");
        goto err;
    }
    // регистрация обработчика.
    udp_recv(g_cmd_ch_pcb, &cmd_recv, 0);
    dbg_puts("[done]");
    return;

err:
    dbg_trace();
    tn_halt();
}
// отправка строки широковещательно
err_t udp_sendStrBroadcast(char const* str,u16_t rport)
{
return udp_native_sendbuf(g_cmd_ch_pcb,str,strlen(str),IP_ADDR_BROADCAST,rport);// широковещательный запрос
}
// отправить строку по UDP ---------------------------------------------------//
err_t udp_sendstr(struct cmd_raw* cmd_p, char const* s)
{
    return udp_sendbuf(cmd_p, s, strlen(s));
}
// отправить буффер по UDP ---------------------------------------------------//
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
// запрос адреса -------------------------------------------------------------//
void get_cmd_ch_ip(struct ip_addr* ipaddr)
{
    *ipaddr = g_security_ipaddr;
}
// запрос порта---------------------------------------------------------------//
unsigned get_cmd_ch_port()
{
    return (unsigned)g_cmd_ch_port;
}
// отправка по UDP -----------------------------------------------------------//
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
// задать размер для приема --------------------------------------------------//
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
// очиска команд -------------------------------------------------------------//
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
// подготовка к приему данных ------------------------------------------------//
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
// поток управления светодиодом
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
/*собрать широковещательный запрос*/
static bool CollectCmdBroadCast(char *pStr,const BYTE typeCmd)
{
if(typeCmd==ALL_DK){
  strcpy(pStr,"allpollingdk\r");
  return true;
  }
if(typeCmd==SET_PHASE){
  strcpy(pStr,"setPhaseUDP");

  strcat(pStr,"\r");
  return true;
  }
return false;
}
//------------------------------------------------------------------------------
/*отправка широковещательных пакетов*/
/*запускаем машину на опрос*/
static void PollingDkLine(const TPROJECT *ret)
{
char BuffCommand[30];
const WORD IPPORT = ret->surd.PORT;
static STEP_NET step = NUL;
static BYTE count = 0,countError = 0,countOk = 0;
static bool fMessageErr = false,fMessageOk = false;

switch(step)
  {
  case NUL: // отправляем
    clearAllStatusDk(); // нулим весь статус
    CollectCmdBroadCast(BuffCommand,ALL_DK); //
    DK[0].StatusSurd.CountRequests++; // считаем количество запросов
    udp_sendStrBroadcast(BuffCommand,IPPORT);
    step = ONE;
    return;
  case ONE:
    count = 0;
    if(retRequestsVPU()==0){ // поверяем запросы по ВПУ
      // запросов по ВПУ нет,
      if(getAllDk()){
        if(++countOk>=ret->surd.Count){
          countError = 0;
          fMessageErr = false;
          if(!fMessageOk){Event_Push_Str("Все ДК в сети");fMessageOk = true;}
          }
        }else{
         if(++countError>=ret->surd.Count){
          countOk = 0;
          fMessageOk = false;
          if(!fMessageErr){Event_Push_Str("ERROR: Ошибки опроса ДК в сети");fMessageErr = true;}
          }
        }
      step = NUL;
      return;
      }
  case TWO:   // работает ВПУ надо отправить запрос
    clearAllStatusDk(); // нулим весь статус
    CollectCmdBroadCast(BuffCommand,SET_PHASE);
    udp_sendStrBroadcast(BuffCommand,IPPORT);
    return;
  case THREE: // получили все ответы
    if(getAllDk()){step = NUL;return;}
    if(++count>=3){step = NUL;count=0;Event_Push_Str("Ошибки ответов ДК");return;}// надо записать в журнал событие!
    step = TWO;
    return;
  default:
    DK[0].StatusSurd.CountRequests = step = NUL;
    count=countError=countOk = 0;
    return;
  }
}
//------------------------------------------------------------------------------
/*основная функция потока CMD*/
static void task_cmd_func(void* param)
{
  for (;;)
    {
        struct cmd_raw* cmd_p;
        const TPROJECT *retPrj = retPointPROJECT();   // получить данные
        const int TimeDelay = 1000;//retPrj->surd.TimeDelay; // время задержки
        // проверка очереди выход по времени
        const int answer = tn_queue_receive(&g_cmd_dque,(void**)&cmd_p,TimeDelay);//TN_WAIT_INFINITE
        // clear WD
        hw_watchdog_clear();
        // таймаут
        if(answer==TERR_TIMEOUT){
          if(DK[0].tumbler!=TUMBLER) // нет режима программирования
              PollingDkLine(retPrj);
          }else{ // есть что то в буффере
          // есть ошибки
          if(answer != TERR_NO_ERR){
            dbg_puts("tn_queue_receive(&g_cmd_dque) error");
            break; // выходим из потока
            }
          // структура ноль
          if (cmd_p == NULL){
            dbg_puts("cmd_p == 0 error");
            break; // выходим из потока
            }
          cmd_processing(cmd_p); // обработка принятых команд
          cmd_free(cmd_p);
          }
    }
// когда вывалились из цикла
dbg_trace();
tn_halt();
}
/*----------------------------------------------------------------------------*/
// проверить являлиться ли адрес отправлки широковещательным
static bool retBroadcastIP(struct cmd_raw* cmd_p)
{
struct ip_addr gw = {pref_get_long(PREF_L_NET_GW)}; // mask net
struct ip_addr ip;
memcpy(&ip,&cmd_p->raddr,sizeof(ip));
if(gw.addr!=0x00FFFFFF)return false; // сравнить не можем маска неправильная выходим
long result =  gw.addr|ip.addr; // сравнить адреса
if(result!=-1)return false; // адресс широквоещательный
return true;
}
/*----------------------------------------------------------------------------*/
// отрабатываем команды по UDP
static void cmd_processing(struct cmd_raw* cmd_p)
{
    static char const ctrl[] = " \t\n\r";

    char*       buf = (char*)cmd_p->buf_p->payload;
    int const   len = cmd_p->buf_p->len < PBUF_POOL_BUFSIZE ? cmd_p->buf_p->len : PBUF_POOL_BUFSIZE - 1;
    buf[len] = '\0';

    struct cmd_nfo* nfo_p = find_cmd_nfo(strtok(buf, ctrl));
    // не наша команда
    if (nfo_p == NULL || (nfo_p->cmd_fl & CMD_FL_ACT) == 0){
      // проверить запрос с какого ip
      if(!retBroadcastIP(cmd_p)){// ответ если не широковещательный адресс IP
        //static char const msg[] = "ERR: Unknown command\n";
        //udp_sendstr(cmd_p, msg); запрет отправки
        }
      return;
      }
    // функция обработчик есть?
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
// добовать команду
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
// вернем команду по названию ------------------------------------------------//
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
// проверка IP адреса --------------------------------------------------------//
static BOOL ip_security_check(struct ip_addr* srv_addr)
{
    if (g_security_ipaddr.addr == IP_ADDR_ANY->addr)
        return TRUE;

    return g_security_ipaddr.addr == srv_addr->addr;
}
/*----------------------------------------------------------------------------*/
//static void SurdStatus(void)
//{
//
        //char buf[30];
        /*
        snprintf(buf, sizeof(buf),
        "IP:%u.%u.%u.%u\n",
        (unsigned)ip4_addr1(cmd_p->raddr),
        (unsigned)ip4_addr2(cmd_p->raddr),
        (unsigned)ip4_addr3(cmd_p->raddr),
        (unsigned)ip4_addr4(cmd_p->raddr)
        ); */
        /*unsigned char ip1 = (unsigned)ip4_addr1(cmd_p->raddr);
        unsigned char ip2 = (unsigned)ip4_addr2(cmd_p->raddr);
        unsigned char ip3 = (unsigned)ip4_addr3(cmd_p->raddr);
        unsigned char ip4 = (unsigned)ip4_addr4(cmd_p->raddr);
        buf[0] = ip1;
        buf[1] = ip2;
        buf[2] = ip3;
        buf[3] = ip4;
        buf[4] = '\n';*/

        /*struct ip_addr  addr_remip;
        IP4_ADDR(&addr_remip, 169, 254,16,255);
        cmd_p->raddr = &addr_remip;
        cmd_p->rport = 11990;
        udp_sendstr(cmd_p,"STR1 new paskage");*/
  //      char *buff = "STR1 new paskage";
    //    udp_native_sendbuf(g_cmd_ch_pcb,buff,strlen(buff),IP_ADDR_BROADCAST,11990);
        /*char *buff = "STR1 new paskage";
        udp_cmd_sendbuf(buff,strlen(buff));*/
//}