/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Alexey Selyutin.
*
*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include "tnkernel/tn.h"
#include "stellaris.h"
#include "adcc.h"
#include "pins.h"
#include "pref.h"
#include "debug/debug.h"
#include "version.h"
#include "dk/dk.h"
#include "memory/ds1390.h"
#include "dk/kurs.h"


#define ADC_REFRESH_INTERVAL    20
// пороговые значение состояний
#define ADC_RED_POROG           100
#define ADC_GREEN_POROG         850
// время "устаканивания" работы ДК
#define TIME_TRYES_CLEAR         5 //seconds


// adc task tcb and stack

static TN_TCB task_adc_tcb;
#pragma location = ".noinit"
#pragma data_alignment=8

static unsigned int task_adc_stk[TASK_ADC_STK_SZ];
static void task_adc_func(void* param);

//static unsigned long g_adc_s0[8];
//static unsigned long g_adc_s1[4];

// размер буфера ADC
#define ADC_BUF_N       30
// коэффициент прореживания
#define ADC_DIV_N   1
////
static int adc_div=0;
//
static ADC_DATA adc_data[ADC_BUF_N];
static unsigned char adc_pos; //позиция в массиве кругового
//static unsigned char adc_divider; // прореживание результатов АЦП
//static int adc_go=0;
///
#define UDC_BUF_N       20
static ADC_DATA udc_data[UDC_BUF_N];
static unsigned char udc_pos[UDC_BUF_N]; //позиция в массиве кругового
static unsigned char udc_pos_PW; //позиция в массиве кругового
static unsigned long udc_data_PW[UDC_BUF_N];


//static int   reboot_tryes = 0; //кол-во попыток перезапуска
//static int   ADC_STAT;
// Состояние выходов токов и напряжений по данным АЦП
unsigned char  U_STAT[U_CH_N]; //зафиксированное значение напряжения
unsigned char  I_STAT[I_CH_N]; //зафиксированное значение токов

bool  U_STAT_PW; //зафиксированное значение напряжения
bool  U_STAT_PW_last; // последнее значение

//static unsigned long  u_time[U_CH_N]; //начало импульса
static unsigned long  u_time_dif[U_CH_N]; // длительность импульса в тактах таймера
static unsigned long  u_last_change[U_CH_N]; //время последнего изменения =hw_time
// слежение за сетевым
static unsigned long  u_time_dif_PW; // длительность импульса в тактах таймера
static unsigned long  u_last_change_PW; //время последнего изменения =hw_time
//
// для токов
unsigned long  adc_middle[I_CH_N]; //усреднение состояний каналов тока
// для напряжений
unsigned long  udc_middle[U_CH_N]; //усреднение состояний каналов напряжений
unsigned long  udc_middle_PW; //усреднение состояний каналов напряжений

#define SENS_LEV  10000

static enum i_pin ipins[SENS_N] = {IPIN_UC1, IPIN_UC2,IPIN_UC3,IPIN_UC4,
                             IPIN_UC5,IPIN_UC6,IPIN_UC7,IPIN_UC8, IPIN_PW_CONTR};
unsigned long sens_plus_count[SENS_N];
unsigned long sens_zero_count[SENS_N];
unsigned long sens_count; //общий счетчик
//
//static DS1390_TIME last_reboot_time;
static unsigned long   cur_time;
//static unsigned long period_tick;
//static unsigned char pw_count=0;
static volatile  unsigned char ADC_WORK_FLAG;


void Calc_Middle();
void Calc_Middle_U();

void adc_init()
{
    dbg_printf("Initializing ADC0...");
    //
    //period_tick = MAP_SysTickPeriodGet();
    //
    //adc_divider=0;
    for (int i=0; i<U_CH_N; i++)
    {
       u_time_dif[i]=0;
       udc_pos[i]=0;
       u_last_change[i]=0;
       //sens_count[i]=0;
    }
    ///

    /////
    adc_pos=0;
    //
    //ADC_STAT=0;
    U_STAT_PW_last=0;
    U_STAT_PW=0;

    //reboot_i=123456;
    //DS1390_TIME time;
    //BOOL true_time = GetTime_DS1390(&last_reboot_time);
    /////
    // CCP INIT!!!!
    /*
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
    /////////
    GPIOPinConfigure(GPIO_PD4_CCP3);

    ROM_GPIOPinTypeTimer(GPIO_PORTE_BASE, GPIO_PIN_7);  // IC1
    */
    /////////
    // Таймер на счет шима
    //MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    //MAP_SysCtlPeripheralReset(SYSCTL_PERIPH_TIMER0);
    //MAP_TimerDisable(TIMER0_BASE, TIMER_A);
    //
    //MAP_TimerConfigure(TIMER0_BASE, TIMER_CFG_32_BIT_PER);
    //MAP_TimerLoadSet(TIMER0_BASE, TIMER_A, ROM_SysCtlClockGet());
    //ROM_TimerLoadSet(TIMER1_BASE, TIMER_A, ROM_SysCtlClockGet() / 2);
    //

    //MAP_TimerConfigure(TIMER0_BASE, TIMER_CFG_32_BIT_PER); // periodic mode
    //TimerPrescaleSet(TIMER0_BASE, TIMER_A, 100);
    //MAP_TimerEnable(TIMER0_BASE, TIMER_BOTH);
    //cur_time = MAP_TimerValueGet(TIMER0_BASE,TIMER_A);
    //MAP_SysCtlDelay(2);
    //period_tick = MAP_TimerValueGet(TIMER0_BASE,TIMER_A);

    /*
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
    //SysCtlPeripheralReset(SYSCTL_PERIPH_TIMER2);
    TimerConfigure(TIMER2_BASE, TIMER_CFG_32_BIT_PER );
    //TimerControlStall(TIMER0_BASE, TIMER_A, true);
    TimerLoadSet(TIMER2_BASE, TIMER_A, SysCtlClockGet()/1000);
    TimerEnable(TIMER2_BASE, TIMER_BOTH);
    */
    ///////////////////
    // настраиваем входные зеленые пины на регистрацию шима
    IntMasterEnable();

    ////////////////

    //MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    MAP_SysCtlDelay(2);
    //MAP_GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_3);    // UBAT, AIN12
    //MAP_GPIOPinTypeADC(GPIO_PORTB_BASE, GPIO_PIN_5);    // UG6, AIN11
    //MAP_GPIOPinTypeADC(GPIO_PORTB_BASE, GPIO_PIN_4);    // UG5, AIN10

    MAP_GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_4);    // IC8, AIN7
    MAP_GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_5);    // IC7, AIN6
    MAP_GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_6);    // IC6, AIN5
    MAP_GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_7);    // IC5, AIN4


    MAP_GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_4);    // IC4, AIN3
    MAP_GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_5);    // IC3, AIN2
    MAP_GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_6);    // IC2, AIN1
    MAP_GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_7);    // IC1, AIN0

    MAP_SysCtlADCSpeedSet(SYSCTL_ADCSPEED_125KSPS);     // 125KSPS, for lm3s9b90 max - 1000 KSPS

    MAP_ADCSequenceDisable(ADC0_BASE, 0);
    MAP_ADCSequenceDisable(ADC0_BASE, 1);

    // The external VREFA input is the voltage reference
    //  MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    //  MAP_GPIOPinTypeADC(GPIO_PORTB_BASE, GPIO_PIN_6);    // VREFA
    //  MAP_ADCReferenceSet(ADC0_BASE, ADC_REF_EXT_3V);

    // Use internal voltage reference (3.0V)
    MAP_ADCReferenceSet(ADC0_BASE, ADC_REF_INT);

    // Sequence 0 - FIFO size = 8
    MAP_ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);
    MAP_ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_CH0);                             // AIN4, UG1
    MAP_ADCSequenceStepConfigure(ADC0_BASE, 0, 1, ADC_CTL_CH1);                             // AIN5, UG2
    MAP_ADCSequenceStepConfigure(ADC0_BASE, 0, 2, ADC_CTL_CH2);                             // AIN6, UG3
    MAP_ADCSequenceStepConfigure(ADC0_BASE, 0, 3, ADC_CTL_CH3);                             // AIN7, UG4
    MAP_ADCSequenceStepConfigure(ADC0_BASE, 0, 4, ADC_CTL_CH4);                            // AIN10, UG5
    MAP_ADCSequenceStepConfigure(ADC0_BASE, 0, 5, ADC_CTL_CH5);                            // AIN11, UG6
    MAP_ADCSequenceStepConfigure(ADC0_BASE, 0, 6, ADC_CTL_CH6);                            // AIN12, UBAT
    MAP_ADCSequenceStepConfigure(ADC0_BASE, 0, 7, ADC_CTL_CH7 | ADC_CTL_IE | ADC_CTL_END);                // temperature sensor
    ////////

    MAP_ADCHardwareOversampleConfigure(ADC0_BASE, 64);

    MAP_ADCIntClear(ADC0_BASE, 0);
    MAP_ADCIntClear(ADC0_BASE, 1);
    //
    MAP_ADCIntEnable(ADC0_BASE, 0);
//    MAP_ADCIntEnable(ADC0_BASE, 0);

    //MAP_ADCIntEnable(ADC0_BASE, 1);
    MAP_IntEnable(INT_ADC0SS0);
    //MAP_IntEnable(INT_ADC0SS1);
    MAP_ADCSequenceEnable(ADC0_BASE, 0);
    //MAP_ADCSequenceEnable(ADC0_BASE, 1);
    MAP_ADCProcessorTrigger(ADC0_BASE, 0);


    if (tn_task_create(&task_adc_tcb, &task_adc_func, TASK_ADC_PRI,
        &task_adc_stk[TASK_ADC_STK_SZ - 1], TASK_ADC_STK_SZ, 0,
        TN_TASK_START_ON_CREATION) != TERR_NO_ERR)
    {
        dbg_puts("tn_task_create(&task_adc_tcb) error");
        dbg_trace();
        tn_halt();
    }

    dbg_puts("[done]");
}
//------------------------------------------------------------------------------
void Get_real_adc(ADC_DATA *data)
{
    //memcpy(data, (const char *)&adc_data,sizeof(adc_data));
    //
    Calc_Middle();
  //
    data->UG1 = adc_middle[0]*3;
    data->UG2 = adc_middle[1]*3;
    data->UG3 = adc_middle[2]*3;
    data->UG4 = adc_middle[3]*3;
    data->UG5 = adc_middle[4]*3;
    data->UG6 = adc_middle[5]*3;
    data->UG7 = adc_middle[6]*3;
    data->UG8 = adc_middle[7]*3;
}
//------------------------------------------------------------------------------
void Get_real_pwm(ADC_DATA *data)
{
   Check_Channels();
   //Calc_Middle_U();
    //
    if (U_STAT[0]) data->UG1 = udc_middle[0];
       else data->UG1 = 0;
    if (U_STAT[1]) data->UG2 = udc_middle[1];
       else data->UG2 = 0;
    if (U_STAT[2]) data->UG3 = udc_middle[2];
      else data->UG3 = 0;
    if (U_STAT[3]) data->UG4 = udc_middle[3];
      else data->UG4 = 0;
    if (U_STAT[4]) data->UG5 = udc_middle[4];
      else data->UG5 = 0;
    if (U_STAT[5]) data->UG6 = udc_middle[5];
      else data->UG6 = 0;
    if (U_STAT[6]) data->UG7 = udc_middle[6];
      else data->UG7 = 0;
    if (U_STAT[7]) data->UG8 = udc_middle[7];
      else data->UG8 = 0;
    ///
}

//------------------------------------------------------------------------------
void adc_seq0_int_handler()
{
    //static int i;
    MAP_ADCIntClear(ADC0_BASE, 0);
    ///////
    if (adc_div>=ADC_DIV_N)
    {
       MAP_ADCSequenceDataGet(ADC0_BASE, 0, &(adc_data[adc_pos].UG1));
       adc_pos++;
       if (adc_pos >= ADC_BUF_N)
         adc_pos=0;
       //
       adc_div=0;
    }
    else
      adc_div++;
    /////////////
    MAP_ADCProcessorTrigger(ADC0_BASE, 0);


}
//------------------------------------------------------------------------------
// наличие сетевого напряжения
void IntGPIO_PW_CONTR(void)
{
 // выясняем на каком пине произошло прерывание
  unsigned char status = GPIOPinIntStatus(GPIO_PORTH_BASE, true);
  cur_time = hw_time();

  static unsigned long diff, cur_t;
  cur_t =  MAP_TimerValueGet(TIMER0_BASE,TIMER_A);

   if(status & GPIO_PIN_7)
       {
          if (!pin_rd(IPIN_PW_CONTR))
          {
              //u_last_change[ic] = cur_time;
              u_last_change_PW = cur_t;
          }
          else
          {
              udc_pos_PW++;
              if (udc_pos_PW >= UDC_BUF_N)
                udc_pos_PW=0;

              if (u_last_change_PW > cur_t)
                diff = u_last_change_PW- cur_t;
              else
                diff = 0;

              udc_data_PW[udc_pos_PW] = diff;
          }
          u_time_dif_PW = cur_time;
          ///
          GPIOPinIntClear(GPIO_PORTH_BASE, GPIO_PIN_7);
       }
}
//------------------------------------------------------------------------------
//прерывания от ШИМА напряжения на зеленых
void IntGPIO_U(void)
{
  // выясняем на каком пине произошло прерывание
  unsigned char status = GPIOPinIntStatus(GPIO_PORTJ_BASE, true);
  cur_time = hw_time();

  unsigned long pin;
  int i_pos;
  static unsigned long diff, cur_t;
  cur_t =  MAP_TimerValueGet(TIMER0_BASE,TIMER_A);
  //cur_t = MAP_SysTickValueGet();
  ///
  for (int ic=0; ic<8; ic++)
  {
       pin = 1<< ic;

       //
       if (status & pin)
       {
          if (!GPIOPinRead( GPIO_PORTJ_BASE, pin))
          {
              //u_last_change[ic] = cur_time;
              u_last_change[ic] = cur_t;
              /////////
              if (ic==0) pin_on(OPIN_ERR_LED);
              /////
          }
          else
          {
              if (u_last_change[ic] > cur_t)
                diff = u_last_change[ic]- cur_t;
              else
                diff = 0;//u_last_change[ic] + 0xFFFFFFFF-cur_t;
              ///////////
              /////////
              if (ic==0) pin_off(OPIN_ERR_LED);
              /////
              /*
              //Filter
              if (diff<100000)
              {
                GPIOPinIntClear(GPIO_PORTJ_BASE, pin);
                continue;

              } */
              //
              i_pos = udc_pos[ic]++;
              if (i_pos >= UDC_BUF_N)
                i_pos=udc_pos[ic]=0;

              ///
              switch (ic)
              {
                  case 0:udc_data[i_pos].UG1 = diff; break;
                  case 1:udc_data[i_pos].UG2 = diff; break;
                  case 2:udc_data[i_pos].UG3 = diff; break;
                  case 3:udc_data[i_pos].UG4 = diff; break;
                  case 4:udc_data[i_pos].UG5 = diff; break;
                  case 5:udc_data[i_pos].UG6 = diff; break;
                  case 6:udc_data[i_pos].UG7 = diff; break;
                  case 7:udc_data[i_pos].UG8 = diff; break;
              }
              ///


              //udc_data[i_pos].
          }
        u_time_dif[ic] = cur_time;
        ///
        GPIOPinIntClear(GPIO_PORTJ_BASE, pin);
       }
       //////////////////

       // проверяем уровень на входе, если высокий. то
     //
     /////////

  }
  /////////////////////
  /*
  //чистим
  for (int ic=0; ic<8; ic++)
  {
     if ((cur_time - u_time_dif[ic]) > 30)
     {
       i_pos = udc_pos[ic]++;
       if (i_pos >= UDC_BUF_N)
           i_pos=udc_pos[ic]=0;
       //   udc_middle[ic]=0;
              switch (ic)
              {   case 0:udc_data[i_pos].UG1 = 0; break;
                  case 1:udc_data[i_pos].UG2 = 0; break;
                  case 2:udc_data[i_pos].UG3 = 0; break;
                  case 3:udc_data[i_pos].UG4 = 0; break;
                  case 4:udc_data[i_pos].UG5 = 0; break;
                  case 5:udc_data[i_pos].UG6 = 0; break;
                  case 6:udc_data[i_pos].UG7 = 0; break;
                  case 7:udc_data[i_pos].UG8 = 0; break;
              }
     }
  }
  */
  ////////////////////


}
//------------------------------------------------------------------------------

void adc0_seq1_int_handler()
{
  //static int i;
    //MAP_ADCIntClear(ADC0_BASE, 1);
    //i=MAP_ADCSequenceDataGet(ADC0_BASE, 1, &adc_data.IR1);
    //
    //MAP_ADCIntClear(ADC0_BASE, 0);
    //i=MAP_ADCSequenceDataGet(ADC0_BASE, 0, &adc_data.UG1);
    //
    //MAP_ADCProcessorTrigger(ADC0_BASE, 0);
}
//------------------------------------------------------------------------------
/*
void ALARMER()
{
    DK[CUR_DK].REQ.req[ALARM].spec_prog = SPEC_PROG_OC;
    DK[CUR_DK].REQ.req[ALARM].work = SPEC_PROG;
    DK[CUR_DK].REQ.req[ALARM].source = ALARM;
    DK[CUR_DK].REQ.req[ALARM].presence = true;

}
*/

//------------------------------------------------------------------------------
// Вычисление
void Calc_Middle()
{
  //unsigned long mid_res=0;
    ///////
    for (int i=0; i< I_CH_N; i++) adc_middle[i]=0;
    ///
    // подсчет среднего по АЦП
    for (int i=0; i< ADC_BUF_N; i++)
    {
      adc_middle[0]+= adc_data[i].UG1;
      adc_middle[1]+= adc_data[i].UG2;
      adc_middle[2]+= adc_data[i].UG3;
      adc_middle[3]+= adc_data[i].UG4;
      adc_middle[4]+= adc_data[i].UG5;
      adc_middle[5]+= adc_data[i].UG6;
      adc_middle[6]+= adc_data[i].UG7;
      adc_middle[7]+= adc_data[i].UG8;
    }
    ////
      adc_middle[0]/= ADC_BUF_N;
      adc_middle[1]/= ADC_BUF_N;
      adc_middle[2]/= ADC_BUF_N;
      adc_middle[3]/= ADC_BUF_N;
      adc_middle[4]/= ADC_BUF_N;
      adc_middle[5]/= ADC_BUF_N;
      adc_middle[6]/= ADC_BUF_N;
      adc_middle[7]/= ADC_BUF_N;
      ////


}
//------------------------------------------------------------------------------
// Вычисление
void Calc_Middle_U()
{
  //unsigned long mid_res=0;
    ///////
    for (int i=0; i< U_CH_N; i++) udc_middle[i]=0;
    udc_middle_PW=0;
    ///
    // подсчет среднего по АЦП
    for (int i=0; i< UDC_BUF_N; i++)
    {
      udc_middle[0]+= udc_data[i].UG1;
      udc_middle[1]+= udc_data[i].UG2;
      udc_middle[2]+= udc_data[i].UG3;
      udc_middle[3]+= udc_data[i].UG4;
      udc_middle[4]+= udc_data[i].UG5;
      udc_middle[5]+= udc_data[i].UG6;
      udc_middle[6]+= udc_data[i].UG7;
      udc_middle[7]+= udc_data[i].UG8;
      //
      udc_middle_PW += udc_data_PW[i];
    }
    ////
      udc_middle[0]/= UDC_BUF_N;
      udc_middle[1]/= UDC_BUF_N;
      udc_middle[2]/= UDC_BUF_N;
      udc_middle[3]/= UDC_BUF_N;
      udc_middle[4]/= UDC_BUF_N;
      udc_middle[5]/= UDC_BUF_N;
      udc_middle[6]/= UDC_BUF_N;
      udc_middle[7]/= UDC_BUF_N;
      //
      udc_middle_PW/=UDC_BUF_N;
      ////
      cur_time  = hw_time();
      // Post correct
      for (int ic=0; ic< U_CH_N; ic++)
      {
        if ((cur_time - u_time_dif[ic]) > ADC_RED_POROG)
          udc_middle[ic]=0;
        ////
        //if (udc_middle[ic]>10000)
        //  udc_middle[ic]=0;
      }
      ///
      if ((cur_time - u_time_dif_PW) > ADC_RED_POROG)
          udc_middle_PW = 0 ;
}
//------------------------------------------------------------------------------
// 0-OK
int Check_Channels()
{
  cur_time  = hw_time();
  //
  Calc_Middle();
  // ток, красные
  for (int ic=0; ic< I_CH_N; ic++)
     {
        if (adc_middle[ic] < 800)
          I_STAT[ic]=true;
        else
          I_STAT[ic]=false;
     }
  // проверяем наgряжение
  for (int ic=0; ic< U_CH_N; ic++)
     {
       if (0.95*sens_zero_count[ic]>sens_plus_count[ic])
            U_STAT[ic]=true;
       else
            U_STAT[ic]=false;
     }
  if (0.95*sens_zero_count[SENS_N-1]>sens_plus_count[SENS_N-1])
          U_STAT_PW=true;
        else
          U_STAT_PW=false;
return (0);
}
//------------------------------------------------------------------------------
void Check_Power()
{
sens_count++;

for (int ic=0; ic<SENS_N; ic++)
    {
        if (pin_rd(ipins[ic]))
        {
           sens_plus_count[ic]++;
        }
        else
        {
          sens_zero_count[ic]++;
        }
    }
}
//------------------------------------------------------------------------------
void Clear_UDC_Arrays()
{
    ADC_WORK_FLAG=false;
    //
    sens_count=0;
    for (int ic=0; ic<SENS_N; ic++)
    {
          sens_plus_count[ic]=0;
          sens_zero_count[ic]=0;
    }
    ADC_WORK_FLAG=true;

}
/*----------------------------------------------------------------------------*/
//loop cycle
static void task_adc_func(void* param)
{
Clear_UDC_Arrays();
ADC_WORK_FLAG=TRUE;
// loop
for (;;)
  {
  if (ADC_WORK_FLAG)
      Check_Power();
  }
}

