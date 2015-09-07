/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../tnkernel/tn.h"
#include "../stellaris.h"
#include "../debug/debug.h"
#include "../version.h"
#include "light.h"
#include "../pref.h"
#include "../pins.h"
#include "../memory/memory.h"
#include "../crc32.h"
#include "../dk/dk.h"
#include "../memory/ds1390.h"
#include "../adcc.h"
#include "../dk/kurs.h"
#include "../multicast/cmd_ch.h"
#include "../multicast/cmd_fn.h"
#include "../event/evt_fifo.h"
#include "../gps/gps.h"

#define LIGHT_REFRESH_INTERVAL    500

static TN_TCB task_light_tcb;
#pragma location = ".noinit"
#pragma data_alignment=8

_LIGHT_MACHINE light_machine;
unsigned char * text_light[8] = {"ALL_OFF" ,"GREEN", "GREEN_YELLOW","RED","RED_YELLOW","YELLOW","YELLOW_FLASH","RESERVED"};

static unsigned int task_light_stk[TASK_LIGHT_STK_SZ];
static void task_light_func(void* param);

static TN_EVENT         g_light_evt;

static unsigned char LIGHT_STA;
static int   reboot_tryes = 0; //кол-во попыток перезапуска
static SYSTEMTIME L_ERR_TIME;  //время последней ошибки
//
enum {LIGHT_TEST, LIGHT_WORK, LIGHT_FAULT, LIGHT_FAULT_STOP};
// счетчик прерываний часов
unsigned char DS_INTS_COUNT;
// счетчик процесса с tn_wait
unsigned char WAIT_1S_COUNT;
// типы обнаруженных ошибок
FAULT_TYPE  chan_faults;
BOOL  Next_Try_wait=false;
BOOL  power_flag=false;
// откладываем запуск на кол-во тиков
int light_time_out=1;
// значение контрольной суммы проета
struct TCRC_IDP{
  unsigned long crc;
} crc_idp;
//------------------------------------------------------------------------------
void POWER_SET(const BOOL stat)
{
    if (stat)
    {
      if (!power_flag)
      {
         SIGNAL_OFF();
         pin_on(OPIN_POWER);
         SIGNAL_OFF();
         SysCtlDelay( (SysCtlClockGet()/(3*1000))) ;
         SIGNAL_OFF();
         light_time_out=2;
      }

    }
    else
    {
      if (power_flag)
      {
        pin_off(OPIN_POWER);
        light_time_out=2;
      }
    }
power_flag=stat;
}
//------------------------------------------------------------------------------
// fin_try==0 - последняя поппытка, можно и записать в журнал ерроры
BOOL ligh_load_init(int  fin_try)
{
    //
    memset(&DK[CUR_DK],0,sizeof(DK[CUR_DK]));
    // clear crc
    memset(&crc_idp,0,sizeof(crc_idp));
    // FLASH_PROGS
    flash_rd(FLASH_PROGRAM, sizeof(TPROJECT)*CUR_DK,
             (unsigned char*)&PROJ[CUR_DK], sizeof(PROJ[CUR_DK]));
    unsigned long crc=crc32_calc((unsigned char*)&PROJ[CUR_DK] ,
                                 sizeof(PROJ[CUR_DK]) - sizeof(PROJ[CUR_DK].CRC32));
    //
    unsigned long crc_progs;
    flash_rd(FLASH_PROGS, sizeof(TPROGRAMS)*(CUR_DK+1)-sizeof(crc_progs) ,
             (unsigned char*)&crc_progs, sizeof(crc_progs));
    //
    crc_idp.crc =  crc_progs;// сохраним для передачи по UDP
    //i_size = sizeof(PROJ[CUR_DK]);
    if (PROJ[CUR_DK].ProjectSize ==sizeof(PROJ[CUR_DK]))
    {
      if ( crc == PROJ[CUR_DK].CRC32)
      {
        //
        if (crc_progs==PROJ[CUR_DK].CRC32_programs)
        {
          Init_DK();
          DK[CUR_DK].proj_valid=true;
          DK[CUR_DK].work = true;
          DK[CUR_DK].progs_valid = true;
          return true;
        }
        else
        {

            dbg_printf("FAULT. CRC32 of progs invalid");
            if (CUR_DK==0)
            if (!fin_try)
              Event_Push_Str("Некорр. программы (CRC)");
        }
      }
      else
      {
        dbg_printf("FAULT. PROJ.CRC32");
        if (CUR_DK==0)
        if (!fin_try)
          Event_Push_Str("Некорр. проект (CRC).");
      }
    }
    else
    {
      if (CUR_DK==0)
       if (!fin_try)
         Event_Push_Str("Некорр. проект (размер).");
       //
      dbg_printf("FAULT. PROJ.ProjectSize ==sizeof(PROJ)");
    }
return false;
}
//------------------------------------------------------------------------------
// init one call
void light_init(void)
{
    dbg_printf("Initializing LIGHT...");
    pin_on(OPIN_TR_EN );//включить выходные регистры
    LIGHT_STA=LIGHT_WORK;// machine stat
    light_machine.work = FALSE; // stop machine
    //
    BOOL is_init;
    dk_num=0;
    //
    GREEN_PORT_CONF = 0;
    RED_PORT_CONF = 0;
    YEL_PORT_CONF = 0;
    //
   for (int i_dk=0; i_dk<DK_N; i_dk++)
    {
       CUR_DK = i_dk;

       for (int i_try=0; i_try<3; i_try++)
       {
         is_init = ligh_load_init(2-i_try);
          if (is_init)
          {
             dk_num++;
             break;
          }
       }
       //
     if (!is_init)
         break;
    }
if (dk_num)
  light_machine.work = TRUE; // start machine
/*create event*/
if (tn_event_create(&g_light_evt, TN_EVENT_ATTR_SINGLE | TN_EVENT_ATTR_CLR, 0) != TERR_NO_ERR){
  dbg_puts("tn_event_create(&g_light_evt) error");
  dbg_trace();
  tn_halt();
  }
/*create task_light_func*/
if (tn_task_create(&task_light_tcb, &task_light_func, TASK_LIGHT_PRI,
        &task_light_stk[TASK_LIGHT_STK_SZ - 1], TASK_LIGHT_STK_SZ, 0,
        TN_TASK_START_ON_CREATION) != TERR_NO_ERR){
  dbg_puts("tn_task_create(&task_light_tcb) error");
  dbg_trace();
  tn_halt();
  }
dbg_puts("[done]");
}
//------------------------------------------------------------------------------
BOOL GetLightMachineWork(void)
{
return light_machine.work;
}
//------------------------------------------------------------------------------
// true - start machine
// false - stop machine
void SetLightMachineWork(BOOL work)
{
light_machine.work = work;
}
//------------------------------------------------------------------------------
void SetPWM (unsigned long pwm_rgy)
{
}
//------------------------------------------------------------------------------
void GetLightText (char *buf)
{
unsigned short light = light_machine.light;
snprintf(buf, 128, "D1: %s, %s, %s \nD2: %s, %s, %s\n",
             text_light[light&DIRECT1], text_light[(light&WALKER1)>>4], text_light[(light&ARROW1)>>6],
             text_light[(light&DIRECT2)>>8], text_light[(light&WALKER2)>>12], text_light[(light&ARROW2)>>14]);
}
//------------------------------------------------------------------------------
// эта функция используеться в прерывании ds1390.c
void Light_set_event(void)
{
tn_event_iset(&g_light_evt,EVENT_SINHRO_TIME);
DS_INTS_COUNT++;
}
//------------------------------------------------------------------------------
void SIGNAL_OFF()
{
GREEN_PORT=0;
RED_PORT = 0;
YEL_PORT = 0;
SET_OUTPUTS();
}
//------------------------------------------------------------------------------
// Save Last Error Time
static void Save_LET(void)
{
DS1390_TIME time;
//
GetTime_DS1390(&time);
//
L_ERR_TIME.tm_hour = time.hour;
L_ERR_TIME.tm_mday = time.date;
L_ERR_TIME.tm_min  = time.min;
L_ERR_TIME.tm_sec  = time.sec;
L_ERR_TIME.tm_mon  = time.month;
L_ERR_TIME.tm_year = time.year;
}
//------------------------------------------------------------------------------
// проверка конфликтов каналов
// return false - valid true - invalid
static bool Check_Chan(void)
{
    char buf[60];
    static bool ret,sta, sta1, sta2, sta_tra;
    ret=false;
    GREEN_PORT_ERR=0;
    RED_PORT_ERR=0;
    BOOL power_valid=true;
    chan_faults=FT_NO;
    //int ic_new;
    //
    Check_Channels(); // call adc.c
    //
    //Validate power
    if((sens_zero_count[SENS_N-1]< 100) && (sens_plus_count[SENS_N-1]<100))
           power_valid=false;
    //sens_count
    if ((sens_zero_count[SENS_N-1] + sens_plus_count[SENS_N-1]) > (sens_count+5))
           power_valid=false;

    //
    if (power_valid)
    if (!DK[CUR_DK].test)
    if (U_STAT_PW!=U_STAT_PW_last)
    {
      if (U_STAT_PW)
      {
        if (PROJ[CUR_DK].jornal.power_on)
          Event_Push_Str("Включение напряжения");
        ///
        U_STAT_PW_last=true;
      }
      else
      {
        if (PROJ[CUR_DK].jornal.power_off)
          Event_Push_Str("Oтключение напряжения");
        ///
        U_STAT_PW_last=false;
      }
    }
    //
    if (!power_flag)
       return (false);
    ///
    if (light_time_out)
    {
      light_time_out--;
      return (false);
    }
    if (U_STAT_PW==0)
      return (false);
    //
    if (DK[CUR_DK].test)
      return (false);
    // U sensors work
    for (int ic=0; ic<SENS_N; ic++)
    {
        if ((sens_zero_count[ic]> 100) && (sens_plus_count[ic]==0) )
        {
           if (ic<SENS_N-1)
           {
               sta2 = (GREEN_PORT_CONF & (1<<ic))>0 ? true: false;
               //
               if (PROJ[CUR_DK].jornal.alarm)
               if (!DK[CUR_DK].U_sens_fault_log[ic])
               {
                  DK[CUR_DK].U_sens_fault_log[ic]=true;
                  snprintf(buf, 60 , "Датчик зел. %u неисправен",(ic+1));
                  Event_Push_Str(buf);
                  chan_faults |= FT_GREEN_SENS;
               }
               //
               if (sta2)
                 ret=true;
           }
           else
           {
              if (PROJ[CUR_DK].jornal.alarm)
               if (!DK[CUR_DK].U_sens_power_fault_log)
               {
                  DK[CUR_DK].U_sens_power_fault_log=true;
                  Event_Push_Str("Дачик напряжения не работает!");
               }
           }
        }
    }
    //
    for (int ic=0; ic<8; ic++)
    {
      sta =  (U_STAT[ic]>0) ? true : false;
      sta1 = (GREEN_PORT & (1<<ic))>0 ? true : false;
      sta2 = (GREEN_PORT_CONF & (1<<ic))>0 ? true: false;
      // состояния различаются
      if (sta2)
       //if (!sta_tra)
       if (sta ^ sta1)
       {

         //Validate
         if ((sens_zero_count[ic]< 100) && (sens_plus_count[ic]<100))
           continue;
         //sens_count
         if ((sens_zero_count[ic] + sens_plus_count[ic]) > (sens_count+5))
           continue;
         // выход - НЕТ, сигнал ЕСТЬ
         if (sta)
         {   //недостаточный уровень
           // GREEN_PORT=0, U_STAT=1
           if (sens_zero_count[ic]< 3*sens_plus_count[ic])
             //if (udc_middle[ic]<500000)
               continue;
         }
         else
         {   // пропадание зеленого,
             // GREEN_PORT=1, U_STAT=0
             //if (udc_middle[ic]>100000)
           if (sens_zero_count[ic]> sens_plus_count[ic]/2)
               continue;

         }
         //различаются  // AHTUNG
         GREEN_PORT_ERR= GREEN_PORT_ERR | (1<<ic);

         if (PROJ[CUR_DK].jornal.alarm)
         {
           if (sta_tra)
             sta1=false;
           ///
           if (sta1)
           {
            snprintf(buf, 60 , "Зел. выход %u неисправен",(ic+1));
            chan_faults |= FT_GREEN_CHAN ;
           }
           else
           {
             snprintf(buf, 60 , "Конфликт зел., выход %u",(ic+1));
             chan_faults |= FT_GREEN_CONFL ;
           }

           Event_Push_Str(buf);
         }
       ret=true;
       }
    }
for (int ic=0; ic<8; ic++)
  {
  sta  = (I_STAT[ic]>0)? true : false;
  sta1 = (RED_PORT & (1<<ic))>0 ? true : false;
  sta2 = (RED_PORT_CONF & (1<<ic))>0 ? true : false;

  if (sta2)
    if (sta ^ sta1){
      //различаются AHTUNG
      RED_PORT_ERR= RED_PORT_ERR | (1<<ic);
      //
      if(PROJ[CUR_DK].jornal.alarm){
        if (sta1){
          snprintf(buf, 60 , "КР выход %u без нагрузки",(ic+1));
          chan_faults |= FT_RED_CHAN;//FT_RED_CONFL;
          }
        Event_Push_Str(buf);
        }
      ret=true;
      }
  }
return (ret);
}
//------------------------------------------------------------------------------
static void DK_ALARM_OC(void)
{
        DK[CUR_DK].REQ.req[ALARM].spec_prog = SPEC_PROG_OC;
        DK[CUR_DK].REQ.req[ALARM].work = SPEC_PROG;
        DK[CUR_DK].REQ.req[ALARM].source = ALARM;
        DK[CUR_DK].REQ.req[ALARM].presence = true;
}
//------------------------------------------------------------------------------
/*static void DK_ALARM_YF(void)
{
        DK[CUR_DK].REQ.req[ALARM].spec_prog = SPEC_PROG_YF;
        DK[CUR_DK].REQ.req[ALARM].work = SPEC_PROG;
        DK[CUR_DK].REQ.req[ALARM].source = ALARM;
        DK[CUR_DK].REQ.req[ALARM].presence = true;
}*/
/*----------------------------------------------------------------------------*/
// остановка ДК авария
void DK_HALT(void)
{
LIGHT_STA = LIGHT_FAULT;
POWER_SET(false);
SIGNAL_OFF();
DK_ALARM_OC();
DK_MAIN();
}
// запуск ДК после аварии
void DK_RESTART(void)
{
LIGHT_STA = LIGHT_WORK;
Init_DK();
DK_MAIN();
//tn_reset(); // сбросс CPU
}
//------------------------------------------------------------------------------
// следующая попытка
static void Next_Try(bool b)
{
    if (!DK[CUR_DK].no_FAULT)
    if (b)
    {
      //все отключаем
     if (reboot_tryes<PROJ[CUR_DK].guard.restarts)
     {
        reboot_tryes++;
        if (chan_faults!=FT_RED_CONFL){
            POWER_SET(false); // off rele
            SIGNAL_OFF();
            Save_LET();
            //
            for (int i_dk=0; i_dk<dk_num; i_dk++)
              {
              CUR_DK = i_dk;
              Init_DK();
              DK_ALARM_OC();
              }
           //
           DK_MAIN();
           light_machine.work=false;
           //
          }else{
           Save_LET();
           for (int i_dk=0; i_dk<dk_num; i_dk++)
              {
              CUR_DK = i_dk;
              Init_DK();
              DK_ALARM_OC();//DK_ALARM_YF();
              }
           DK_MAIN();
          }
        Next_Try_wait = true;
        chan_faults=FT_NO;
        tn_task_sleep(100);
     }
     else
     {
          DK_HALT();
     }
    }
}
//------------------------------------------------------------------------------
// Save Last Error Time
static void Check_LET(void)
{
        SYSTEMTIME  cur_time;
        DS1390_TIME time;
        //
        if (reboot_tryes==0)
          return;
        ///////
        GetTime_DS1390(&time);
        //
        cur_time.tm_hour = time.hour;
        cur_time.tm_mday = time.date;
        cur_time.tm_min = time.min;
        cur_time.tm_sec = time.sec;
        cur_time.tm_mon = time.month;
        cur_time.tm_year = time.year;
        ///////////////
        int i_sec = Seconds_Between(&L_ERR_TIME, &cur_time);
          if (i_sec > PROJ[CUR_DK].guard.time_clear*60)
            reboot_tryes=0;

}
/*----------------------------------------------------------------------------*/
/*loop cycle*/
/*----------------------------------------------------------------------------*/
static void task_light_func(void* param)
{
    bool  b_ch;
    unsigned int fpattern;
    SYSTEMTIME  cur_time;
    DS1390_TIME time;

    CUR_DK=0;
    SIGNAL_OFF();
    POWER_SET(true);
    SIGNAL_OFF();
    pin_off(OPIN_ERR_LED); // error enabled
    //
    tn_task_sleep(500);
    SIGNAL_OFF();
    pin_off(OPIN_TR_EN);//включить выходные регистры
    //
    Event_Push_Str("СТАРТ");// запись в журнал старт
    light_time_out=1;

for (;;)
    {
    //считать время
    GetTime_DS1390(&time);
    // обновить
    cur_time.tm_hour = time.hour;
    cur_time.tm_mday = time.date;
    cur_time.tm_min = time.min;
    cur_time.tm_sec = time.sec;
    cur_time.tm_mon = time.month;
    cur_time.tm_year = time.year;
    //
    if (Next_Try_wait)
       if (Seconds_Between(&L_ERR_TIME, &cur_time) >
            PROJ[CUR_DK].guard.restart_interval){
       for (int i_dk=0; i_dk<dk_num; i_dk++)
          {
           DK[i_dk].REQ.req[ALARM].presence = false;
          }
          light_machine.work=true;
          //
          if (chan_faults!=FT_RED_CONFL){
             SIGNAL_OFF();
             POWER_SET(true);
             SIGNAL_OFF();
             }
          //
          Next_Try_wait = false;
          }
    // test mode
    if(DK[0].test)
      {
      if(!DK[0].test_init){
        SIGNAL_OFF();
        DK[0].test_init=true;
        }
      //
      Clear_UDC_Arrays();
      tn_task_sleep(500);
      //
      Check_Channels();
      continue;
      }
    // last time error
    Check_LET();
    // work mode
    if (!light_machine.work){
      tn_task_sleep(500);
      continue;
      }
    //machine stat
    switch (LIGHT_STA)
       {
       case LIGHT_WORK:
         {
          tn_event_wait(&g_light_evt, EVENT_SINHRO_TIME, TN_EVENT_WCOND_OR, &fpattern, TN_WAIT_INFINITE);
          //синхронизация времени GPS&RTC
          Synk_TIME();
          //проверка конфликтов каналов
          b_ch = Check_Chan();
          if(b_ch){
            Next_Try(b_ch);break;}
          // основная логика работы
          DK_MAIN();
          tn_task_sleep(500);// delay 0.5S
          //проверка конфликтов каналов
          b_ch = Check_Chan();
          if(b_ch){
            Next_Try(b_ch);break;}
          // здесь организовано миганине силовых выходов вот так!(в проетке его нет)
          /*for (int i_dk=0; i_dk<dk_num; i_dk++)
                {
                CUR_DK = i_dk; // тукущий ДК
                Update_STATES(true);
                } */
         SET_OUTPUTS();
         break;
         }
       case LIGHT_FAULT:
          {
           tn_task_sleep(500);
           break;
          }
       }// end switch
    }// end for
}
//----------------------------------------------------------------------------//
// возращаем значение контрольной суммы проекта
//----------------------------------------------------------------------------//
unsigned long retCRC32()
{
return crc_idp.crc;
}
/*----------------------------------------------------------------------------*/