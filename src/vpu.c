/*****************************************************************************
*
* � 2015 Cyber-SB. Written by Alex Tuzhikov.
*
******************************************************************************/
#include <string.h>
#include "vpu.h"
#include "crc16.h"
#include "pins.h"
#include "dk/dk.h"

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
// �������� ������� �����.
// 1) ���� �� ������ �������� ������ �� - 
//      s_to_m.vpuOn=1
// 2) � s_to_m.vpu -������ ���, � ��-���� - ������� ������� ������
//      ��� tlNo (������ �� ������. ��� ��������)
// 3) ���� m_to_s.vpuOn==1 , �� � ���� ���-�� ����� �� ���
// 4) ���� m_to_s.vpu �� ����� tlNo, ��������������� ��������������� 
//    ���������.
//
//
//
//
VPU_EXCHANGE  vpu_exch;
TVPU dataVpu;

/////////
VPU_EXCHANGE  vpu_exchN[VPU_COUNT]; 
TVPU          dataVpuN[VPU_COUNT];
int           cur_vpu=0;  

/*---------- Functions----------------------------------------------------*/
void Switch_VPU_Context(int vpu_new)
{
  // 1 ��������� ������� ��������
  memcpy(&vpu_exchN[cur_vpu], &vpu_exch, sizeof(vpu_exch));
  memcpy(&dataVpuN[cur_vpu],  &dataVpu, sizeof(dataVpu));
  //2   ��������� �� ����� ��������
  memcpy(&vpu_exch, &vpu_exchN[vpu_new], sizeof(vpu_exch));
  memcpy(&dataVpu, &dataVpuN[vpu_new],  sizeof(dataVpu));
  ////
  cur_vpu = vpu_new;
  
}
//------------------------------------------------------------------------------
void vpu_init() // ��� ��� �� ������������� UART � ������� ����� tn_kernel
{
    dbg_printf("Initializing VPU UART..."); //���������, ��� ��������� �����
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
/*Interrupt UART 1*/ // ����� ��� ��� �������� � ������������� � UART
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
static void ClearStatusButton(void)
{
  for(int i=0;i<MAX_BUTTON-1;i++)
  {
    dataVpu.rButton[i].On=bOff;
    dataVpu.led[i].On = ledOff;
  }
}
//------------------------------------------------------------------------------
/*update the structure TVPU */
// ������������ ������� ������
// ������ ���� ������ ����� ���� ������ � ������� ������!
static void UpdateSatus(void)
{
  // bOn ����� ���� ������ � 1 ������
  // bUp - � ���������� �������
  // ���� ������� 
  int bUpIndx=0xFF;
  int bOnIndx=0xFF;
  for(int i=0;i<MAX_BUTTON-1;i++) {
    if(dataVpu.rButton[i].On==bUp) {
        bUpIndx=i; break;
    }
  }
  ////
  for(int i=0;i<MAX_BUTTON-1;i++) {
    if(dataVpu.rButton[i].On==bOn) {
        bOnIndx=i; break;
    }
  }
  /////////////  
  if (bUpIndx!=0xFF) {
    //����� ������� ��������
    bOnIndx=bUpIndx;
  }
  dataVpu.bOnIndx = bOnIndx; 
  // ������ ��� 
  for(int i=0;i<MAX_BUTTON-1;i++) {
    if (dataVpu.rButton[i].On != bDown)
      dataVpu.rButton[i].On=bOff;
    ///
    dataVpu.led[i].On=ledOff;
  }
  // ���������������
  if (dataVpu.bOnIndx!=0xFF)
    dataVpu.rButton[dataVpu.bOnIndx].On=bOn;
  ////////////////////////////  
  // ���� ���� �������
  if (bOnIndx!=0xFF) 
     dataVpu.rButton[bOnIndx].On=bOn;
  //////////////////////
  // ��� �� - ������ � 2-�� ������������ �����������
  if(dataVpu.rButton[ButManual].On==bUp)
     dataVpu.rButton[ButManual].On=bOn; 
  /////////////
  
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
    DK[CUR_DK].REQ.req[VPU].source = SERVICE;
    DK[CUR_DK].REQ.req[VPU].presence = true;
    // ���������� ����� �� ��
    for (int i_dk=0; i_dk<DK_N; i_dk++)
           DK[i_dk].OSSOFT = true;
}
/*----------------------------------------------------------------------------*/
void DK_VPU_YF(void)
{
    DK[CUR_DK].REQ.req[VPU].spec_prog = SPEC_PROG_YF;
    DK[CUR_DK].REQ.req[VPU].work = SPEC_PROG;
    DK[CUR_DK].REQ.req[VPU].source = SERVICE;
    DK[CUR_DK].REQ.req[VPU].presence = true;
}
//------------------------------------------------------------------------------
// ��������� ������
static void Clear_STATE(STATE *sta)
{
  memset(sta,0,sizeof(STATE));
  sta->spec_prog = SPEC_PROG_OC;
}
/*----------------------------------------------------------------------------*/
void DK_VPU_undo(void)
{
  
  Clear_STATE(&(DK[CUR_DK].REQ.req[VPU]));
  // ���������� ����� �� ��
  for (int i_dk=0; i_dk<DK_N; i_dk++)
           DK[i_dk].OSSOFT = false;
}
/*----------------------------------------------------------------------------*/
void DK_VPU_KK(void)
{
    DK[CUR_DK].REQ.req[VPU].spec_prog = SPEC_PROG_KK;
    DK[CUR_DK].REQ.req[VPU].work = SPEC_PROG;
    DK[CUR_DK].REQ.req[VPU].source = SERVICE;
    DK[CUR_DK].REQ.req[VPU].presence = true;
}
/*----------------------------------------------------------------------------*/
void DK_VPU_faza(const unsigned long faz_i)
{
     DK[CUR_DK].REQ.req[VPU].work = SINGLE_FAZA;
     DK[CUR_DK].REQ.req[VPU].faza = faz_i;
     DK[CUR_DK].REQ.req[VPU].source = SERVICE;
     DK[CUR_DK].REQ.req[VPU].presence = true;
}
//------------------------------------------------------------------------------
/*send message and get answer*/
Type_STATUS_VPU DataExchange(void)
{
  static int step=Null;
  static U8 number = NULL,countError = NULL,TimeTest = NULL;
  U8 Buff[20],length;
  Type_ANS answer;
  ////////////////
  ///////////////
  switch(step)
      {
      case Null:
        length = MessageCollect(Buff,sizeof(Buff),vpuCmd[number]);
        if(StrPutVPU(&TX1buff,Buff,length)!=0){
          memset(Buff,0,sizeof(Buff));
          step = 1;
          //step2 = 1;
          }
        return tlNo;
      case 1:
        answer = StrGetVPU(&RX1buff,Buff,sizeof(Buff));
        if(answer&ansOk|ansErr){
          if(vpuCmd[number].FlagEnd){
            number = NULL;
            step = 0;
            //step2 = 0;
            return tlEnd;
            }
          dataVpu.satus = tlManual;
          number++;
          countError = 0;
          }
        if(++countError>50)dataVpu.satus = tlOff; // 5 sec
        step = Null;
        return tlNo;
      default:
        step = Null;
        countError = NULL;
        return tlNo;
     }
}
//------------------------------------------------------------------------------
// ���������� ����� ���� �� ������� ���. 00xFF - ���� ������ - �� ����
int ret_FAZA_num(Type_STATUS_VPU vpu)
{
  int ret_f=0xFF;  
  switch (vpu_exch.m_to_s.vpu){
            case tlPhase1: ret_f=0; break;
            case tlPhase2: ret_f=1; break;
            case tlPhase3: ret_f=2; break;
            case tlPhase4: ret_f=3; break;
            case tlPhase5: ret_f=4; break;
            case tlPhase6: ret_f=5; break;
            case tlPhase7: ret_f=6; break;
            case tlPhase8: ret_f=7; break;
            default: 
  }///////////
  return(ret_f);
}
//------------------------------------------------------------------------------
// ������ ������ ���
void VPU_LOGIC()
{
  // ������������� ����� ��
  dataVpu.RY = vpu_exch.m_to_s.vpuOn;
  //myRY - ��� ��� �����
  if (dataVpu.RY) 
      dataVpu.myRY = (vpu_exch.m_to_s.idDk==PROJ[0].surd.ID_DK_CUR) ? 1 : 0;
    else
      dataVpu.myRY = 0;
  /////////////////
  ////////////////
  // �������� ����� �� ��� �������  
  if(dataVpu.rButton[ButManual].On==bOn  ){
     vpu_exch.s_to_m.vpuOn=1; 
  } else { //PY ���������
     vpu_exch.s_to_m.vpuOn=0; 
  }
  ///////
  ///// ���������� ���-������
  if (dataVpu.bOnIndx!=0xFF) 
     vpu_exch.s_to_m.vpu = dataVpu.bOnIndx;   
   else
    vpu_exch.s_to_m.vpu = tlNo; //��� ������� ������
  /////////////////////
    //////////////////////////
    // �������� ����������� �� � �������
    // ���-�� ����� - ���������� � �� ���������
    if (dataVpu.RY) {
      if (vpu_exch.m_to_s.vpu==tlYellBlink) {
        DK_VPU_YF();
      }
      /////////
      if (vpu_exch.m_to_s.vpu==tlOff) {
        DK_VPU_OS();
      }
      //////
      // ����
      int fz_n = ret_FAZA_num(vpu_exch.m_to_s.vpu);
      if (fz_n!=0xFF) {
            DK_VPU_faza(fz_n);
      }
      /////////////
      // ������ ����������� - ������������
      if (vpu_exch.m_to_s.vpu > tlYellBlink)
        DK_VPU_undo();
      //////////////
    }
    else
      DK_VPU_undo();
    //////////////////////
    //////////////////////////
    // �����������
    // ������� ��
    if (bOn == dataVpu.rButton[ButManual].On) {
      if (dataVpu.myRY)
          dataVpu.led[ButManual].On = ledOn;
      else
          dataVpu.led[ButManual].On = ledBlink1;
    } else {
      dataVpu.led[ButManual].On = ledOff;
    }
    ////////////////  
    // ����� ��!!, ���������� ���������
    if (dataVpu.myRY)  {
         // ������������� ���������
         if (dataVpu.bOnIndx!=0xFF) 
            dataVpu.led[dataVpu.bOnIndx].On = ledBlink1;
         /////////////////////
         // ������� ��������� 
        // ������� - ��� ��� �� ��
        if (SPEC_PROG == DK[CUR_DK].CUR.work){
          //��
          if (SPEC_PROG_YF == DK[CUR_DK].CUR.spec_prog) 
                dataVpu.led[ButYllBlink].On = ledOn; //������� ���������
          //��
          if (SPEC_PROG_OC == DK[CUR_DK].CUR.spec_prog) 
                dataVpu.led[ButTlOff].On = ledOn;  
          ////////
        }
        ///////////////////
        // ��������� ���� (����� ���?)
        if (SINGLE_FAZA == DK[CUR_DK].CUR.work){
            int fz_n =  DK[CUR_DK].CUR.faza;
            if (fz_n<MAX_VPU_FAZE)
                  dataVpu.led[fz_n].On = ledOn;  
        }
        ////////////////////////
        // ����������� ���� - ���� �� �� ���������� ������?
        if (PROG_FAZA == DK[CUR_DK].CUR.work){
            int fz_prog =  DK[CUR_DK].CUR.prog_faza;
            int fz_n =   PROJ[CUR_DK].Program.fazas[fz_prog].NumFasa;
            if (fz_n<MAX_VPU_FAZE)
                  dataVpu.led[fz_n].On = ledOn;  
        }
        ////////////////////////
        
        
    }
    /////////////
}
//------------------------------------------------------------------------------
/* loop VPU*/ // ��� ��������� �� ���� �������
static void task_VPU_func(void* param)
{
  DataInstall();
  //////
  memset(&vpu_exch.s_to_m, 0,sizeof(vpu_exch.s_to_m));
  vpu_exch.m_to_s.vpuOn= 0;
  vpu_exch.m_to_s.vpu = tlNo;   
  for (int i=0; i<VPU_COUNT; i++) {
    vpu_exchN[i].m_to_s.vpu = tlNo;   
  }
  /////
  for (;;)
    {
      tn_task_sleep(VPU_REFRESH_INTERVAL);
      //////
      for (int i=0; i<VPU_COUNT; i++) {
        Switch_VPU_Context(i);
        if(DataExchange()==tlEnd){
          UpdateSatus();
          VPU_LOGIC();
        }
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
// bOff - ������ �� ���� ������ 
// bOn - ������ ���� ������ - ��������������� ���������� ���������
//    �� ��������� ������������������ bDown->bUp
// bDown - ������ ���������� � ��� � ������� ������
// bUp - ����� ��������
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
        ///// ������ ��� ��
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
const TVPU *retDateVPU(void){return &dataVpu;}
/*������� �� ��� ���������� true ���� ��� �� � �������*/
BYTE retRequestsVPU(void)
{
  return false;
}
/*--------------------- return text status ----------------------------*/
void retTextStatusVPU(char *pStr,const Type_STATUS_VPU status)
{
switch(status)
  {
  case tlNo:strcpy(pStr,"NO");return;
  case tlPhase1:strcpy(pStr,"PHASE1");return;
  case tlPhase2:strcpy(pStr,"PHASE2");return;
  case tlPhase3:strcpy(pStr,"PHASE3");return;
  case tlPhase4:strcpy(pStr,"PHASE4");return;
  case tlPhase5:strcpy(pStr,"PHASE5");return;
  case tlPhase6:strcpy(pStr,"PHASE6");return;
  case tlPhase7:strcpy(pStr,"PHASE7");return;
  case tlPhase8:strcpy(pStr,"PHASE8");return;
  case tlOff:strcpy(pStr,"OFF");return;
  case tlYellBlink:strcpy(pStr,"YBLINK");return;
  case tlManual:strcpy(pStr,"MANUAL");return;
  }
}
/**/
/*static void OSDk(const BOOL Enabled)
{
  if(Enabled){
  for (int i_dk=0; i_dk<DK_N; i_dk++)
           DK[i_dk].OS = true;
  }else{
  for (int i_dk=0; i_dk<DK_N; i_dk++)
           DK[i_dk].OS = false;
  }
} */
/**/
/*static void YELDk(const char Enabled)
{
  if(Enabled){
  for (int i_dk=0; i_dk<DK_N; i_dk++)
           DK[i_dk].YF = true;
  }else{
  for (int i_dk=0; i_dk<DK_N; i_dk++)
           DK[i_dk].YF = false;
  }
}*/