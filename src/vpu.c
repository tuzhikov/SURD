/*****************************************************************************
*
* © 2015 Cyber-SB. Written by Alex Tuzhikov.
*
******************************************************************************/
#include <string.h>
#include <stdio.h>
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
VPU_EXCHANGE  vpu_exch;
TVPU dataVpu;
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
static Type_ANS parserData(U8* data, U8 size);
static Type_ANS StrGetVPU(UART_SBUF * buf,U08* str, U16 size);
static void DK_VPU_undo(void);
static void ResetStrSlave(void);
static void ResetStrMaster(void);
static void ClearStatusLed(void);
static void ClearStatusLedPhase(void);
static void ClearStatusButton(void);
static void ClearStatusButtonPhase(void);

// Structure for exchange between master and slave
// Алгоритм общения такой.
// 1) Если на пульте включена кнопка РУ -
//      s_to_m.vpuOn=1
// 2) В s_to_m.vpu -статус ВПУ, а по-сути - текущая нажатая кнопка
//      или tlNo (ничего не нажато. нет запросов)
// 3) Если m_to_s.vpuOn==1 , то в сети кто-то рулит по ВПУ
// 4) Если m_to_s.vpu не равно tlNo, устанавливается соответствующее
//    состояние.
/*----------------------------------------------------------------------------*/
/*
*                               Functions
*/
/*----------------------------------------------------------------------------*/
// Обновляет структуру m_t_s для ВСЕх ВПУ
static void update_m_t_s(MASTER_SLAVE_VPU *mts)
{
memcpy(&vpu_exch.m_to_s, mts, sizeof(vpu_exch.m_to_s));
}
/*----------------------------------------------------------------------------*/
// обновить данные включить фазу
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
update_m_t_s(&mts);
}
/*----------------------------------------------------------------------------*/
// сбросить режим ВПУ вернуться к АВТО
void ReturnToWorkPlan(void)
{
// сбросс запроса ВПУ
DK_VPU_undo();
// сбросс структуры m_to_s
updateCurrentDatePhase(0,0,0,tlNo);
// сбросс значений фазы структуры s_to_m
vpu_exch.s_to_m.vpu = tlNo;
// сбросс активного ВПУ
dataVpu.RY = dataVpu.myRY = false;
// установить флаг перехода на
dataVpu.nextPhase = NO_EVENTS;
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
/*----------------------------------------------------------------------------*/
/* function is called from interrupt UART uart1_int_handler*/
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
/*----------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------*/
/*Install*/
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
/*----------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------*/
/*Clear status Button phase*/
static void ClearStatusButtonPhase(void)
{
dataVpu.bOnIndx=NO_EVENTS; // сбросс состояние кнопок
for(int i=0;i<MAX_BUTTON_PHASE;i++)
    {
    dataVpu.rButton[i].On=bOff;
    }
}
/*----------------------------------------------------------------------------*/
/*Clear status Button*/
static void ClearStatusButton(void)
{
  for(int i=0;i<END_BUTTON;i++)
    {
    dataVpu.rButton[i].On=bOff;
    }
  dataVpu.bOnIndx=NO_EVENTS; // сбрасываем все события по нажатой кнопке
}
/*----------------------------------------------------------------------------*/
/*Clear status LED Phase*/
static void ClearStatusLedPhase(void)
{
for(int i=0;i<MAX_BUTTON_PHASE;i++)
  {
  dataVpu.rLed[i].On = dataVpu.led[i].On = ledOff;
  }
}
/*----------------------------------------------------------------------------*/
/*Clear status LED*/
static void ClearStatusLed(void)
{
for(int i=0;i<MAX_LED;i++)
  {
  dataVpu.rLed[i].On = dataVpu.led[i].On = ledOff;
  }
}
/*----------------------------------------------------------------------------*/
/*Command VPU*/
const VPU_COMMAND vpuCmd[] =
{
  {cmdButtun,   0x00,0x00,0x00,0x0B, 0,       FALSE},
  {cmdLed,      0x00,0x00,0x00,0x16, 0,       FALSE},
  {cmdLedSetup, 0x00,0x00,0x00,0x16, 0,       TRUE}
};
U8 testBuff[50];
/*----------------------------------------------------------------------------*/
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
static void DK_VPU_undo(void)
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
/*----------------------------------------------------------------------------*/
/*send message and get answer*/
Type_ANS DataExchange(void)
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
        return ansNull;
      case One:
        answer = StrGetVPU(&RX1buff,Buff,sizeof(Buff));
        if(answer&(ansOk)){  //(ansOk|ansErr)
          if(vpuCmd[number].FlagEnd){
            dataVpu.satus = tlEnd;number = NULL;step = Null;
            return ansOk;
            }
          countError = 0;
          number++;
          }
        if(answer&(ansErr|ansNoAnsw)){
          if(++countError>10){
            countError = 0;dataVpu.satus = tlNo;return ansErr;} // 5 sec
          }
        step = Null;
        return ansNull;
      default:
        step = Null;
        countError = NULL;
        return ansErr;
     }
}
/*----------------------------------------------------------------------------*/
// возвращает номер фазы по статусу ВПУ. 00xFF - если статус - не фаза
static int retNumPhase(const Type_STATUS_VPU vpu)
{
int ret_f=0xFF;
if(vpu<tlOS)ret_f=(int)vpu;
return(ret_f);
}
/*----------------------------------------------------------------------------*/
/*update the structure TVPU */
// Обрабатываем нажатия клавиш
// Только Одна кнопка может быть нажата в текущий момент!
static void updateSatusButton(void)
{
static BYTE btStat = Null;// режимы работы кнопок РУ и АВТО
static BYTE btStep = Null;
static BYTE countTime = 0; // increment 0.1S

dataVpu.stepbt = btStep;
// обрабатываем кнопки фаз только активного ВПУ
if(dataVpu.myRY){

  switch(btStep)
    {
    // переводим ДК в режим ВПУ
    case Null:
      if(PROG_FAZA == DK[CUR_DK].CUR.work){
        dataVpu.nextPhase = DK[CUR_DK].CUR.prog_faza;//текущая фаза вызываем
        }
    case One:// кнопка нажата
      for(int i=0;i<MAX_BUTTON_PHASE;i++)
        {
        if(dataVpu.rButton[i].On==bUp){
          dataVpu.rButton[i].On = bOn;
          // зафиксировали кнопку <status On>
          dataVpu.bOnIndx = i;
          btStep = Two;
          break;
          }
        }
      break;
    case Two:// delay 1 S
      if(++countTime>10){
        countTime=0;
        if(PROG_FAZA == DK[CUR_DK].CUR.work)btStep = Three; // переход ПЛАН->ФАЗА (первый вызов в ВПУ)
        if(SINGLE_FAZA == DK[CUR_DK].CUR.work)btStep = For; // переход ФАЗА->ФАЗА
        }
      break;
    case Three: //переход ПЛАН->ФАЗА
      if((DK[CUR_DK].CUR.source==VPU)&&(SINGLE_FAZA == DK[CUR_DK].CUR.work)){
        if(dataVpu.nextPhase==DK[CUR_DK].CUR.faza){
          btStep = For;}
        }
      break;
    case For: // переход ФАЗА->ФАЗА
      // защита от зависания в этом шаге машины
      if((DK[CUR_DK].CUR.source==PLAN)||(dataVpu.bOnIndx==0xFF)){btStep = Null;break;}
      //отправляем на вызов фазы
      dataVpu.nextPhase=dataVpu.bOnIndx;
      // проверяем переход по фазам
      if(dataVpu.nextPhase==DK[CUR_DK].CUR.faza){btStep = Five;}
      break;
    case Five:
    default:
      ClearStatusLedPhase();
      ClearStatusButtonPhase();
      btStep = One; // на опрос кнопок
      break;
    }
  }else{
  // reset satus button phase
  ClearStatusButtonPhase();
  btStep = Null;
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
     //был сбросс сотояния кнопок
     if((dataVpu.rButton[ButAUTO].On==bOff)&&
       (dataVpu.rButton[ButManual].On==bOff)){
       btStat=Null;
       }
    }
}
/*----------------------------------------------------------------------------*/
static void showManualLED(void)
{
// LED AUTO  & LED MANUAL
if(dataVpu.RY){ // работает другое ВПУ или уже включили РУ
  dataVpu.led[ButAUTO].On = ledOff; // LED AUTO
  if(dataVpu.myRY)
    dataVpu.led[ButManual].On = ledOn;    // мы рулим
    else
    dataVpu.led[ButManual].On = ledBlink2;// не мы рулим
  }else{
  dataVpu.led[ButAUTO].On   = ledOn;  // LED AUTO
  dataVpu.led[ButManual].On = ledOff; // LED MANUAL
  }
}
/*----------------------------------------------------------------------------*/
// отобразить текущее состояние на светодиодах ВПУ
static void showPhaseLED(void)
{
// чистим LED Phase
ClearStatusLedPhase();
// Отображаем только когда рулим сами ВПУ, в других случаях состояние LED приходит с NEt
if(dataVpu.myRY){
  // отображаем LED на вызов кнопок
  if(vpu_exch.s_to_m.vpuOn){// ВПУ on локкальное
    if(dataVpu.bOnIndx!=0xFF)//запрашиваемое состояние
      dataVpu.led[dataVpu.bOnIndx].On = ledBlink1;
    }
  }
// Одиночные фазы(режим ВПУ)
if(SINGLE_FAZA == DK[CUR_DK].CUR.work){
  const int fz_n =  DK[CUR_DK].CUR.faza;
  if(fz_n<MAX_VPU_FAZE)
  dataVpu.led[fz_n].On = ledOn;
  }else{ // Программная фазы - надо ли ее отображать вообще?
     // Программная фазы в плане
     if(PROG_FAZA == DK[CUR_DK].CUR.work){
        const int fz_prog =  DK[CUR_DK].CUR.prog_faza;
        if(fz_prog<MAX_VPU_FAZE)
          dataVpu.led[fz_prog].On = ledOn;
        }else
        if(SPEC_PROG == DK[CUR_DK].CUR.work){//ОС
          if(SPEC_PROG_OC == DK[CUR_DK].CUR.spec_prog)
            dataVpu.led[ButTlOff].On = ledOn;
            }else{dataVpu.led[ButTlOff].On = ledOff;}  // LED OS
  }
}
/*---------------------------------------------------------------------------*/
// вызыв фаз работаем со структурой m_to_s
static void DK_Phase_Call(void)
{
// режим OS
if (vpu_exch.m_to_s.vpu==tlOS) {
  DK_VPU_OS();
  }
// Фазы
const int phase = retNumPhase(vpu_exch.m_to_s.vpu);
if(phase!=0xFF){
  DK_VPU_faza(phase);
  }
}
/*---------------------------------------------------------------------------*/
/* Логика работы ВПУ */
static void VPU_LOGIC()
{
// включить ВПУ Обратный канал от нас Мастеру
if(dataVpu.rButton[ButManual].On==bOn){ // РУ on local VPU
  vpu_exch.s_to_m.vpuOn=true;
  }else{
  if(dataVpu.rButton[ButAUTO].On==bOn){//PY отключено
    vpu_exch.s_to_m.vpuOn=false;
    }
  }
// Устанавливаем флаги РУ, кто-то рулит ДК
dataVpu.RY = vpu_exch.m_to_s.vpuOn;
//myRY - наш ВПУ рулит
if(dataVpu.RY)
      dataVpu.myRY = (vpu_exch.m_to_s.idDk==PROJ[0].surd.ID_DK_CUR) ? true : false;
    else
      dataVpu.myRY = false;
//определяем ВПУ-статус (вызываемая фаза)
if(dataVpu.nextPhase<tlOS) //реальная фаза
  vpu_exch.s_to_m.vpu = (Type_STATUS_VPU)dataVpu.nextPhase;
  else
  vpu_exch.s_to_m.vpu = tlNo;//не вызываем
//Отображение LED
showPhaseLED();
showManualLED();
}
/*---------------------------------------------------------------------------*/
static void switchPhaseOnVPU(void)
{
// проверка включенного РУ с мастера
if(dataVpu.RY){ // рулим - выставляем в ДК состояния
  DK_Phase_Call();
  }else{// уходим в режим авто
  ReturnToWorkPlan(); // exit to PLAN
  }
}
/*---------------------------------------------------------------------------*/
// сброс параметров управления ВПУ
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
vpu_exch.m_to_s.vpuOn= false;//?
vpu_exch.s_to_m.vpu = tlNo;
}
/*---------------------------------------------------------------------------*/
/* loop VPU*/ // все крутиться от этой функции
static void task_VPU_func(void* param)
{
  static BYTE stepVPU = Null;
  static WORD answer = ansNull;
  static BYTE DelayTime = Null;
  static BYTE OldSourse;
  DataInstall();
  ResetStrSlave();
  ResetStrMaster();

  for (;;)
    {
    // интервал 100мс.
    tn_task_sleep(VPU_REFRESH_INTERVAL);
    // получить статус СУРД
    vpu_exch.m_to_s.statusNet =  getFlagStatusSURD(); // статус СУРД
    // опрос ВПУ
    answer = DataExchange();

    switch(stepVPU)
      {
      case Null:// активный ВПУ
        if(!vpu_exch.m_to_s.statusNet){
          OldSourse=DK[CUR_DK].CUR.source;DelayTime=7;stepVPU = Three;break;}
        if(answer&ansOk){
          updateSatusButton();
          VPU_LOGIC();
          switchPhaseOnVPU();
          } else
          if(answer&ansErr){
            if(vpu_exch.m_to_s.idDk!=PROJ[CUR_DK].surd.ID_DK_CUR){//это не наш ВПУ
              stepVPU = One;
              }else{
              stepVPU = For;
              }
            }
        break;
      case One: // пасcивный ВПУ выключен или не подключен
        if(!vpu_exch.m_to_s.statusNet){
          OldSourse=DK[CUR_DK].CUR.source;DelayTime=7;stepVPU = Three;break;}// сеть есть?
        if(answer&ansOk){stepVPU = Null; break;} // включили ВПУ
        if(vpu_exch.m_to_s.vpuOn){DK_Phase_Call();}
                            else {stepVPU = For;}
        break;
      case Three: // очистка через КК
        {
        if(--DelayTime)break;
        if((OldSourse==VPU)){
          //DK_RESTART();//
          tn_reset();
          }
        stepVPU = For;
        }
      case For: // очистка
        ResetStrSlave();         // сброс структуры опроса ВПУ.
        ResetStrMaster();        // сбросс структуры управления ВПУ
        ClearStatusLed();        // сбросс светодиодов
        ClearStatusButton();     // сбросс кнопок
        ReturnToWorkPlan();      // return to PLAN
        stepVPU = Five;
      case Five: // ни показывать LED пока не появиься статус СУРД
        if(vpu_exch.m_to_s.statusNet){ClearStatusButton();stepVPU = Null;}
        break;
      default:stepVPU = Null;
        break;
      }
    }
}
/*----------------------------------------------------------------------------*/
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
      }else{
        if(dataVpu.rButton[j].On==bDown)
           dataVpu.rButton[j].On = bUp;
        if(dataVpu.rButton[j].On==bEnd)
           dataVpu.rButton[j].On = bOff;
    }
    //
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
/*запросы по ВПУ отправляем true если все ДК в системе*/
BYTE retRequestsVPU(void)
{
return ((vpu_exch.s_to_m.vpuOn)&&(vpu_exch.s_to_m.vpu!=tlNo)); //РУ on, вызвана фаза
}
/*--------------------- return text status ----------------------------*/
void retPhaseToText(char *pStr,const BYTE leng,const BYTE phase)
{
const Type_STATUS_VPU status = (Type_STATUS_VPU)phase;
/*if(status<tlOS){
    snprintf(pStr,leng,"%u",phase);
    return;
    } */
switch(status)
  {
  case tlPhase1: strcpy(pStr,"1");return;
  case tlPhase2: strcpy(pStr,"2");return;
  case tlPhase3: strcpy(pStr,"3");return;
  case tlPhase4: strcpy(pStr,"4");return;
  case tlPhase5: strcpy(pStr,"5");return;
  case tlPhase6: strcpy(pStr,"6");return;
  case tlPhase7: strcpy(pStr,"7");return;
  case tlPhase8: strcpy(pStr,"8");return;
  case tlPhase9: strcpy(pStr,"9");return;
  case tlPhase10:strcpy(pStr,"10");return;
  case tlPhase11:strcpy(pStr,"11");return;
  case tlPhase12:strcpy(pStr,"12");return;
  case tlPhase13:strcpy(pStr,"13");return;
  case tlPhase14:strcpy(pStr,"14");return;
  case tlPhase15:strcpy(pStr,"15");return;
  case tlPhase16:strcpy(pStr,"16");return;
  case tlPhase17:strcpy(pStr,"17");return;
  case tlPhase18:strcpy(pStr,"18");return;
  case tlPhase19:strcpy(pStr,"19");return;
  case tlPhase20:strcpy(pStr,"20");return;
  case tlPhase21:strcpy(pStr,"21");return;
  case tlPhase22:strcpy(pStr,"22");return;
  case tlPhase23:strcpy(pStr,"23");return;
  case tlPhase24:strcpy(pStr,"24");return;
  case tlPhase25:strcpy(pStr,"25");return;
  case tlPhase26:strcpy(pStr,"26");return;
  case tlPhase27:strcpy(pStr,"27");return;
  case tlPhase28:strcpy(pStr,"28");return;
  case tlPhase29:strcpy(pStr,"29");return;
  case tlPhase30:strcpy(pStr,"30");return;
  case tlPhase31:strcpy(pStr,"31");return;
  case tlPhase32:strcpy(pStr,"32");return;
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
if(strcmp(pStr,"1")  == 0)return tlPhase1;
if(strcmp(pStr,"2")  == 0)return tlPhase2;
if(strcmp(pStr,"3")  == 0)return tlPhase3;
if(strcmp(pStr,"4")  == 0)return tlPhase4;
if(strcmp(pStr,"5")  == 0)return tlPhase5;
if(strcmp(pStr,"6")  == 0)return tlPhase6;
if(strcmp(pStr,"7")  == 0)return tlPhase7;
if(strcmp(pStr,"8")  == 0)return tlPhase8;
if(strcmp(pStr,"9")  == 0)return tlPhase9;
if(strcmp(pStr,"10") == 0)return tlPhase10;
if(strcmp(pStr,"11") == 0)return tlPhase11;
if(strcmp(pStr,"12") == 0)return tlPhase12;
if(strcmp(pStr,"13") == 0)return tlPhase13;
if(strcmp(pStr,"14") == 0)return tlPhase14;
if(strcmp(pStr,"15") == 0)return tlPhase15;
if(strcmp(pStr,"16") == 0)return tlPhase16;
if(strcmp(pStr,"17") == 0)return tlPhase17;
if(strcmp(pStr,"18") == 0)return tlPhase18;
if(strcmp(pStr,"19") == 0)return tlPhase19;
if(strcmp(pStr,"20") == 0)return tlPhase20;
if(strcmp(pStr,"21") == 0)return tlPhase21;
if(strcmp(pStr,"22") == 0)return tlPhase22;
if(strcmp(pStr,"23") == 0)return tlPhase23;
if(strcmp(pStr,"24") == 0)return tlPhase24;
if(strcmp(pStr,"25") == 0)return tlPhase25;
if(strcmp(pStr,"26") == 0)return tlPhase26;
if(strcmp(pStr,"27") == 0)return tlPhase27;
if(strcmp(pStr,"28") == 0)return tlPhase28;
if(strcmp(pStr,"29") == 0)return tlPhase29;
if(strcmp(pStr,"30") == 0)return tlPhase30;
if(strcmp(pStr,"31") == 0)return tlPhase31;
if(strcmp(pStr,"32") == 0)return tlPhase32;
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
tn_mutex_lock(&led_mutex, TN_WAIT_INFINITE);
dataVpu.led[ButPhase8].On = (TYPE_LED_SATUS)((stLed>>14)&MASK);
dataVpu.led[ButPhase7].On = (TYPE_LED_SATUS)((stLed>>12)&MASK);
dataVpu.led[ButPhase6].On = (TYPE_LED_SATUS)((stLed>>10)&MASK);
dataVpu.led[ButPhase5].On = (TYPE_LED_SATUS)((stLed>>8)&MASK);
dataVpu.led[ButPhase4].On = (TYPE_LED_SATUS)((stLed>>6)&MASK);
dataVpu.led[ButPhase3].On = (TYPE_LED_SATUS)((stLed>>4)&MASK);
dataVpu.led[ButPhase2].On = (TYPE_LED_SATUS)((stLed>>2)&MASK);
dataVpu.led[ButPhase1].On = (TYPE_LED_SATUS)((stLed>>0)&MASK);
tn_mutex_unlock(&led_mutex);
}
/*----------------------------------------------------------------------------*/