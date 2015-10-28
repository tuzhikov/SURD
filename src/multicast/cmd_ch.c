/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
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
// состояния всех ВПУ по сети
VIR_VPU vir_vpu;
// промахи опроса по UDP статистика
POL_SLIPS pol_slips = {0};
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
    CMD_NFO_PUSH_BACK("allpollingdk",&cmd_getsatus_func,"");
    CMD_NFO_PUSH_BACK("getstatussurd",&cmd_getsatus_func,"");
    CMD_NFO_PUSH_BACK("setstatussurd",&cmd_setsatus_func,"");
    CMD_NFO_PUSH_BACK("setphaseudp",&cmd_setphase_func,"");
    CMD_NFO_PUSH_BACK("SUCCESS:",&cmd_answer_surd_func,""); // проверяем ответы мастером
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
/*----------------------------------------------------------------------------*/
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
       if (ETH_RECV_FLAG)
       {
         hw_watchdog_clear(); // clear WD
         pin_off(OPIN_TEST2);
          ETH_RECV_FLAG = FALSE;
          tn_task_sleep(100);
          pin_on(OPIN_TEST2);
        hw_watchdog_clear(); // clear WD
       }
    }
}
/*----------------------------------------------------------------------------*/
/* проверка запровсов от ВПУ или сети */
BOOL retAnswerVPU(void)
{
static BYTE stat = Null;
if(getFlagStatusSURD()){// проверки статуса соединения свех ДК getStatusSURD
  // забираем данные c ВПУ мастер
  vir_vpu.vpu[0].id = 0;
  vir_vpu.vpu[0].vpuOn = retOnVPU();
  vir_vpu.vpu[0].phase = retStateVPU();
  vir_vpu.vpu[0].stLED = retStatusLed();
  //
  if(stat==Null)
    {
    // проверяем активные ВПУ
    for(int i=0;i<MAX_VIR_VPU;i++)
      {
      if(vir_vpu.vpu[i].vpuOn){
        vir_vpu.active = i;
        stat=One;
        return false;
        }
      }
    }
  else if(stat==One) // работа активного ВПУ
    {
    if(vir_vpu.vpu[vir_vpu.active].vpuOn){ // ВПУ еще активет?
       // здесь надо запустить таймер на залипание фазы
       // установить фазы для мастера
       updateCurrentDatePhase(true,vir_vpu.vpu[vir_vpu.active].id,true,
                                vir_vpu.vpu[vir_vpu.active].phase);
       if(vir_vpu.active){ // управляет не мастер
          setStatusLed(vir_vpu.vpu[vir_vpu.active].stLED);
          }
       }else{
       stat = Null;
       vir_vpu.vpu[vir_vpu.active].phase = tlNo;
       updateCurrentDatePhase(true, vir_vpu.vpu[vir_vpu.active].id,
                              false,vir_vpu.vpu[vir_vpu.active].phase);
       if(vir_vpu.active){ // управляет не мастер
          setStatusLed(vir_vpu.vpu[vir_vpu.active].stLED);
          }
       }
    return true;
    }
  // выходим переключая режим
  ReturnToWorkPlan(); // Если был включен ВПУ -> режим ПЛАН
  return false;
  }
// попадаем при ощибке в СУРД, и переходим в режим ПЛАН
ReturnToWorkPlan(); // Если был включен ВПУ -> режим ПЛАН
memset(&vir_vpu,NULL,sizeof(vir_vpu)); // чистим всю структуру
stat = Null; // сбросс сотояний
return false;
}
/* переключение на OS and AUTO -----------------------------------------------*/
BOOL retModeNoPolling(void)
{
const BOOL flagOS_AUTO_ON = (DK[CUR_DK].OSHARD)|(DK[CUR_DK].OSSOFT)|(DK[CUR_DK].tumblerAUTO);
// установка флага ДК
if(DK[CUR_DK].CUR.source==ALARM) setStatusDK(false);
                            else setStatusDK(true);
// установить FlagLocalStatusSURD
if((!getStatusDK())||(!getFlagNetwork())){setFlagLocalStatusSURD(SURD_RESTART_DK_OK);}
                  else if(flagOS_AUTO_ON){setFlagLocalStatusSURD(SURD_RESTART_DK_NO);}
                                     else{setFlagLocalStatusSURD(SURD_DK_OK);}
return false;
}
/*собрать запрос*/
static bool CollectCmd(char *pStr,WORD leng,const BYTE typeCmd)
{
if(typeCmd==ONE_DK){
  strcpy(pStr,"allpollingdk\r");
  return true;
  }
if(typeCmd==GET_STATUS){
  strcpy(pStr,"getstatussurd\r");
  return true;
  }
if(typeCmd==SET_STATUS){
  const TPROJECT *prg = retPointPROJECT();// данные по проекту
  const long idp = retCRC32();
  const long passw =prg->surd.Pass;  // получить пароль
  const long stnet = retStatusSurdSend();// статус СУРД общий для сети
  const long sdnet = retStatusNetSend(); // статус NET общий для сети
  const long retTime = retTimePhase(); // остаток времени для работы фазы

  snprintf(pStr,leng,
        "setstatussurd\r"
        "IDP:   %u\r\n"
        "PASSW: %u\r\n"
        "ST:    %u\r\n"
        "SD:    %u\r\n"
        "TM:    %u\r\n",
        idp,
        passw,
        stnet,
        sdnet,
        retTime);
  return true;
  }
if(typeCmd==SET_PHASE){
    const TPROJECT *prg = retPointPROJECT();// данные по проекту
    const long id = vir_vpu.active;// ДК с включенным ВПУ
    const long idp = retCRC32();
    const long passw = prg->surd.Pass;
    const BOOL stnet = retStatusSurdSend();// статус СУРД общий для сети
    const long sdnet = retStatusNetSend(); // статус NET общий для сети
    const long phase = vir_vpu.vpu[vir_vpu.active].phase; // отправить фазу для установки
    const long stLed = vir_vpu.vpu[vir_vpu.active].stLED;
    const long retTime = retTimePhase(); // остаток времени для работы фазы

    snprintf(pStr,leng,
        "setphaseudp\r"
        "ID:    %u\r\n"
        "IDP:   %u\r\n"
        "PASSW: %u\r\n"
        "ST:    %u\r\n"
        "SD:    %u\r\n"
        "PHASE: %u\r\n"
        "LED:   %u\r\n"
        "TM:    %u\r\n",
        id,
        idp,
        passw,
        stnet,
        sdnet,
        phase,
        stLed,
        retTime
         );
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
// отработаем запросы сетевого статуса
Type_Return masterDk(const TPROJECT *ret)
{
const WORD IPPORT     = ret->surd.PORT;
const WORD CountRepet = ret->surd.Count;
static char BuffCommand[250];
static int stepMaster = Null;
static BYTE indeDk = 1,countsend=0,countsendOk=0,countsendErr=0;// счет indeDk со второго ДК
static BYTE countmessageOk = 0,countmessageErr = 0;
static BYTE countSrdOk=0,countSrdError=0;
static BOOL fMessageErr = false,fMessageOk = false;
static BOOL fMessageErrSrd = false,fMessageOkSrd = false;
struct ip_addr ipaddr;

retModeNoPolling();// polling

switch(stepMaster)
  {
  case Null: //сбросс
    clearStatusOneDk(indeDk);// нулим
    clearStSurdOneDk(indeDk);// нулим
    setStatusOneDk(0,getFlagStatusSURD(),getValueFlagLocalStatusSURD()); // status master, статус СУРД
    countsendErr += countsend; // сохраним промахи предыдущего обмена
    countsend = 0;
    if(retAnswerVPU()){
                  CollectCmd(BuffCommand,sizeof(BuffCommand),SET_PHASE); // установить фазу
                  }else{
                  CollectCmd(BuffCommand,sizeof(BuffCommand),SET_STATUS);// опрос статуса
                  }
  case One:
    if(retSurdIP(indeDk,&ipaddr)==retOk){
      udp_native_sendbuf(g_cmd_ch_pcb,BuffCommand,strlen(BuffCommand),&ipaddr,IPPORT);
      stepMaster=Two;
      }else {
      stepMaster=Three;
      }
    return retNull;
  case Two: // timeout OK проверяем ответ
    stepMaster=One;
    if(getStatusOneDk(indeDk)){//ответ получен
      if(++indeDk<ret->maxDK){stepMaster=Null;}
                         else{stepMaster=Three;}
      countsendOk++;// считаем удачные попытки ответа
      }
    if(++countsend>CountRepet){ // ответа нет CountRepet раз выходим
      stepMaster = Three;return retError;
      }
    return retNull;
  case Three:
    stepMaster=Five;
    pol_slips.glOk  = countsendOk;
    pol_slips.glErr = countsendErr;
    countsendOk = countsendErr = 0;
  case Five: // Опрошены все ДК, формируем статус СУРД
    stepMaster=Seven;
    // все ДК в сети
    if(getAllDk()){
      setFlagNetwork(true);
      setStatusNetSend(retStatusNet());
      if(++countmessageOk>5){
        countmessageErr = 0;
        fMessageErr = false;
        if(!fMessageOk){Event_Push_Str("SUCCESS:Все ДК в сети");fMessageOk = true;}
        }
      }else{
      setFlagNetwork(false);
      setStatusNetSend(retStatusNet());
      if(++countmessageErr>5){
        countmessageOk  =0;
        fMessageOk = false;
        if(!fMessageErr){Event_Push_Str("ERROR: Ошибки опроса ДК");fMessageErr = true;}
        }
      }
    // все могут работать в СУРД
    if(getAllSURD()){
      setStatusSurdSend(retStSurd()); // текущее состояние СУРД для отправки по UDP
      setFlagStatusSURD(true);     // установить статус сети
      if(++countSrdOk>3){
          countSrdOk = 0;
          fMessageErrSrd = false;
          if(!fMessageOkSrd){Event_Push_Str("SUCCESS: СУРД ОК");fMessageOkSrd = true;}
          }
      }else{
      setStatusSurdSend(retStSurd()); // текущее состояние СУРД для отправки по UDP
      setFlagStatusSURD(false);   // уберем флаг СУРД по сети
      if(++countSrdError>3){
          countSrdError = 0;
          fMessageOkSrd = false;
          if(!fMessageErrSrd){Event_Push_Str("ERROR: СУРД NO");fMessageErrSrd = true;}
          }
      }
  case Seven: // exit OK
    indeDk = 1; // опрос с первого slave
    stepMaster = Null;
    return retOk;
  default:   // exit Error
    indeDk = 1; // опрос с первого slave
    stepMaster = Null;
    return retError;
  }
}
/*slave DK -------------------------------------------------------------------*/
Type_Return slaveDk(const TPROJECT *ret)
{
const WORD CountRepet = ret->surd.Count; // кол повторений запросов
const WORD TimeDelay = ((WORD)ret->maxDK)*CountRepet; // общее время на формирование СУРД
static BYTE stepSlave = Null,countTime = 0;
static BYTE countOk = 0,countError = 0;
static BYTE countSrdOk = 0,countSrdError = 0;
static BOOL fMessageErr = false,fMessageOk = false;
static BOOL fMessageErrSrd = false,fMessageOkSrd = false;

switch(stepSlave)
  {
  case Null:
    clearStatusNet();
    clearStSurd();
    stepSlave = One;
  case One: // Ждем TimeDelay сек и проверка на ОС
    if(retModeNoPolling())return retNull; // включили ОС, выходим
    if(++countTime>TimeDelay){//TimeDelay время опроса всех ДК в сети
        countTime = 0;
        stepSlave = Two;
        }
    return retNull;
  case Two: // проверка СУРД, отключение ВПУ
    if(!getFlagStatusSURD()){ReturnToWorkPlan();}
    stepSlave = Three;
  case Three: // проверка ДК в сети?
    if(getAllDk()){ // ДК в сети
        setFlagNetwork(true); // установить статус сети
        if(++countOk>3){
          countOk = 0;
          fMessageErr = false;
          if(!fMessageOk){Event_Push_Str("SUCCESS: Все ДК в сети");fMessageOk = true;}
          }
        }else{
        setFlagNetwork(false); // установить статус сети
        if(++countError>3){
          countError = 0;
          fMessageOk = false;
          if(!fMessageErr){Event_Push_Str("ERROR: Ошибки опроса ДК");fMessageErr = true;}
          }
        }
      // все могут работать в СУРД
      if(getAllSURD()){
        setFlagStatusSURD(true);  // установим флаг СУРД по сети
        if(++countSrdOk>3){
          countSrdOk = 0;
          fMessageErrSrd = false;
          if(!fMessageOkSrd){Event_Push_Str("SUCCESS: СУРД ОК");fMessageOkSrd = true;}
          }
        }else{
        setFlagStatusSURD(false); // уберем флаг СУРД по сети
        if(++countSrdError>3){
          countSrdError = 0;
          fMessageOkSrd = false;
          if(!fMessageErrSrd){Event_Push_Str("ERROR: СУРД NO");fMessageErrSrd = true;}
          }
        }
      stepSlave = Null;
      return retOk;
  default:stepSlave = Null;return retError;
  }
}
/*запускаем машину на опрос---------------------------------------------------*/
static void PollingDkLine(const TPROJECT *ret)
{
if(ret->maxDK>1){ // в проекте больше одного ДК
   if(ret->surd.ID_DK_CUR==0){
      masterDk(ret);// мастер
      }else{
      slaveDk(ret); // slave
      }
  }
}
/*основная функция потока CMD ------------------------------------------------*/
static void task_cmd_func(void* param)
{

  for (;;)
    {
        struct cmd_raw* cmd_p;
        const TPROJECT *pPr = retPointPROJECT();   // получить данные проекта
        const int DelayTime = (pPr->surd.TimeDelay>900)? 900:pPr->surd.TimeDelay;

        // проверка очереди выход по времени
        const int answer = tn_queue_receive(&g_cmd_dque,(void**)&cmd_p,DelayTime);//TN_WAIT_INFINITE
        hw_watchdog_clear(); // clear WD
        // таймаут
        if(answer==TERR_TIMEOUT){
          PollingDkLine(pPr);
          }else{ // есть что то в буффере UDP
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
    hw_watchdog_clear(); // clear WD
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
// Статус ДК
BOOL getStatusDK(void)
{
return (BOOL)DK[CUR_DK].StatusDK;
}
// установить статус
void setStatusDK(const BOOL flag)
{
DK[CUR_DK].StatusDK = flag;
}
// очистить статус
void clearStatusDK(void)
{
DK[CUR_DK].StatusDK = NULL;
}
/*----------------------------------------------------------------------------*/
// функции для работы в группе
/*----------------------------------------------------------------------------*/
//вернуть статус СУРД сетевой
BOOL getFlagStatusSURD(void)
{
return (BOOL)DK[CUR_DK].StatusSurd.flagStatusSURD;
//return true; // тест
}
// установить статус СУРД
void setFlagStatusSURD(const BOOL flag)
{
DK[CUR_DK].StatusSurd.flagStatusSURD = flag;
}
// очистить статус СУРД
void clearFlagStatusSURD(void)
{
DK[CUR_DK].StatusSurd.flagStatusSURD = NULL;
}
/* -------------------------статус СУРД локально------------------------------*/
// установить
void setFlagLocalStatusSURD(const BYTE flag)
{
DK[CUR_DK].StatusSurd.flagLocalStatusSURD = flag;
}
// значение СУРД значение
BYTE getValueFlagLocalStatusSURD(void)
{
return (BYTE)DK[CUR_DK].StatusSurd.flagLocalStatusSURD;
}
// вернуть статус СУРД
BOOL getFlagLocalStatusSURD(void)
{
return ((~DK[CUR_DK].StatusSurd.flagLocalStatusSURD)&SURD_DK_OK)? false:true;
}
/*----------------------------------------------------------------------------*/
// проверка все ДК в сети
BOOL getFlagNetwork(void)//network
{
return (BOOL)DK[CUR_DK].StatusSurd.flagNetworkStatus;
}
// clear flag net
void claerFlagNetwork(void)
{
DK[CUR_DK].StatusSurd.flagNetworkStatus = NULL;
}
// set flag net
void setFlagNetwork(const BOOL flag)
{
DK[CUR_DK].StatusSurd.flagNetworkStatus = flag;
}
/*----------------------------------------------------------------------------*/
// запросить статус One DK
BOOL getStSurdOneDk(const BYTE nDk)
{
return (BOOL)(DK[CUR_DK].StatusSurd.tmpStatusSURD)&(1<<nDk);
}
// сбрость статус One ДК
void clearStSurdOneDk(const BYTE nDk)
{
DK[CUR_DK].StatusSurd.tmpStatusSURD&=~(1<<nDk);
}
// очистить весь статус all DK
void clearStSurd(void)
{
DK[CUR_DK].StatusSurd.tmpStatusSURD=NULL;
}
// return status all DK
DWORD retStSurd(void)
{
return DK[CUR_DK].StatusSurd.tmpStatusSURD;
}
// set all status surd
void setStSurd(const DWORD st)
{
DK[CUR_DK].StatusSurd.tmpStatusSURD = st;
}
// проверить все ДК в СУРД
BOOL getAllSURD(void)
{
const int maxDk = PROJ[CUR_DK].maxDK;

for(int i=0;i<maxDk;i++)
  {
  if(PROJ[CUR_DK].surd.ID_DK_CUR==i){// это наш id
    if(getFlagLocalStatusSURD())DK[CUR_DK].StatusSurd.tmpStatusSURD|=(1<<i);
                           else DK[CUR_DK].StatusSurd.tmpStatusSURD&=~(1<<i);
    }
  if((DK[CUR_DK].StatusSurd.tmpStatusSURD&(1<<i))==0)return false; // ДК не в СУРД
  }
return true;
}
/*----------------------------------------------------------------------------*/
// проверить все ДК на связи.
BOOL getAllDk(void)
{
const DWORD sattusDk = DK[CUR_DK].StatusSurd.tmpStatusNet;
const int maxDk = PROJ[CUR_DK].maxDK;

for(int i=0;i<maxDk;i++)
  {
  if(PROJ[CUR_DK].surd.ID_DK_CUR==i)continue; // это наш id
  if((sattusDk&(1<<i))==0)return false; // дк не ответил
  }
return true;
}
// установть новый статус One ДК
void setStatusOneDk(const BYTE nDk,const BOOL sSurd,const BYTE actSurd)
{
DK[CUR_DK].StatusSurd.tmpStatusNet|=1<<nDk; //статус NET ДК
if(sSurd)DK[CUR_DK].StatusSurd.tmpStatusSURD|=1<<nDk; // статус СУРД для этого ДК
}
// запросить статус One DK
BOOL getStatusOneDk(const BYTE nDk)
{
return (BOOL)(DK[CUR_DK].StatusSurd.tmpStatusNet)&(1<<nDk);
}
// сбрость статус One ДК
void clearStatusOneDk(const BYTE nDk)
{
DK[CUR_DK].StatusSurd.tmpStatusNet&=~(1<<nDk);
}
// очистить весь статус all DK
void clearStatusNet(void)
{
DK[CUR_DK].StatusSurd.tmpStatusNet=NULL;
}
// return status all DK
DWORD retStatusNet(void)
{
return DK[CUR_DK].StatusSurd.tmpStatusNet;
}
// set all status surd
void setStatusNet(DWORD st)
{
DK[CUR_DK].StatusSurd.tmpStatusNet = st;
}
/*----------------------------------------------------------------------------*/
// word send to UDP передаем по сети
DWORD retStatusNetSend(void)
{
return DK[CUR_DK].StatusSurd.sendStatusDK;
}
void  setStatusNetSend(DWORD st)
{
DK[CUR_DK].StatusSurd.sendStatusDK = st;
}
void clearStatusNetSend(void)
{
DK[CUR_DK].StatusSurd.sendStatusDK = 0;
}
/*----------------------------------------------------------------------------*/
// word send to UDP передаем по сети статус СУРД
DWORD retStatusSurdSend(void)
{
return DK[CUR_DK].StatusSurd.sendStatusSURD;
}
void  setStatusSurdSend(DWORD st)
{
DK[CUR_DK].StatusSurd.sendStatusSURD = st;
}
void clearStatusSurdSend(void)
{
DK[CUR_DK].StatusSurd.sendStatusSURD = 0;
}
/*----------------------------------------------------------------------------*/
// "slave" проверяем запросы ДК от мастера и устанавливаем сетевой статус
BOOL checkSlaveMessageDk(const DWORD idp,const DWORD pass,const DWORD stnet,const DWORD sdnet)
{
const TPROJECT *prj = retPointPROJECT();
const long idp_calc = retCRC32();//сравним  IDP
if(idp==idp_calc){
  if(pass==prj->surd.Pass){
    if(prj->surd.ID_DK_CUR){// это slave
      // Net
      setStatusNet(sdnet);   // флаг проверки
      if(getAllDk())setFlagNetwork(true);
               else setFlagNetwork(false);
      //СУРД
      setStSurd(stnet);
      if(getAllSURD())setFlagStatusSURD(true);
                 else setFlagStatusSURD(false);
      return true;
      }
    }
  }
return false;
}
/*----------------------------------------------------------------------------*/
// "master" получает ответы от slav, добавлен статус ВПУ, статус СУРД
BOOL checkMasterMessageDk(const BYTE id,
                          const DWORD pass,
                          const DWORD idp,
                          const BYTE surdOn,
                          const BYTE vpuOn,
                          const BYTE vpuPhase,
                          const WORD stled,
                          const BYTE valSurd)
{
const TPROJECT *prj = retPointPROJECT();
//сравним  IDP
const long idp_calc = retCRC32();
if(idp==idp_calc){// проект наш
  //проверка пароля
  if(pass==prj->surd.Pass){
    if(id<prj->maxDK){
      if(prj->surd.ID_DK_CUR==0){ //DK master
        setStatusOneDk(id,surdOn,valSurd);//дк ответил, установили статус Net и СУРД
        vir_vpu.vpu[id].id = id;
        vir_vpu.vpu[id].vpuOn = vpuOn;
        vir_vpu.vpu[id].phase = vpuPhase;
        vir_vpu.vpu[id].stLED = stled;
        }
      return true;
      }
    }
  }
return false;
}
/*----------------------------------------------------------------------------*/