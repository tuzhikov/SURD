/*****************************************************************************
*
* © 2015 Cyber-SB. Written by Alex Tuzhikov.
*
******************************************************************************/
#include <string.h>
#include "vpu.h"
#include "crc16.h"
#include "pins.h"
#include "dk/dk.h"
#include "multicast/cmd_ch.h"
/*------------ UART      ------------------------------------------------*/
#define SYSCTL_PERIPH_UART          SYSCTL_PERIPH_UART1
#define INT_UART                    INT_UART1
#define UART_BASE                   UART1_BASE
#define UART_SPEED                  38400
#define TX1_BUFF_SIZE               128
#define RX1_BUFF_SIZE               128

#define PWR485_OFF()                pin_off(OPIN_485_PV)
#define PWR485_ON()                 pin_on(OPIN_485_PV)
#define RST485_OFF()                pin_off(OPIN_485_RST)
#define RST485_ON()                 pin_on(OPIN_485_RST)

/*MUTEX*/
static TN_MUTEX    led_mutex;
/* UART*/
static UART_SBUF   TX1buff, RX1buff;
static U08         TX1buff_mem[TX1_BUFF_SIZE];
static U08         RX1buff_mem[RX1_BUFF_SIZE];
/*VPU*/
/*TASK*/
#define VPU_REFRESH_INTERVAL        100
static TN_TCB task_VPU_tcb;
#pragma location = ".noinit"
#pragma data_alignment=8
static unsigned int task_VPU_stk[TASK_VPU_STK_SZ];
static void task_VPU_func(void* param);
/*Local functions*/
static BOOL tx1_pkt_snd_one();
static void bsp_vpu_buff_flush(UART_SBUF *buf);
static BOOL DataInstall(void);
static U16 StrPutVPU(UART_SBUF *buf,const U8* str, const U16 size);
//static S16 StrGetVPU(UART_SBUF const * buf);
static Type_ANS parserData(U8* data, U8 size);
static Type_ANS StrGetVPU(UART_SBUF * buf,U08* str, U16 size);
//static void YELDk(const char Enabled);
//static void OSDk(const BOOL Enabled);
////////////////////////////////////////////////
// Structure for exchange between master and slave
// Алгоритм общения такой.
// 1) Если на пульте включена кнопка РУ -
//      s_to_m.vpuOn=1
// 2) В s_to_m.vpu -статус ВПУ, а по-сути - текущая нажатая кнопка
//      или tlNo (ничего не нажато. нет запросов)
// 3) Если m_to_s.vpuOn==1 , то в сети кто-то рулит по ВПУ
// 4) Если m_to_s.vpu не равно tlNo, устанавливается соответствующее
//    состояние.
//
//
//
//
VPU_EXCHANGE  vpu_exch;
TVPU dataVpu;

// если несколько ВПУ
//VPU_EXCHANGE  vpu_exchN[VPU_COUNT];
//TVPU          dataVpuN[VPU_COUNT];
//int           cur_vpu=0; //номер текущего ВПУ

/*---------------------Local functions----------------------------------------*/
static void ResetStrMaster(void);
void DK_VPU_undo(void);
/*---------- Functions--------------------------------------------------------*/
// переключает контекст ВПУ-пультов
/*void Switch_VPU_Context(int vpu_new)
{
  // 1 сохраняем текущий контекст
  memcpy(&vpu_exchN[cur_vpu], &vpu_exch, sizeof(vpu_exch));
  memcpy(&dataVpuN[cur_vpu],  &dataVpu, sizeof(dataVpu));
  //2   переходим на новый контекст
  memcpy(&vpu_exch, &vpu_exchN[vpu_new], sizeof(vpu_exch));
  memcpy(&dataVpu, &dataVpuN[vpu_new],  sizeof(dataVpu));
  ////
  cur_vpu = vpu_new;

}  */
/*----------------------------------------------------------------------------*/
// проверка перехода на новую фазу
BOOL checkJumpPhase(void)
{
if(dataVpu.fixBut)
  return (DK[CUR_DK].REQ.req[VPU].faza==DK[CUR_DK].CUR.faza)? true:false;
return true;
}
//------------------------------------------------------------------------------
// Обновляет структуру m_t_s для ВСЕх ВПУ
void Update_m_t_s(MASTER_SLAVE_VPU *mts)
{
    //текущий контекст
    memcpy(&vpu_exch.m_to_s, mts, sizeof(vpu_exch.m_to_s));
    //
    /*for (int i=0; i<VPU_COUNT; i++)
    {
      memcpy(&vpu_exchN[i].m_to_s, mts, sizeof(vpu_exch.m_to_s));
    }*/

}
// обновить данные включить фазу----------------------------------------------//
// если не будет вызывать более 10 сек, то происходит обнулние статуса сети
void updateCurrentDatePhase(const BYTE stNet, // статус Net
                            const BYTE idDk,  // ID активного ВПУ
                            const BYTE vpuOn, // ВПУ on
                            const BYTE vpuST) // Вызываемая Фаза
{
MASTER_SLAVE_VPU mts;
mts.statusNet = stNet;
mts.vpuOn = vpuOn;
mts.idDk = idDk;
mts.vpu = (Type_STATUS_VPU)vpuST;
Update_m_t_s(&mts);
}
// сбросить режим ВПУ вернуться к АВТО
void setPlanMode(void)
{
// сбросс запроса ВПУ
DK_VPU_undo();
// сбросс структуры m_to_s
updateCurrentDatePhase(0,0,0,tlNo);
// сбросс значений фазы структуры s_to_m
vpu_exch.s_to_m.vpu = tlNo;
// установить флаг перехода на
dataVpu.fsetPlan  = true;
}
//------------------------------------------------------------------------------
void vpu_init() // это все по инициализации UART и создаем поток tn_kernel
{
    dbg_printf("Initializing VPU UART..."); //сообщение, что запустили поток
    // UART 1
    // PINS 10 (PD0) and 11 (PD1)
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART);

    MAP_GPIOPinConfigure(GPIO_PD0_U1RX);
    MAP_GPIOPinConfigure(GPIO_PD1_U1TX);
    MAP_GPIOPinTypeUART(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    MAP_UARTConfigSetExpClk(UART_BASE, MAP_SysCtlClockGet(),UART_SPEED,
                            UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_TWO | UART_CONFIG_PAR_NONE);
    MAP_UARTDisable(UART_BASE);
    MAP_UARTTxIntModeSet(UART_BASE, UART_TXINT_MODE_EOT);
    MAP_UARTIntEnable(UART_BASE, UART_INT_RX /*| UART_INT_TX*/);
    MAP_IntEnable(INT_UART);
    MAP_UARTEnable(UART_BASE);
    MAP_UARTFIFODisable(UART_BASE);
    /*Clear TX buffer*/
    TX1buff.buff = TX1buff_mem;
    TX1buff.size = sizeof(TX1buff_mem);
    bsp_vpu_buff_flush(&TX1buff);
    /*Clear RX buffer*/
    RX1buff.buff = RX1buff_mem;
    RX1buff.size = sizeof(RX1buff_mem);
    bsp_vpu_buff_flush(&RX1buff);
    /*Clear buff VPU*/
    memset(&dataVpu,0,sizeof(dataVpu));

//create sync mutex
if (tn_mutex_create(&led_mutex, TN_MUTEX_ATTR_INHERIT, 0) != TERR_NO_ERR)
    {
        dbg_puts("tn_mutex_create(&led_mutex) error");
        dbg_trace();
        tn_halt();
    }
// tn_mutex_lock(&led_mutex, TN_WAIT_INFINITE);
// tn_mutex_unlock(&led_mutex);
// create task
if (tn_task_create(&task_VPU_tcb, &task_VPU_func, TASK_VPU_PRI,
        &task_VPU_stk[TASK_VPU_STK_SZ - 1], TASK_VPU_STK_SZ, 0,
        TN_TASK_START_ON_CREATION) != TERR_NO_ERR)
    {
        dbg_puts("tn_task_create(&task_VPU_tcb) error");
        goto err;
    }

    dbg_puts("[done]");
    return;
err:
    dbg_trace();
    tn_halt();
}
//--------------------------------------------------------------------------
/* function is called from interrupt UART uart1_int_handler
*/
static BOOL tx1_pkt_snd_one()
{
if (!TX1buff.cnt){
  TX1buff.stat.EMPTY = TRUE;
  MAP_UARTIntDisable(UART_BASE, UART_INT_TX);
  return FALSE;
  }
MAP_UARTCharPutNonBlocking(UART_BASE, *(TX1buff.pRD));
TX1buff.cnt--;
if (++TX1buff.pRD >= (TX1buff.buff + TX1buff.size))
  TX1buff.pRD = TX1buff.buff;
MAP_UARTIntEnable(UART_BASE, UART_INT_TX);
return TRUE;
}
//--------------------------------------------------------------------------
/*receiver UART1*/
/*void receiverUART1(void)
{
MAP_UARTIntClear(UART_BASE, UART_INT_RX|UART_INT_RT);
long  recByte;
while(MAP_UARTCharsAvail(UART_BASE)) // Do byte FIFO
  {
  recByte = MAP_UARTCharGetNonBlocking(UART_BASE);
  }//while end
}*/
//------------------------------------------------------------------------------
/*Interrupt UART 1*/ // здесь все что приходит и отправляеться с UART
void uart1_int_handler()
{
unsigned long const ist = MAP_UARTIntStatus(UART_BASE, TRUE);

if (ist & UART_INT_RX){
  MAP_UARTIntClear(UART_BASE, UART_INT_RX);
  for (;;)
    {
    long const err = MAP_UARTCharGetNonBlocking(UART_BASE);
    if (err == -1)
      break;
    *(RX1buff.pWR) = err;
    RX1buff.stat.EMPTY = FALSE;
    if (++RX1buff.cnt >= RX1buff.size){
      RX1buff.stat.FULL = TRUE;
      break;
      }
    if (++RX1buff.pWR >= (RX1buff.buff + RX1buff.size))
      RX1buff.pWR = RX1buff.buff;
    }
  }
if (ist & UART_INT_TX){
  MAP_UARTIntClear(UART_BASE, UART_INT_TX);
  if(tx1_pkt_snd_one()==FALSE){
    //hw_delay(10);
    RST485_OFF();
    }
  }
}
//------------------------------------------------------------------------------
/*  flush Buffers VPU*/
void bsp_vpu_buff_flush (UART_SBUF *buf)
{
    buf->pRD = buf->buff;
    buf->pWR = buf->buff;
    buf->cnt = 0;

    buf->stat.EMPTY = TRUE;
    buf->stat.FULL  = FALSE;
    buf->stat.ERR   = FALSE;
    buf->stat.OERR  = FALSE;
}
/**/
BYTE Buff[50];
static BOOL DataInstall(void)
{
  PWR485_OFF(); // disabled power RS485
  TX1buff.buff = TX1buff_mem;
  TX1buff.size = sizeof(TX1buff_mem);
  bsp_vpu_buff_flush(&TX1buff);

  RX1buff.buff = RX1buff_mem;
  RX1buff.size = sizeof(RX1buff_mem);
  bsp_vpu_buff_flush(&RX1buff);
  // here these settings from flash
  dataVpu.address = ADR_VPU;
  dataVpu.satus = tlNo;
  PWR485_ON();  // enabled power RS485
return TRUE;
}
//------------------------------------------------------------------------------
/*Set LED protocol*/
static U8 setLedSetup(void *p,U8 len)
{
U8 lenght = 0;
BYTE *st = (BYTE*)p;

st[0] = 0x03; //lenght
lenght++;
st[1] = 0;
st[1] = dataVpu.led[3].On<<6|dataVpu.led[2].On<<4|
        dataVpu.led[1].On<<2|dataVpu.led[0].On;
lenght++;
//
st[2] = 0;
st[2] = dataVpu.led[7].On<<6|dataVpu.led[6].On<<4|
        dataVpu.led[5].On<<2|dataVpu.led[4].On;
lenght++;
//
st[3] = 0;
st[3] = dataVpu.led[10].On<<4|dataVpu.led[9].On<<2|
        dataVpu.led[8].On;
lenght++;
return lenght;
}
//------------------------------------------------------------------------------
/*Set status Button test function
static void SetStatusButton(void)
{
for(int i=0;i<MAX_BUTTON;i++)
  {
  if(dataVpu.rButton[i].On==bUp){
    dataVpu.rButton[i].On=bOn;
    dataVpu.led[i].On = ledOn;
    }
  if((dataVpu.rButton[i].On==bOff)||
    (dataVpu.rButton[i].On==bEnd))
    {
    dataVpu.led[i].On = ledOff;
    }
  }
}
*/
//------------------------------------------------------------------------------
/*Clear status Button*/
static void ClearStatusButtonLed(void)
{
  for(int i=0;i<MAX_BUTTON-1;i++)
  {
    dataVpu.rButton[i].On=bOff;
    dataVpu.led[i].On = ledOff;
  }
}
//------------------------------------------------------------------------------
/*update the structure TVPU */
// Обрабатываем нажатия клавиш
// Только ОДна кнопка может быть нажата в текущий момент!
static void UpdateSatus(void)
{
  static int btStat = Null;
  // bOn может быть только у 1 Кнопки
  // bUp - у нескольких нажатых
  // Ищем индексы
  int bUpIndx=0xFF;
  int bOnIndx=0xFF;
// обрабатываем кнопки фаз только по условию
if(((dataVpu.myRY)||(!dataVpu.RY))&&(checkJumpPhase())){
  dataVpu.fixBut = false;
  // кнопка нажата
  for(int i=0;i<MAX_BUTTON_PHASE;i++)
    {
    if(dataVpu.rButton[i].On==bUp) {
        bUpIndx=i; break;
      }
    }
  // кнопка зафиксирована
  for(int i=0;i<MAX_BUTTON_PHASE;i++)
    {
    if(dataVpu.rButton[i].On==bOn) {
      dataVpu.fixBut = true;
      bOnIndx=i;
      break;
      }
    }
  //новые нажатые кнопочки фиксация
  if(bUpIndx!=0xFF) {
    bOnIndx=bUpIndx;
    }
  dataVpu.bOnIndx = bOnIndx;
  // Чистим все
  for(int i=0;i<MAX_BUTTON_PHASE;i++)
    {
    if (dataVpu.rButton[i].On != bDown){
      dataVpu.rButton[i].On=bOff;
      }
    //LED OFF
    dataVpu.led[i].On=ledOff;
    }
  // восстанавливаем
  if (dataVpu.bOnIndx!=0xFF)
    dataVpu.rButton[dataVpu.bOnIndx].On=bOn;
}

// тригерный режим конопок РУ и АВТО
  if(btStat==Null){
    if(dataVpu.rButton[ButManual].On==bUp){
      dataVpu.rButton[ButManual].On=bOn;
      dataVpu.rButton[ButAUTO].On=bOff;
      btStat=One;
      }
    }else{
     if(btStat==One){
       if(dataVpu.rButton[ButAUTO].On==bUp){
        dataVpu.rButton[ButAUTO].On=bOn;
        dataVpu.rButton[ButManual].On=bOff;
        btStat=Null;
        }
      }else{
      btStat=Null;
      dataVpu.rButton[ButAUTO].On=bOff;
      dataVpu.rButton[ButManual].On=bOff;
      }
    }
}
//------------------------------------------------------------------------------
/*Command VPU*/
const VPU_COMMAND vpuCmd[] =
{
  {cmdButtun,   0x00,0x00,0x00,0x0B, 0,       FALSE},
  {cmdLed,      0x00,0x00,0x00,0x16, 0,       FALSE},
  {cmdLedSetup, 0x00,0x00,0x00,0x16, 0,       TRUE}
};
U8 testBuff[50];
/*Collect message*/
static U16 MessageCollect(U8 *pData,const U16 lenBuff, const VPU_COMMAND vpucmd)
{
U16 length_add=0,length=0;
INT_TO_TWO_CHAR crc16;
BYTE tmpbuff[20];
#pragma data_alignment=8
TMOBUS_REQUEST *pdate  = (TMOBUS_REQUEST*)pData;

pdate->add = dataVpu.address;
pdate->cmd = vpucmd.cmd;
pdate->dataStart.Byte[0]  = vpucmd.dataStartHi;
pdate->dataStart.Byte[1]  = vpucmd.dataStartLow;
pdate->dataLength.Byte[0] = vpucmd.dataLengthHi;
pdate->dataLength.Byte[1] = vpucmd.dataLengthLow;

if(pdate->cmd==cmdLedSetup){
  length_add=setLedSetup(tmpbuff,sizeof(tmpbuff));
  memcpy(&pData[sizeof(TMOBUS_REQUEST)],tmpbuff,length_add);
  }
crc16.Int = CRC16(pdate,sizeof(TMOBUS_REQUEST)+length_add);
pData[sizeof(TMOBUS_REQUEST)+length_add]   = crc16.Byte[1];
pData[sizeof(TMOBUS_REQUEST)+length_add+1] = crc16.Byte[0];
length = sizeof(TMOBUS_REQUEST)+length_add+sizeof(U16);
return length;
}
/*----------------------------------------------------------------------------*/
void DK_VPU_OS(void)
{
    DK[CUR_DK].REQ.req[VPU].spec_prog = SPEC_PROG_OC;
    DK[CUR_DK].REQ.req[VPU].work = SPEC_PROG;
    DK[CUR_DK].REQ.req[VPU].source = VPU; //SERVICE;
    DK[CUR_DK].REQ.req[VPU].presence = true;
}
/*----------------------------------------------------------------------------*/
void DK_VPU_YF(void)
{
    DK[CUR_DK].REQ.req[VPU].spec_prog = SPEC_PROG_YF;
    DK[CUR_DK].REQ.req[VPU].work = SPEC_PROG;
    DK[CUR_DK].REQ.req[VPU].source = VPU;//SERVICE;
    DK[CUR_DK].REQ.req[VPU].presence = true;
}
//------------------------------------------------------------------------------
// отключаем выходы
static void Clear_STATE(STATE *sta)
{
  memset(sta,0,sizeof(STATE));
  sta->spec_prog = SPEC_PROG_OC;
}
/*----------------------------------------------------------------------------*/
void DK_VPU_undo(void)
{
  Clear_STATE(&(DK[CUR_DK].REQ.req[VPU]));
}
/*----------------------------------------------------------------------------*/
void DK_VPU_KK(void)
{
    DK[CUR_DK].REQ.req[VPU].spec_prog = SPEC_PROG_KK;
    DK[CUR_DK].REQ.req[VPU].work = SPEC_PROG;
    DK[CUR_DK].REQ.req[VPU].source = VPU;
    DK[CUR_DK].REQ.req[VPU].presence = true;
}
/*----------------------------------------------------------------------------*/
void DK_VPU_faza(const unsigned long faz_i)
{
     DK[CUR_DK].REQ.req[VPU].work = SINGLE_FAZA;
     DK[CUR_DK].REQ.req[VPU].faza = faz_i;
     DK[CUR_DK].REQ.req[VPU].source = VPU;
     DK[CUR_DK].REQ.req[VPU].presence = true;
}
//------------------------------------------------------------------------------
/*send message and get answer*/
Type_STATUS_VPU DataExchange(void)
{
  static int step=Null;
  static U8 number = NULL,countError = NULL;
  U8 Buff[20],length;
  Type_ANS answer;

  switch(step)
      {
      case Null:
        length = MessageCollect(Buff,sizeof(Buff),vpuCmd[number]);
        if(StrPutVPU(&TX1buff,Buff,length)!=0){
          memset(Buff,0,sizeof(Buff));
          step = One;
          }
        return tlNo;
      case One:
        answer = StrGetVPU(&RX1buff,Buff,sizeof(Buff));
        if(answer&(ansOk|ansErr)){
          if(vpuCmd[number].FlagEnd){
            number = NULL;
            step = Null;
            return tlEnd;
            }
          dataVpu.satus = tlManual;
          number++;
          countError = 0;
          }
        if(answer&(ansErr|ansNoAnsw)){
          if(++countError>10){
            countError = 0;dataVpu.satus = tlNo;return tlError;} // 5 sec
          }
        step = Null;
        return tlNo;
      default:
        step = Null;
        countError = NULL;
        return tlNo;
     }
}
//------------------------------------------------------------------------------
// возвращает номер фазы по статусу ВПУ. 00xFF - если статус - не фаза
int ret_FAZA_num(Type_STATUS_VPU vpu)
{
  int ret_f=0xFF;
  switch (vpu_exch.m_to_s.vpu)
            {
            case tlPhase1: ret_f=0; break;
            case tlPhase2: ret_f=1; break;
            case tlPhase3: ret_f=2; break;
            case tlPhase4: ret_f=3; break;
            case tlPhase5: ret_f=4; break;
            case tlPhase6: ret_f=5; break;
            case tlPhase7: ret_f=6; break;
            case tlPhase8: ret_f=7; break;
            default:break;
            }
  return(ret_f);
}
//----------------------------------------------------------------------------//
// отобразить текущее состояние на светодиодах ВПУ
void showLED(void)
{
// отображаем LED на вызов кнопок
if(bOn == dataVpu.rButton[ButManual].On){// ВПУ on
  if(dataVpu.myRY){// наш ВПУ активный, отображаем ссотояние LED
    if(dataVpu.bOnIndx!=0xFF)//запрашиваемое состояние
      dataVpu.led[dataVpu.bOnIndx].On = ledBlink1;
    }
  }
// Отображаем только когда рулим сами или нет ни одного включенного ВПУ
if((dataVpu.myRY)||(!dataVpu.RY)){
  // смотрим - что щас на ДК
  if(SPEC_PROG == DK[CUR_DK].CUR.work){
    //ОС
    if (SPEC_PROG_OC == DK[CUR_DK].CUR.spec_prog)
      dataVpu.led[ButTlOff].On = ledOn;
    }
  // Одиночные фазы (режим ВПУ?)
  if(SINGLE_FAZA == DK[CUR_DK].CUR.work){
    int fz_n =  DK[CUR_DK].CUR.faza;
    if(fz_n<MAX_VPU_FAZE)
      dataVpu.led[fz_n].On = ledOn;
    }
  // Программная фазы - надо ли ее отображать вообще?
  if(PROG_FAZA == DK[CUR_DK].CUR.work){
    int fz_prog =  DK[CUR_DK].CUR.prog_faza;
    //int fz_n =     PROJ[CUR_DK].Program.fazas[fz_prog].NumFasa; // это номер фазы из шаблона
    if (fz_prog<MAX_VPU_FAZE)
      dataVpu.led[fz_prog].On = ledOn;
    }
  } // end LED Phase
// LED AUTO  & LED MANUAL
if(dataVpu.RY){ // работает другое ВПУ или уже включили РУ
  dataVpu.led[ButAUTO].On = ledOff; // LED AUTO
  if(dataVpu.myRY)
    dataVpu.led[ButManual].On = ledOn;    // мы рулим
    else
    dataVpu.led[ButManual].On = ledBlink2;// не мы рулим
  }else{
  dataVpu.led[ButAUTO].On = ledOn;    // LED AUTO
  dataVpu.led[ButTlOff].On = ledOff;  // LED OS
  dataVpu.led[ButManual].On = ledOff; // LED MANUAL
  }
}
// вызыв фаз работаем со структурой m_to_s
void DK_Phase_Call(void)
{
// режим OS
if (vpu_exch.m_to_s.vpu==tlOS) {
  DK_VPU_OS();
  }
// Фазы
int fz_n = ret_FAZA_num(vpu_exch.m_to_s.vpu);
if(fz_n!=0xFF){
  DK_VPU_faza(fz_n);
  }
}
/* Логика работы ВПУ----------------------------------------------------------*/
void VPU_LOGIC()
{
// Устанавливаем флаги РУ, кто-то рулит ДК
dataVpu.RY = vpu_exch.m_to_s.vpuOn;
//myRY - наш ВПУ рулит
if(dataVpu.RY)
      dataVpu.myRY = (vpu_exch.m_to_s.idDk==PROJ[0].surd.ID_DK_CUR) ? true : false;
    else
      dataVpu.myRY = false;
// Обратный канал от нас Мастеру
if(dataVpu.rButton[ButManual].On==bOn){ // РУ on local VPU
  vpu_exch.s_to_m.vpuOn=true;
  }else{
  if(dataVpu.rButton[ButAUTO].On==bOn){//PY отключено
    vpu_exch.s_to_m.vpuOn=false;
    }
  }
//определяем ВПУ-статус (нажатые кнопки)
if(dataVpu.bOnIndx!=0xFF)
  vpu_exch.s_to_m.vpu = (Type_STATUS_VPU)dataVpu.bOnIndx;
  else
  vpu_exch.s_to_m.vpu = tlNo; //нет нажатых кнопок
// проверка включенного РУ с мастера
if(dataVpu.RY){ // рулим - выставляем в ДК состояния
  DK_Phase_Call();
  }else{// уходим в режим авто
  //DK_VPU_undo(); // не управляем
  setPlanMode(); // exit to PLAN
  }
//Отображение LED
showLED();
}
/*---------------------------------------------------------------------------*/
static void ResetStrMaster(void)
{
memset(&vpu_exch.s_to_m, 0,sizeof(vpu_exch.s_to_m));
vpu_exch.m_to_s.vpuOn= false;
vpu_exch.m_to_s.vpu = tlNo;
}
/*---------------------------------------------------------------------------*/
// сброс параметров подключенного ВПУ
static void ResetStrSlave(void)
{
memset(&vpu_exch.s_to_m, 0,sizeof(vpu_exch.s_to_m));
vpu_exch.s_to_m.vpu = tlNo;
}
/*---------------------------------------------------------------------------*/
// полный сбросс ВПУ
void allResetVPU(void)
{
ResetStrMaster();
ResetStrSlave();
memset(&dataVpu,0,sizeof(dataVpu));
}
/*---------------------------------------------------------------------------*/
/* loop VPU*/ // все крутиться от этой функции
static void task_VPU_func(void* param)
{
  static int fStatus = 0;
  DataInstall();
  ResetStrMaster();

  for (;;)
    {
    // интервал 100мс.
    tn_task_sleep(VPU_REFRESH_INTERVAL);
    vpu_exch.m_to_s.statusNet = getStatusSURD();  //retNetworkOK(); // статус СУРД
    Type_STATUS_VPU answer = DataExchange();      // опрос ВПУ
    //ждем результатов от DataExchange
    if(answer==tlNo){
      continue;
      }
    // ВПУ подключен, отвечает
    if(answer==tlEnd){//опрос закончен можно обновить статусы
      fStatus = Null;
      if(vpu_exch.m_to_s.statusNet){ // status SURD OK
        UpdateSatus();
        VPU_LOGIC();
        }else{ // all clear mode AUTO
        ClearStatusButtonLed();
        setPlanMode(); // exit to PLAN
        }
      continue;
      }// end stat==tlEnd
    // ВПУ не подключен или ошибка опроса
    if(answer==tlError){
      if(vpu_exch.m_to_s.idDk!=PROJ[CUR_DK].surd.ID_DK_CUR){//это не наш ВПУ
        fStatus = One;
        }
      ResetStrSlave();
      ClearStatusButtonLed();
      }
    // ВПУ не подключен или выключен "ошибка опроса"
    if(fStatus==One){
      // сеть есть?
      if(vpu_exch.m_to_s.statusNet){
          if(vpu_exch.m_to_s.vpuOn){ // включен, вызывает фазы
            DK_Phase_Call(); // вызываем фазы
            }else{
            fStatus=Two;
            }
        }else{ // сети нет, отключаем светодиоды и переходим на план
        fStatus=Two;
        }
      }
    // ошибки чистим структуру и выходим
    if(fStatus==Two){
      fStatus=Null;
      ClearStatusButtonLed();
      setPlanMode(); // exit to PLAN
      }
    }
}
//------------------------------------------------------------------------------
/*
The function puts a string to UART transmit buffer.
In case of failure it returns zero.
*/
static U16 StrPutVPU(UART_SBUF *buf,const U8* str, const U16 size)
{
U16 i = 0;
//If the buffer is full - will refund the amount of bytes transferred
if (buf->stat.FULL == TRUE)
  return 0;
RST485_ON(); // on send RS485
MAP_UARTIntDisable(UART_BASE, UART_INT_TX);
  do
    {
    *(buf->pWR) = str[i++];
    if (++buf->cnt >= buf->size){
      buf->stat.FULL = TRUE;
      MAP_UARTIntEnable(UART_BASE, UART_INT_TX);
      return i;
      }
    if (++buf->pWR >= (buf->buff + buf->size))
      buf->pWR = buf->buff;
    }while ( i < size);
if (buf->stat.EMPTY == TRUE){
  //Starting the transmission if the buffer is empty
  buf->stat.EMPTY = FALSE;
  MAP_UARTCharPutNonBlocking(UART_BASE, *(buf->pRD));
  buf->cnt--;
  if (++buf->pRD >= (buf->buff + buf->size))
    buf->pRD = buf->buff;
  }
MAP_UARTIntEnable(UART_BASE, UART_INT_TX);
return i;
}
//------------------------------------------------------------------------------
/*
*The function returns a string in the buffer read from the UART
*If the buffer is nothing - it returns -1
*If you count - Returns the number of bytes read
*/
static Type_ANS StrGetVPU(UART_SBUF * buf,U08* str, U16 size)
{
S16 read_index=0;
BOOL start_parser = false;
U8 *pBegin,lenght=0;

if (buf->stat.EMPTY == TRUE) // buffer empty
  return ansNoAnsw;
MAP_UARTIntDisable(UART_BASE, UART_INT_RX);
while(size&&(buf->stat.EMPTY == FALSE))
    {
    str[read_index]=*(buf->pRD);
    buf->stat.FULL = FALSE;
    if (++buf->pRD >= (buf->buff + buf->size)) // buffer reciever
      buf->pRD = buf->buff;
    if (--buf->cnt == 0)   // is buffer empty
      buf->stat.EMPTY = TRUE;
    if(!start_parser){
      if(str[read_index] == dataVpu.address){
        pBegin  = &str[read_index];
        lenght=0;
        start_parser = true;
        }
      }
    if(start_parser)lenght++;
    read_index++;
    size--;
    };
bsp_vpu_buff_flush(buf); // clear reciever buffer
MAP_UARTIntEnable(UART_BASE, UART_INT_RX);
// check crc16 OK
if(!CRC16(pBegin,lenght)){
  return parserData(pBegin,lenght);
  }
return ansNoAnsw;
}
//------------------------------------------------------------------------------
/* Parser Data */
// bOff - кнопка не была нажата
// bOn - кнопка была нажата - зафиксированное логическое состояние
//    на основании последовательности bDown->bUp
// bDown - статус полученный с ВПУ о нажатой кнопке
// bUp - кнока отпущена
static Type_ANS parserData(U8* data, U8 size)
{
const BYTE maskButton[]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};
data++; // address ID

if(*(data)==cmdButtun){ // command anwer 01| 02 02 00 00
  data++;data++;
  U8 inMask = NULL;
  for (int j=0;j<MAX_BUTTON;j++)
    {
     if(*data&maskButton[inMask++]){
        if(dataVpu.rButton[j].On==bOff)
          dataVpu.rButton[j].On = bDown;
        ///// только для РУ
        if (j==MAX_BUTTON-1)
        if(dataVpu.rButton[j].On==bOn)
          dataVpu.rButton[j].On = bEnd;
      }
      else{
        if(dataVpu.rButton[j].On==bDown)
           dataVpu.rButton[j].On = bUp;
        ////
        if(dataVpu.rButton[j].On==bEnd)
           dataVpu.rButton[j].On = bOff;
    }
    ////////
    if(inMask>=sizeof(maskButton)){
      data++; // nex byte
      inMask = NULL;
      }
    }
  return ansOk;
  }
if(*(data)==cmdLed){ // command anwer 01| 01 03 01 00 00 6d 8e
  data++;data++;
  return ansOk;
  }
if(*(data)==cmdLedSetup){ // command answer 01| 0f 00 00 00 16 d4 05
  //data++;data++;
  return ansOk;
  }
return ansErr;
}
//------------------------------------------------------------------------------
/*return date VPU*/
//const TVPU *retDateVPU(void){return &dataVpu;}
/*запросы по ВПУ отправляем true если все ДК в системе*/
BYTE retRequestsVPU(void)
{
return ((vpu_exch.s_to_m.vpuOn)&&(vpu_exch.s_to_m.vpu!=tlNo)); //РУ on, вызвана фаза
}
/*--------------------- return text status ----------------------------*/
void retPhaseToText(char *pStr,const BYTE phase)
{
const Type_STATUS_VPU status = (Type_STATUS_VPU)phase;
switch(status)
  {
  case tlPhase1:strcpy(pStr,"1");return;
  case tlPhase2:strcpy(pStr,"2");return;
  case tlPhase3:strcpy(pStr,"3");return;
  case tlPhase4:strcpy(pStr,"4");return;
  case tlPhase5:strcpy(pStr,"5");return;
  case tlPhase6:strcpy(pStr,"6");return;
  case tlPhase7:strcpy(pStr,"7");return;
  case tlPhase8:strcpy(pStr,"8");return;
  case tlOS:strcpy(pStr,"OC");return;
  case tlAUTO:strcpy(pStr,"AUTO");return;
  case tlManual:strcpy(pStr,"MANUAL");return;
  case tlNo:
  default:strcpy(pStr,"NO");return;
  }
}
/*----------------------------------------------------------------------------*/
WORD retTextToPhase(char *pStr)
{
if(strcmp(pStr,"1") == 0)return tlPhase1;
if(strcmp(pStr,"2") == 0)return tlPhase2;
if(strcmp(pStr,"3") == 0)return tlPhase3;
if(strcmp(pStr,"4") == 0)return tlPhase4;
if(strcmp(pStr,"5") == 0)return tlPhase5;
if(strcmp(pStr,"6") == 0)return tlPhase6;
if(strcmp(pStr,"7") == 0)return tlPhase7;
if(strcmp(pStr,"8") == 0)return tlPhase8;
if(strcmp(pStr,"OC") == 0)return tlOS;
if(strcmp(pStr,"AUTO") == 0)return tlAUTO;
if(strcmp(pStr,"MANUAL") == 0)return tlManual;
return tlNo;
}
/*----------------------------------------------------------------------------*/
BYTE retOnVPU(void){return vpu_exch.s_to_m.vpuOn;}
/*----------------------------------------------------------------------------*/
Type_STATUS_VPU retStateVPU(void){return vpu_exch.s_to_m.vpu;}
/*----------------------------------------------------------------------------*/
// состояния светофдиодов на ВПУ
WORD retStatusLed(void)
{
WORD led = 0;
tn_mutex_lock(&led_mutex, TN_WAIT_INFINITE);
led = dataVpu.led[ButPhase8].On<<14|dataVpu.led[ButPhase7].On<<12|
      dataVpu.led[ButPhase6].On<<10|dataVpu.led[ButPhase5].On<<8|
      dataVpu.led[ButPhase4].On<<6|dataVpu.led[ButPhase3].On<<4|
      dataVpu.led[ButPhase2].On<<2|dataVpu.led[ButPhase1].On<<0;
tn_mutex_unlock(&led_mutex);
return led;
}
/*----------------------------------------------------------------------------*/
// отобразить сотояние светодиодов из вне
void setStatusLed(const WORD stLed)
{
const BYTE MASK = 0x03;
dataVpu.led[ButPhase8].On = (TYPE_LED_SATUS)((stLed>>14)&MASK);
dataVpu.led[ButPhase7].On = (TYPE_LED_SATUS)((stLed>>12)&MASK);
dataVpu.led[ButPhase6].On = (TYPE_LED_SATUS)((stLed>>10)&MASK);
dataVpu.led[ButPhase5].On = (TYPE_LED_SATUS)((stLed>>8)&MASK);
dataVpu.led[ButPhase4].On = (TYPE_LED_SATUS)((stLed>>6)&MASK);
dataVpu.led[ButPhase3].On = (TYPE_LED_SATUS)((stLed>>4)&MASK);
dataVpu.led[ButPhase2].On = (TYPE_LED_SATUS)((stLed>>2)&MASK);
dataVpu.led[ButPhase1].On = (TYPE_LED_SATUS)((stLed>>0)&MASK);
}
/*----------------------------------------------------------------------------*/