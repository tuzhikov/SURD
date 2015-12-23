//---------------------------------------------------------------------------
//  Модуль ДК
// архитектура - model-controller
#include <stdlib.h>
#include <stdio.h>
#include "dk.h"
#include "structures.h"
#include "takts.h"
#include "string.h"
#include "../utime.h"
#include "../memory/ds1390.h"
#include "../multicast/cmd_fn.h"
#include "../memory/memory.h"
#include "../event/evt_fifo.h"
#include "../light/light.h"
#include "../adcc.h"
#include "../pins.h"
#include "../vpu.h"

#ifdef KURS_DK
  #include "kurs.h"
#endif



__DK DK[DK_N];
TPROJECT     PROJ[DK_N];
TPROGRAM     PROG_PLAN;
BYTE   dk_num; //кол-во обнаруженных ДК
BYTE   CUR_DK; // текущий ДК
SYSTEMTIME   CT; // control time
/*local functions*/
static int  Get_Next_Viz_Faz(int prog, int cur_faz);
static int  WeekOfTheYear(SYSTEMTIME *st);
static int  DayOfWeek(SYSTEMTIME *st);
static bool Check_TVP_En(void);
int getCurrentTimeBeginPhase(void);
static int  getNextVisibledPhaseOfPlan(const int prog);
/*----------------------------------------------------------------------------*/
/*
functions discrintion
*/
/*----------------------------------------------------------------------------*/
//#define D_W_ENABLE
// тестовый вывод сообщений по UDP
void D_W(const char*s)
{
   #ifdef D_W_ENABLE
     debugee(s);
   #endif
}
//------------------------------------------------------------------------------
#ifndef KURS_DK  // не КУРС
// обновить OLD
unsigned short Update_STATES_VO()
{
        int i_napr;
        unsigned short fire=0;
        //for (int i_dk=0; i_dk<n_n; i_dk++)
        //{
        int i_dk=0;
           // первый транспортный
           i_napr = DK[CUR_DK].PROJ->Muls.OneMUL[i_dk].direct1;
           if (i_napr)
               fire |=DK[CUR_DK].control.napr[i_napr-1];
           // второй транспортный
           i_napr = PROJ.Muls.OneMUL[i_dk].direct2;
           if (i_napr)
              fire |= DK[CUR_DK].control.napr[i_napr-1]<<8;
           // первый пешеходный
           i_napr = PROJ.Muls.OneMUL[i_dk].walker1;
           if (i_napr && DK[CUR_DK].control.napr[i_napr-1] <= RED )
               fire |= DK[CUR_DK].control.napr[i_napr-1]<<4;
           // второй пешеходный
           i_napr = PROJ.Muls.OneMUL[i_dk].walker2;
           if (i_napr && DK[CUR_DK].control.napr[i_napr-1] <= RED )
               fire |= DK[CUR_DK].control.napr[i_napr-1]<<12;
           // первый стрелка
           i_napr = PROJ.Muls.OneMUL[i_dk].arrow1;
           if (i_napr && DK[CUR_DK].control.napr[i_napr-1] <= GREEN_YELLOW)
              fire |= DK[CUR_DK].control.napr[i_napr-1]<<6;
           // первый стрелка
           i_napr = PROJ.Muls.OneMUL[i_dk].arrow2;
           if (i_napr && DK[CUR_DK].control.napr[i_napr-1] <= GREEN_YELLOW)
               fire |= DK[CUR_DK].control.napr[i_napr-1]<<14;
          return fire;
        //}
}
#endif
//------------------------------------------------------------------------------
unsigned short Update_STATES(const bool blink,const bool fudzoff)
{
   unsigned short fire_light=0;

   #ifdef KURS_DK
      Update_STATES_KURS(blink,fudzoff); // call ->light.c
   #else
      fire_light=Update_STATES_VO();
   #endif
return(fire_light);
}
//------------------------------------------------------------------------------
// считаем время и сравниваем, используеться для проверки RTC
int Seconds_Between(SYSTEMTIME *tt, SYSTEMTIME *tl)
{
        time_t tim, lim;
        //
        tim = mktime(tt);
        lim = mktime(tl);
        //
        return (lim-tim);
}
//------------------------------------------------------------------------------
// начальные значения структур
void Init_DK(void)
{
        memset(&DK[CUR_DK],0,sizeof(DK[CUR_DK]));
        DK[CUR_DK].PROJ = &PROJ[CUR_DK];
        DK[CUR_DK].REQ.prior_req = PLAN;
        //
        DK[CUR_DK].synhro_mode = PROJ[CUR_DK].comments.inner.in.synhro_mode;
        //
        DK[CUR_DK].CUR.source=SERVICE;
        DK[CUR_DK].CUR.work = SPEC_PROG;
        DK[CUR_DK].CUR.spec_prog = SPEC_PROG_OC;
        //
        DK[CUR_DK].NEXT.work = SPEC_PROG;
        DK[CUR_DK].NEXT.spec_prog = SPEC_PROG_OC;
        //
        DK[CUR_DK].control.STA = STA_INIT;
        DK[CUR_DK].TVP.STA = STA_FIRST;
        DK[CUR_DK].VPU.STA = STA_FIRST;
        DK[CUR_DK].SERVICE.STA = STA_FIRST;
        //
        if (PROJ[CUR_DK].guard.restarts==0)
          PROJ[CUR_DK].guard.restarts=3;
        //
        if (PROJ[CUR_DK].guard.restart_interval==0)
          PROJ[CUR_DK].guard.restart_interval=3;
        //
        initTAKTS(&PROJ[CUR_DK]);
        //
        #ifdef KURS_DK
           Prepare_KURS_Structures();
        #endif
}
//------------------------------------------------------------------------------
// окончание текущего состояния
static BOOL TIME_END(void)
{
return (!DK[CUR_DK].control.len);
}
// текущее состояние для фазы
static BOOL TIME_PHASE_END(void)
{
if((DK[CUR_DK].CUR.source==PLAN)){//||(DK[CUR_DK].NEXT.source==PLAN)){
  const int nPhase =  getNextVisibledPhaseOfPlan(DK[CUR_DK].PLAN.cur.prog);
  if(nPhase!=DK[CUR_DK].CUR.prog_faza){
    if(nPhase==DK[CUR_DK].NEXT.prog_faza){ // nex phase
      DK[CUR_DK].control.len = 0;
      }else{
      //Init_DK();
      Event_Push_Str("Reset phase...\n");
      tn_reset();
      }
    }
  }
//В режиме ВПУ, выходим после отработанного Тмин фазы при старте.
/*const BOOL retValueVPU = (retOnNetworkVPU())?(!DK[CUR_DK].control.len)&retFlagTminEnd():!DK[CUR_DK].control.len;
return retValueVPU; */
return !DK[CUR_DK].control.len;
}
//------------------------------------------------------------------------------
// плюсует к времени секунды
void TIME_PLUS(SYSTEMTIME *const tt,SYSTEMTIME *const tplus,const DWORD sec_plus)
{
time_t tim;
tim = mktime(tt);
tim = tim + sec_plus;
gmtime(tim,tplus);
}
//------------------------------------------------------------------------------
//Вычислить время цикла текущей программы (Tцикла), в данном проекте должно всегда быть 1440 мин.
static void Calc_Tc(const int prog)
{
#ifdef   KURS_DK
  const int faz_n = PROG_PLAN.AmountFasa;
#else
  const int faz_n = DK[CUR_DK].PROJ->Program[prog].AmountFasa;
#endif

int tc=0;
for (int c_f=0; c_f < faz_n; c_f++)
       {
          const int n_f = Get_Next_Viz_Faz(prog, c_f);
          //
          Build_Takts(prog, prog, c_f, n_f);
          //
          tc+= osn_takt_time[CUR_DK] + Tprom_len[CUR_DK];
       }
// save structure
DK[CUR_DK].control.Tproga = tc;
}
//------------------------------------------------------------------------------
// является ли данная фаза участвующей в цикле
// prog - номер программы, prog_faza - порядковый номер фазы в программе
bool  Is_Faz_Vis(int prog, int   prog_faza)
{
        bool  ret_b=false;
        /////////////////////
   #ifdef KURS_DK
        if (PROG_PLAN.fazas[prog_faza].FasaProperty!=FAZA_PROP_TVP)
   #else
        if (DK[CUR_DK].PROJ->Program[prog].fazas[prog_faza].FasaProperty!=FAZA_PROP_TVP)
   #endif
           ret_b = true;
        ///////
        return (ret_b);
}
//------------------------------------------------------------------------------
// получает следующую видимую фазу для отображения на временной диаграмме
static int  Get_Next_Viz_Faz(const int prog,const int cur_faz)
{
int ret_i,n_faz;
//
ret_i = cur_faz;

    #ifdef KURS_DK
        n_faz= PROG_PLAN.AmountFasa;
    #else
        n_faz = DK[CUR_DK].PROJ->Program[prog].AmountFasa;
    #endif
        ////
    for (int i = cur_faz + 1; i < n_faz; i++)
      {
       if (Is_Faz_Vis(prog,i)){
            ret_i = i;
            break;
          }//if
       }//for
    //
    if (ret_i == cur_faz){
       for (int i = 0; i < cur_faz; i++)
          {
          if ( Is_Faz_Vis(prog,i) ){
              ret_i = i;
              break;
              }//if
          }//for
        }// if ret_i
return (ret_i);
}
/*----------------------------------------------------------------------------*/
// получает следующую видимую фазу по календарному плану для отображения на временной диаграмме
static int  getNextVisibledPhaseOfPlan(const int prog)
{
//wek=1..53
const int cur_week = WeekOfTheYear(&CT);
const int week_plan = DK[CUR_DK].PROJ->Year.YearCalendar[cur_week-1];
const int cur_day_of_week = DayOfWeek(&CT);
const int day_plan = DK[CUR_DK].PROJ->WeeksPlans[week_plan].Calendar[cur_day_of_week];
//for time slot and current program
const int cur_sec=CT.tm_hour*3600 + CT.tm_min*60 + CT.tm_sec;
const int c_slots = DK[CUR_DK].PROJ->DaysPlans[day_plan].AmountCalendTime;
int cur_slot=0;
int i_s=0;
//
for (int i=1; i< c_slots; i++)
  {
  i_s = DK[CUR_DK].PROJ->DaysPlans[day_plan].CalendTime[i].BeginTimeWorks;
  if (cur_sec < i_s)
        break;
      else
        cur_slot++; // текущий временной слот и фаза, в этом проекте они привязаны
  }
// сдвигаем время начала слота
if (DK[CUR_DK].synhro_mode){
  i_s = DK[CUR_DK].PROJ->DaysPlans[day_plan].CalendTime[cur_slot].BeginTimeWorks +
                       DK[CUR_DK].PROJ->comments.inner.in.syhhro_add;
  DK[CUR_DK].cur_slot_start = i_s;
  }
// определяем видимую фазу
int  WorkPhase=cur_slot;
const int n_faz= PROG_PLAN.AmountFasa; // max phase in project
// фаза невидима
if(!Is_Faz_Vis(prog,WorkPhase)){
  for(int i = WorkPhase; i < n_faz; i++)
      {
      if (Is_Faz_Vis(prog,i)){
            WorkPhase = i;
            break;
          }
      }
  // в конце нет нужных фаз перебор сначала
  if (WorkPhase == cur_slot){
       for (int i = 0; i < cur_slot; i++)
          {
          if ( Is_Faz_Vis(prog,i) ){
              WorkPhase = i;
              break;
              }//if
          }//for
        }// if ret_i
  }
return WorkPhase;
}
/*----------------------------------------------------------------------------*/
int getCurrentTimeBeginPhase(void)
{
const int cur_week = WeekOfTheYear(&CT);
const int week_plan = DK[CUR_DK].PROJ->Year.YearCalendar[cur_week-1];
const int cur_day_of_week = DayOfWeek(&CT);
const int day_plan = DK[CUR_DK].PROJ->WeeksPlans[week_plan].Calendar[cur_day_of_week];
const int nPhase =  getNextVisibledPhaseOfPlan(DK[CUR_DK].PLAN.cur.prog);

return DK[CUR_DK].PROJ->DaysPlans[day_plan].CalendTime[nPhase].BeginTimeWorks;
}
/*----------------------------------------------------------------------------*/
// проверяет состояние
// prog, faza-  зависит от контекста work
bool TEST_STA(const STATE *sta, WORK_STATE  w_sta, BYTE prog, BYTE faza)
{
        bool res=false;
        //
        if (sta->work == w_sta)
        switch (w_sta)
        {
           case SPEC_PROG:
           {
             if (sta->spec_prog == prog)
              res=true;
             break;
           }
           //
           case SINGLE_FAZA:
           {
             if (sta->faza == faza)
              res=true;
             break;
           }
           //
           case PROG_FAZA:
           {
             if (sta->prog == prog)
             if (sta->prog_faza == faza)
               res=true;
             break;
           }
           // SINGLE_FAZA
        }
        //
return (res);
}
//------------------------------------------------------------------------------
// скопировать состояния
static void Copy_STATES(STATE *dest_sta,const STATE *source_sta)
{
memcpy(dest_sta,source_sta,sizeof(STATE));
}
//------------------------------------------------------------------------------
// отключаем выходы
static void Clear_STATE(STATE *sta)
{
memset(sta,0,sizeof(STATE));
sta->spec_prog = SPEC_PROG_OC;
}
//------------------------------------------------------------------------------
// Возвращает номер недели
static int  WeekOfTheYear(SYSTEMTIME *const st)
{
       DS1390_TIME time;
       //
       time.hour =  st->tm_hour ;
       time.date = st->tm_mday;
       time.min = st->tm_min;
       //
       time.month = st->tm_mon ;
       time.sec = st->tm_sec;
       time.year = st->tm_year;
       //
       int iwn = get_week_num(&time);
return (iwn);
}
//------------------------------------------------------------------------------
//{"SUN","MON","TUE","WED","THU","FRI","SAT" };
//  0      1     2     3     4     5     6
static int DayOfWeek(SYSTEMTIME *const st)
{
       DS1390_TIME time;
       //
       time.hour =  st->tm_hour ;
       time.date = st->tm_mday;
       time.min = st->tm_min;
       //
       time.month = st->tm_mon ;
       time.sec = st->tm_sec;
       time.year = st->tm_year;
       //
       int id = get_day(&time);
        if (id)
          id--;
return (id);
}
//------------------------------------------------------------------------------
// возвращает номер программы по времени CT
static void Get_Calend_Prog(STATE *sta)
{
               int cur_week, cur_day_of_week;
               int day_plan, week_plan;
               //wek=1..53
                cur_week = WeekOfTheYear(&CT);
                week_plan = DK[CUR_DK].PROJ->Year.YearCalendar[cur_week-1];
                cur_day_of_week = DayOfWeek(&CT);
                day_plan = DK[CUR_DK].PROJ->WeeksPlans[week_plan].Calendar[cur_day_of_week];
                // serch for time slot and current program
                int cur_sec=CT.tm_hour*3600 + CT.tm_min*60 + CT.tm_sec;
                //
                int cur_slot=0;
                int i_s=0;
                int c_slots = DK[CUR_DK].PROJ->DaysPlans[day_plan].AmountCalendTime;
                //
                for (int i=1; i< c_slots; i++)
                {
                  i_s = DK[CUR_DK].PROJ->DaysPlans[day_plan].CalendTime[i].BeginTimeWorks;
                  //
                  if (cur_sec < i_s)
                        break;
                  else
                    cur_slot++;
                }
                // сдвигаем время начала слота
                if (DK[CUR_DK].synhro_mode)
                {
                   i_s = DK[CUR_DK].PROJ->DaysPlans[day_plan].CalendTime[cur_slot].BeginTimeWorks +
                       DK[CUR_DK].PROJ->comments.inner.in.syhhro_add;
                   DK[CUR_DK].cur_slot_start = i_s;
                }
                //
                sta->prog =
                    DK[CUR_DK].PROJ->DaysPlans[day_plan].CalendTime[cur_slot].NumProgramWorks;
                sta->spec_prog =
                   DK[CUR_DK].PROJ->DaysPlans[day_plan].CalendTime[cur_slot].SpecProg;
}
//------------------------------------------------------------------------------
#ifdef KURS_DK
// загружаем программу из FLASH I2C
void Load_Plan_Prog(int i_prog)
{
if (!DK[CUR_DK].no_LOAD_PROG_PLAN)
  flash_rd(FLASH_PROGS, CUR_DK*sizeof(TPROGRAMS) +  i_prog*sizeof(TPROGRAM),
                  (unsigned char*)&PROG_PLAN, sizeof(TPROGRAM));
}
#endif
//------------------------------------------------------------------------------
// Заполняет поля - work
static void  Fill_STATE_work(STATE *sta)
{
if (sta->spec_prog) sta->work = SPEC_PROG;
               else sta->work = PROG_FAZA;
}
//------------------------------------------------------------------------------
// Работа по календарному плану и программам
static void GO_PLAN(void)
{
   //BYTE  cur_prog_faza;       // номер фазы в программе

   switch (DK[CUR_DK].PLAN.STA)
   {
        // первый вход
        case STA_INIT:
        {
                Get_Calend_Prog(&DK[CUR_DK].PLAN.cur);
                Fill_STATE_work(&DK[CUR_DK].PLAN.cur);
                DK[CUR_DK].PLAN.cur.source = PLAN;
                //
                #ifdef KURS_DK
                    Load_Plan_Prog(DK[CUR_DK].PLAN.cur.prog);
                    //cur_prog_faza =  PROG_PLAN.AmountFasa-1;
                #else
                    //cur_prog_faza =  DK[CUR_DK].PROJ->Program[DK[CUR_DK].PLAN.cur.prog].AmountFasa-1;
                #endif
                 // ищем фазу видимую по календарному плану
                 DK[CUR_DK].PLAN.cur.prog_faza =
                         //Get_Next_Viz_Faz(DK[CUR_DK].PLAN.cur.prog, cur_prog_faza);
                         getNextVisibledPhaseOfPlan(DK[CUR_DK].PLAN.cur.prog);
                //
                Copy_STATES(&DK[CUR_DK].NEXT, &DK[CUR_DK].PLAN.cur);
                //
                Calc_Tc(DK[CUR_DK].PLAN.cur.prog);  // расче времени всего цикла
                DK[CUR_DK].NEXT.presence=true;
                DK[CUR_DK].PLAN.STA = STA_GET_REQ;
                break;
        }
        ////////////////////////
        // ждем освобождения
        case STA_GET_REQ:
        {
                D_W("STA_GET_REQ");
                //
                if (DK[CUR_DK].NEXT.presence==false)
                {
                   D_W("DK[CUR_DK].NEXT.presence==false");
                   //
                   Get_Calend_Prog(&DK[CUR_DK].PLAN.next);
                   // флаги
                   if (DK[CUR_DK].PLAN.next.spec_prog)
                      DK[CUR_DK].PLAN.next.work = SPEC_PROG;
                   else
                      DK[CUR_DK].PLAN.next.work = PROG_FAZA;
                   // PROG1->PROG2
                   if ((!DK[CUR_DK].PLAN.cur.spec_prog) &&
                       (!DK[CUR_DK].PLAN.next.spec_prog))
                   {
                       // та же программа
                       if (DK[CUR_DK].PLAN.cur.prog == DK[CUR_DK].PLAN.next.prog)
                       {
                              // состояние не было установлено
                              if (!DK[CUR_DK].PLAN.cur.set)
                              {
                                    Copy_STATES(&DK[CUR_DK].PLAN.next, &DK[CUR_DK].PLAN.cur);
                              }
                              else
                              {
                                    D_W("следующее по плану");
                                   // следующее по плану
                                   DK[CUR_DK].PLAN.next.prog_faza =
                                       //Get_Next_Viz_Faz(-1000, DK[CUR_DK].PLAN.cur.prog_faza);
                                       Get_Next_Viz_Faz(DK[CUR_DK].PLAN.cur.prog,DK[CUR_DK].PLAN.cur.prog_faza);
                              }
                       }
                       else  //переходим на другую программу, сюда не должны попасть, программа одна
                       {
                         // случайно попали- это предохранительный костыль
                         DK[CUR_DK].PLAN.cur.prog = DK[CUR_DK].PLAN.next.prog = 0;
                         break;
                            /*
                            #ifdef KURS_DK
                              Load_Plan_Prog(DK[CUR_DK].PLAN.cur.prog);
                              // ищем первую видимую фазу
                              cur_prog_faza =  PROG_PLAN.AmountFasa-1;
                            #else
                              cur_prog_faza =  DK[CUR_DK].PROJ->Program[DK[CUR_DK].PLAN.cur.prog].AmountFasa-1;
                            #endif  */
                       }
                   } // PROG1->PROG2
                   Copy_STATES(&DK[CUR_DK].PLAN.cur, &DK[CUR_DK].PLAN.next);
                   Copy_STATES(&DK[CUR_DK].NEXT, &DK[CUR_DK].PLAN.cur);
                   //
                   DK[CUR_DK].NEXT.presence=true;
                   DK[CUR_DK].NEXT.source = PLAN;
                }
        }
   }
}
//------------------------------------------------------------------------------
// ищем чему соотсветствует запрос
static bool Calculate_TVP(void)
{
        TPROGRAM *proga;
        bool found=false;
        int  pres1, pres2;
        int  fpres1, fpres2;
        //
        DK[CUR_DK].REQ.req[TVP].work = PROG_FAZA;//, DK[CUR_DK].CUR
        DK[CUR_DK].REQ.req[TVP].prog =  DK[CUR_DK].CUR.prog;
        DK[CUR_DK].REQ.req[TVP].source = TVP;
        //
        #ifdef KURS_DK
           proga = &DK[CUR_DK].PROJ->Program;
        #else
           proga = &DK[CUR_DK].PROJ->Program[cur_prog];
        #endif
        for (int f_i=0; f_i < proga->AmountFasa; f_i++)
        {
           if (proga->fazas[f_i].FasaProperty != FAZA_PROP_FIKS)
           {
              pres1 = DK[CUR_DK].tvps[0].pres;
              pres2 = DK[CUR_DK].tvps[1].pres;
             if (fpres1 && pres1)
             {
               DK[CUR_DK].REQ.req[TVP].prog_faza = f_i;
               found=true;
               break;
             }
             //
             if (fpres2 && pres2)
             {
               DK[CUR_DK].REQ.req[TVP].prog_faza = f_i;
               found=true;
               break;
             }
           }
        }
        //
        if (found)
          Copy_STATES(&DK[CUR_DK].TVP.cur, &DK[CUR_DK].REQ.req[TVP]);
        //
return (found);
}
//------------------------------------------------------------------------------
//
static void GO_TVP(void)
{
        switch (DK[CUR_DK].TVP.STA)
        {
                // первый вход - обработка запроса
                case STA_FIRST:
                {
                   if (  Calculate_TVP() )
                     DK[CUR_DK].TVP.STA=STA_SEE_REQUEST;
                }
                ///
                case STA_SEE_REQUEST:
                {
                    //ждем отработки хотяб одной фазы не ТВП
                    if (DK[CUR_DK].CUR.source!=TVP)
                    {
                       // ждем освободения следующего состояния
                       if (!DK[CUR_DK].NEXT.set)
                       if (DK[CUR_DK].CUR.prog_faza!= DK[CUR_DK].TVP.cur.prog_faza)
                       {
                          if (DK[CUR_DK].NEXT.source!=TVP)
                          {
                             Copy_STATES(&DK[CUR_DK].NEXT, &DK[CUR_DK].TVP.cur);
                             DK[CUR_DK].TVP.STA=STA_EXIT;
                          }
                       }
                       else
                       {
                            DK[CUR_DK].TVP.STA=STA_EXIT;
                            DK[CUR_DK].TVP.cur.set=true;

                       }
                    }
                    break;
                }
                // вышли
                case STA_EXIT:
                {
                    // ждем установки
                    if (DK[CUR_DK].TVP.cur.set)
                    {
                       // какой запрос отработали
                      for (int tvp_i=0; tvp_i< MaxTVP; tvp_i++)
                      {
                        if (DK[CUR_DK].CUR.prog_faza ==
                               DK[CUR_DK].tvp_faza[tvp_i])
                          DK[CUR_DK].tvps[tvp_i].pres=0;
                      }
                       ///
                      // if (DK[CUR_DK].CUR.prog_faza == DK[CUR_DK].tvp2_faza)
                      //   DK[CUR_DK].tvps[1].pres=0;
                       ///
                       /*
                       if (DK[CUR_DK].tvps[0].pres || DK[CUR_DK].tvps[1].pres)
                       {
                          //остался еще один запрос
                          DK[CUR_DK].TVP.STA = STA_SEE_REQUEST;
                          break;
                       }
                       else
                       {
                       */
                          //установили, снимаем флаг запроса
                          DK[CUR_DK].REQ.req[TVP].presence=false;
                          DK[CUR_DK].TVP.cur.presence=false;
                          //DK[CUR_DK].TVP.enabled = false;
                          DK[CUR_DK].TVP.STA=STA_FIRST;
                       //}
                    }
                    //
                    if (DK[CUR_DK].CUR.source!=TVP)
                    if (DK[CUR_DK].NEXT.source!=TVP)
                    {
                       // че мы здесь вообще делаем
                       DK[CUR_DK].TVP.STA=STA_FIRST;
                    }
                    break;
                }
        }
}
//------------------------------------------------------------------------------
static void GO_ALARM(void)
{
                  if (DK[CUR_DK].REQ.req[ALARM].presence)
                    {
                       if ((DK[CUR_DK].REQ.req[ALARM].work==SPEC_PROG))
                       {
                         Copy_STATES(&DK[CUR_DK].NEXT, &DK[CUR_DK].REQ.req[ALARM]);
                         Copy_STATES(&DK[CUR_DK].SERVICE.cur, &DK[CUR_DK].REQ.req[ALARM]);
                       }
                    }
}
//------------------------------------------------------------------------------
static void GO_TUBMLER(void)
{
                  if (DK[CUR_DK].REQ.req[TUMBLER].presence)
                    {
                       if ((DK[CUR_DK].REQ.req[TUMBLER].work==SPEC_PROG) ||
                          (!DK[CUR_DK].NEXT.set) )
                       {
                         Copy_STATES(&DK[CUR_DK].NEXT, &DK[CUR_DK].REQ.req[TUMBLER]);
                         Copy_STATES(&DK[CUR_DK].SERVICE.cur, &DK[CUR_DK].REQ.req[TUMBLER]);
                       }
                    }
}
//------------------------------------------------------------------------------
static void GO_SERVICE(void)
{
    switch (DK[CUR_DK].SERVICE.STA)
        {
                // первый вход - обработка запроса
                case STA_FIRST:
                {
                    if (DK[CUR_DK].REQ.req[SERVICE].presence)
                    {
                       if ((DK[CUR_DK].REQ.req[SERVICE].work==SPEC_PROG) ||
                          (!DK[CUR_DK].NEXT.set) )
                       {
                         Copy_STATES(&DK[CUR_DK].NEXT, &DK[CUR_DK].REQ.req[SERVICE]);
                         Copy_STATES(&DK[CUR_DK].SERVICE.cur, &DK[CUR_DK].REQ.req[SERVICE]);
                       }

                    }
                       break;
                }
                //// вышли
                case STA_EXIT:
                {
                    break;
                }
        }// end switch
}
//----------------------------------------------------------------
static void GO_VPU(void)
{
  switch (DK[CUR_DK].VPU.STA)
        {
                // первый вход - обработка запроса
                case STA_FIRST:
                {   // есть запрос и освобождение после перегрузки ДК
                    if(DK[CUR_DK].REQ.req[VPU].presence)
                    {
                        // ждем освободения следующего состояния
                        if ((!DK[CUR_DK].NEXT.set) ||
                           (DK[CUR_DK].REQ.req[VPU].work==SPEC_PROG))
                        {
                          Copy_STATES(&DK[CUR_DK].NEXT, &DK[CUR_DK].REQ.req[VPU]);
                          Copy_STATES(&DK[CUR_DK].VPU.cur, &DK[CUR_DK].REQ.req[VPU]);
                        }
                    }
                       break;
                }
                //// вышли
                case STA_EXIT:
                {
                    break;
                }
        }// end switch
}
/*----------------------------------------------------------------------------*/
// логическая обработка данных ДК
static void MODEL(void)
{
// Общие состояния ДК
switch(DK[CUR_DK].REQ.prior_req)
  {
  case ALARM:   {GO_ALARM();  break;}
  case TUMBLER: {GO_TUBMLER();break;}
  case SERVICE: {GO_SERVICE();break;}
  case VPU:     {GO_VPU();    break;}
  case TVP:     {GO_TVP();    break;}
  case PLAN:    {GO_PLAN();   break;}
  }
}
//------------------------------------------------------------------------------
//  обработка поступающих запросов
static int REQUESTS(void)
{
        // каким-либо образом получаем запросы
        // и распихиваем в структуру DK[CUR_DK].requests
        if (DK[CUR_DK].REQ.req[ALARM].presence)
        {
           DK[CUR_DK].REQ.prior_req=ALARM;
           return (RET_OK);
        }
        //
        if (DK[CUR_DK].REQ.req[TUMBLER].presence)
        {
           DK[CUR_DK].REQ.prior_req=TUMBLER;
           return (RET_OK);
        }
        //
        if (DK[CUR_DK].REQ.req[SERVICE].presence)
        {
           DK[CUR_DK].REQ.prior_req=SERVICE;
           return (RET_OK);
        }
        //
        if((DK[CUR_DK].REQ.req[VPU].presence)&&(retFlagTminEnd())) // затянуть переход проверка готовности по reset
        {
           DK[CUR_DK].REQ.prior_req=VPU;
           return (RET_OK);
        }
        //
        if (DK[CUR_DK].REQ.req[TVP].presence)
        {
          if (DK[CUR_DK].TVP.enabled)
          {
             DK[CUR_DK].REQ.prior_req=TVP;
             return (RET_OK);
          }
        }
DK[CUR_DK].REQ.prior_req=PLAN;
return (RET_OK);
}
//------------------------------------------------------------------------------
//генерирует таблицу тактов на основании CUR->NEXT
static void GEN_TAKTS(void)
{
     #ifdef KURS_DK
       //грузим текущую программу
        if (!DK[CUR_DK].no_LOAD_PROG_PLAN)
                  Load_Plan_Prog(DK[CUR_DK].CUR.prog);
     #endif
         Check_TVP_En();
        // СПЕЦ. ФАЗА-> ......
        if (DK[CUR_DK].CUR.work == SINGLE_FAZA)
        {
          //СПЕЦ. ФАЗА-> СПЕЦ. ФАЗА
          if (DK[CUR_DK].NEXT.work == SINGLE_FAZA)
            Build_Takts(NO_PROG,
                        NO_PROG,
                        DK[CUR_DK].CUR.faza,
                        DK[CUR_DK].NEXT.faza);
          //////
          //СПЕЦ. ФАЗА-> ФАЗА ПРОГРАММЫ
           if (DK[CUR_DK].NEXT.work == PROG_FAZA)
            Build_Takts(NO_PROG,
                        DK[CUR_DK].NEXT.prog,
                        DK[CUR_DK].CUR.faza,
                        DK[CUR_DK].NEXT.prog_faza);
          //
           //СПЕЦ. ФАЗА-> СПЕЦ. ПРОГРАММА
           if (DK[CUR_DK].NEXT.work == SPEC_PROG)
            Build_Takts(NO_PROG,
                        NO_PROG,
                        DK[CUR_DK].CUR.faza,
                        DK[CUR_DK].NEXT.faza);

        }
        // ФАЗА ПРоГРАММЫ-> ......
        if (DK[CUR_DK].CUR.work == PROG_FAZA)
        {
          // .... -> СПЕЦ. ФАЗА
          if (DK[CUR_DK].NEXT.work == SINGLE_FAZA)
            Build_Takts(DK[CUR_DK].CUR.prog,
                        NO_PROG,
                        DK[CUR_DK].CUR.prog_faza,
                        DK[CUR_DK].NEXT.faza);
          // ......-> ФАЗА ПРОГРАММЫ
           if (DK[CUR_DK].NEXT.work == PROG_FAZA)
            Build_Takts(DK[CUR_DK].CUR.prog,
                        DK[CUR_DK].NEXT.prog,
                        DK[CUR_DK].CUR.prog_faza,
                        DK[CUR_DK].NEXT.prog_faza);
          // .... -> СПЕЦ. ПРОГРАММА
          if (DK[CUR_DK].NEXT.work == SPEC_PROG)
            Build_Takts(DK[CUR_DK].CUR.prog,
                        NO_PROG,
                        DK[CUR_DK].CUR.prog_faza,
                        DK[CUR_DK].NEXT.faza);
        }
}
//------------------------------------------------------------------------------
// устанавдивает цвет следующего направления
static void SET_PROM_STATE_LEDS(void)
{
const int maxDir = DK[CUR_DK].PROJ->Directs.countDirectCCG;
for (int i_napr=0; i_napr< maxDir; i_napr++)
  {
  const int prom_indx = DK[CUR_DK].control.prom_indx[i_napr];
  DK[CUR_DK].control.napr[i_napr] = prom_takts[CUR_DK][i_napr][prom_indx].col;
  }
}
//------------------------------------------------------------------------------
// установить состояние на направления
static void SET_OSN_STATE_LEDS(void)
{
const int maxDir = DK[CUR_DK].PROJ->Directs.countDirectCCG;
for (int i_napr=0; i_napr < maxDir; i_napr++)
    {
    DK[CUR_DK].control.napr[i_napr] = osn_takt[CUR_DK][i_napr];
    }
}
//------------------------------------------------------------------------------
// установить состояние на локальных силовых каналах
static void SET_SPEC_PROG_LEDS(void)
{
STATES_LIGHT  fill_col = ALL_OFF;
//
switch (DK[CUR_DK].CUR.spec_prog)
  {
  case SPEC_PROG_YF: fill_col = YELLOW_FLASH; break;
  case SPEC_PROG_OC: fill_col = ALL_OFF;      break;
  case SPEC_PROG_KK: fill_col = RED;          break;
  }//sw
for (int i_napr=0; i_napr< DK[CUR_DK].PROJ->Directs.countDirectCCG; i_napr++)
   DK[CUR_DK].control.napr[i_napr] = fill_col;
}
//------------------------------------------------------------------------------
// проверяет, есть ли в текущей программе ТВП
static bool Check_TVP_En(void)
{
       TPROGRAM  *proga;
       int   faz_n;
       BOOL   ret_b = false;
       //int   tvp_n;
       //
       DK[CUR_DK].TVP.enabled=false;
       ///
       #ifdef   KURS_DK
            faz_n = PROG_PLAN.AmountFasa;
            proga = &PROG_PLAN;
       #else
            faz_n = DK[CUR_DK].PROJ->Program[DK[CUR_DK].CUR.prog].AmountFasa;
            proga = &DK[CUR_DK].PROJ->Program[DK[CUR_DK].CUR.prog];
       #endif
       /// чистим
       for (int tvp_i=0; tvp_i< MaxTVP; tvp_i++)
       {
         DK[CUR_DK].tvp_en[tvp_i] = false;
       }
       ///

    for (int f_i=0; f_i < faz_n; f_i++)
        {

           if (proga->fazas[f_i].FasaProperty != FAZA_PROP_FIKS)
           {
             DK[CUR_DK].TVP.enabled=true;
             ////////////
             for (int tvp_i=0; tvp_i< MaxTVP; tvp_i++)
             {
                /*if (proga->fazas[f_i].tvps[tvp_i].pres)
                {
                  DK[CUR_DK].tvp_faza[tvp_i]=f_i;
                  ///
                  DK[CUR_DK].tvp_en[tvp_i] = true;
                  // proga->fazas[f_i].tvps[tvp_i].pres;
                } */


             }
             /////////////
             /*
             tvp_n = proga->fazas[f_i].tvps[0].pres;
             if (tvp_n==1)
               DK[CUR_DK].tvp1_faza=f_i;
             //
             if (tvp_n==2)
               DK[CUR_DK].tvp2_faza=f_i;
             ///////////////
             ////////////
             tvp_n = proga->fazas[f_i].tvps[1].pres;
             if (tvp_n==1)
               DK[CUR_DK].tvp1_faza=f_i;
             //
             if (tvp_n==2)
               DK[CUR_DK].tvp2_faza=f_i;
             ///////////////
             */
             ret_b = true;
             //return (true);
           }
        }
//
return (ret_b);
}
//------------------------------------------------------------------------------
// Проверяем на сколько убежал цикл
// Возвращает время убегания
static int  Check_Synhro(bool noKK)
{
      int sec_part=0;
      //////////////
      if ((DK[CUR_DK].CUR.source == PLAN) &&
          (DK[CUR_DK].CUR.spec_prog == SPEC_PROG_YF))
       return 0;
      /////////////////

      if (DK[CUR_DK].synhro_mode)
      if (DK[CUR_DK].NEXT.work==PROG_FAZA)
      //if (DK[CUR_DK].CUR.work==PROG_FAZA)
      if (DK[CUR_DK].NEXT.prog_faza==0)
      {
         //текущее время
         int cur_sec = CT.tm_hour*3600 + CT.tm_min*60 + CT.tm_sec;
         // вычитаем начало текущего временного слота
         cur_sec = cur_sec - DK[CUR_DK].cur_slot_start;
         //остаток цикла
         sec_part = cur_sec % DK[CUR_DK].control.Tproga;
         // добавляем к режиму КК
         //if (sec_part)
         //   kk = sec_part;
         //if (sec_part==0)
         if (sec_part<3)
         if (noKK)
           return 0;
         // цикл сбился и нужна корректировка
         int tim = DK[CUR_DK].control.Tproga - sec_part;
         //if (noKK==false)
             tim=tim- DK[CUR_DK].PROJ->guard.kk_len ;
         ////////
         if (noKK)
         { // true - вызов в рабочем цикле, без КК
           // учет дельты
           if (abs(tim)<3)
            return 0;
         }
         else
         { // false - вызов с KK, при старте
           if (tim==0)
             return 0;
         }
         ////текущий момент попал в хвост КК
         if (tim<0){
            tim = tim + DK[CUR_DK].control.Tproga;
            }
         DK[CUR_DK].control.len = tim;
         DK[CUR_DK].CUR.spec_prog = SPEC_PROG_YF;
         DK[CUR_DK].CUR.work = SPEC_PROG;
         DK[CUR_DK].CUR.source = PLAN;
         SET_SPEC_PROG_LEDS();
         DK[CUR_DK].control.STA=STA_SPEC_PROG;
         return tim;
      }
return (0);
}
//------------------------------------------------------------------------------
// сравнивает состояния
static BOOL EQ_States(STATE *sta1, STATE *sta2)
{
bool res=false;
//
if (sta1->work == sta2->work)
  switch (sta1->work)
    {
    case SPEC_PROG:
      {
      if (sta1->spec_prog == sta2->spec_prog)
        res=true;
       break;
      }
    case SINGLE_FAZA:
      {
      if (sta1->faza == sta2->faza)
        res=true;
      break;
      }
    case PROG_FAZA:
      {
      if (sta1->prog == sta2->prog)
        if (sta1->prog_faza == sta2->prog_faza)
          res=true;
      break;
      }
    }
return (res);
}
//------------------------------------------------------------------------------
static void Event_Change_Fase(void)
{
     char buf[128];
     ///
     if (!PROJ[CUR_DK].jornal.faz)
       return;
     ////
        if (DK[CUR_DK].CUR.work == PROG_FAZA)
        {
          snprintf(buf, sizeof(buf), "ПРОГР=%d, ФАЗА=%d",
             (DK[CUR_DK].CUR.prog+1),(DK[CUR_DK].CUR.prog_faza+1));

        }
        //
        if (DK[CUR_DK].CUR.work == SINGLE_FAZA)
        {
           snprintf(buf, sizeof(buf), "ФАЗА=%d",
             (DK[CUR_DK].CUR.faza+1));

           //S_W("Oaae. oaca=" + IntToStr(DK[CUR_DK].CUR.faza+1));
        }
        //
        if (DK[CUR_DK].CUR.work == SPEC_PROG)
        {
          //snprintf(buf, sizeof(buf), "SUCCESS:WORK=SPEC_PROG SPEC_PROG=%d",
          //   (DK[CUR_DK].CUR.spec_prog));
          strcpy(buf,"Спец. пр.=");
          if (DK[CUR_DK].CUR.spec_prog==1)
            strcat(buf,"ЖМ");
          ///
          if (DK[CUR_DK].CUR.spec_prog==2)
            strcat(buf,"ОС");
          ///
        }
        ////
        strcat(buf,". Ист=");
        if (DK[CUR_DK].CUR.source==ALARM) strcat(buf,"АВАРИЯ");
        if (DK[CUR_DK].CUR.source==TUMBLER) strcat(buf,"ТУМБЛЕР");

        if (DK[CUR_DK].CUR.source==SERVICE) strcat(buf,"СЕРВИС");
        if (DK[CUR_DK].CUR.source==VPU) strcat(buf,"ВПУ");
        if (DK[CUR_DK].CUR.source==TVP) strcat(buf,"ТВП");
        if (DK[CUR_DK].CUR.source==PLAN) strcat(buf,"ПЛАН");
        //
        strcat(buf,"\n");
        Event_Push_Str(buf);
}
//----------------------------------------------------------------------------//
//получить время защитное
static DWORD getTimeGuard(void)
{
return(DK[CUR_DK].PROJ->guard.green_min<DK[CUR_DK].PROJ->guard.red_min)?
           (DK[CUR_DK].PROJ->guard.red_min):(DK[CUR_DK].PROJ->guard.green_min);
}
//----------------------------------------------------------------------------//
static void checkTimeGuard(void)
{
//защитный интервал
if(DK[CUR_DK].control.STA==STA_OSN_TAKT){
  // установим Тмин
  const DWORD Tmin = getTimeGuard();
  if ((DK[CUR_DK].control.startLen - DK[CUR_DK].control.len) >  Tmin){// защитный интервал по Тмин зел
    DK[CUR_DK].control.len = 0;
    }
  // повторение фаз после VPU и ТВП
  if(DK[CUR_DK].CUR.source==PLAN)
    if(DK[CUR_DK].OLD.source==PLAN){
      if (DK[CUR_DK].CUR.prog_faza==DK[CUR_DK].OLD.prog_faza){
        DK[CUR_DK].control.len = 0;
        }
    }
  }
}
//----------------------------------------------------------------------------//
// вызывается при переходах состояний
static int CUR_NEXT(void)
{
static BYTE nextPhase = 0;

D_W("CUR_NEXT. Change STATE\n");
//
if((TEST_STA(&DK[CUR_DK].CUR, SPEC_PROG, SPEC_PROG_OC,0)) ||
    (TEST_STA(&DK[CUR_DK].CUR, SPEC_PROG, SPEC_PROG_YF,0)))
    if (DK[CUR_DK].NEXT.work!=SPEC_PROG)
      {
      if (DK[CUR_DK].flash){
        if (!ligh_load_init())
            DK_HALT();}
      //
      POWER_SET(true);
      //
      //if (TEST_STA(&DK[CUR_DK].CUR, SPEC_PROG, SPEC_PROG_YF,0))
      //{
        DK[CUR_DK].PLAN.STA=STA_INIT;;
        MODEL();
      //}
      // запуск KK
      // но проверим рассинхронизацию
      if (Check_Synhro(false)){return (0);}
      //
      memcpy(&DK[CUR_DK].control.start, &CT, sizeof(SYSTEMTIME));
      TIME_PLUS(&CT, &DK[CUR_DK].control.end, DK[CUR_DK].PROJ->guard.kk_len);
      //
      DK[CUR_DK].control.len = DK[CUR_DK].PROJ->guard.kk_len;
      DK[CUR_DK].CUR.spec_prog = SPEC_PROG_KK;
      DK[CUR_DK].CUR.source = PLAN;
      SET_SPEC_PROG_LEDS();
      //
      return 0;
      }// end DK[CUR_DK].NEXT.work!=SPEC_PROG
//
if (TEST_STA(&DK[CUR_DK].CUR, SPEC_PROG, SPEC_PROG_KK,0))
  if (DK[CUR_DK].NEXT.work!=SPEC_PROG){
    DK[CUR_DK].PLAN.STA=STA_INIT;;
    MODEL();
    }
//
if (Check_Synhro(true)){return (0);}
//
Copy_STATES(&DK[CUR_DK].OLD, &DK[CUR_DK].CUR);
Copy_STATES(&DK[CUR_DK].CUR, &DK[CUR_DK].NEXT);
nextPhase = DK[CUR_DK].NEXT.faza; // сохраняем для фильтра мин. зел
// обнуляем следующее состояние
Clear_STATE(&DK[CUR_DK].NEXT);
//
if(!EQ_States(&DK[CUR_DK].CUR, &DK[CUR_DK].OLD))Event_Change_Fase();
// перешли на ОС?
if (TEST_STA(&DK[CUR_DK].CUR, SPEC_PROG, SPEC_PROG_OC,0)){
  POWER_SET(false);
  }else{
  POWER_SET(true);
  }
//memset(&DK[CUR_DK].NEXT,0,sizeof(STATE));
//DK[CUR_DK].NEXT.spec_prog = SPEC_PROG_OC;
// обратная связь
//установили плановую
if(DK[CUR_DK].CUR.source == PLAN)DK[CUR_DK].PLAN.cur.set = true;
//
if(DK[CUR_DK].CUR.source == TVP) DK[CUR_DK].TVP.cur.set = true;
//
if(DK[CUR_DK].CUR.source == VPU) DK[CUR_DK].VPU.cur.set = true;
//
if(DK[CUR_DK].CUR.source == SERVICE) DK[CUR_DK].SERVICE.cur.set = true;
// определяем - что будем следующим
// если спец. прога - своё состояние
if(DK[CUR_DK].CUR.work== SPEC_PROG){
    SET_SPEC_PROG_LEDS();
    DK[CUR_DK].control.STA = STA_SPEC_PROG;
    }else{
    Calc_Tc(DK[CUR_DK].CUR.prog);
    GEN_TAKTS();
    SET_OSN_STATE_LEDS();
    DK[CUR_DK].control.STA = STA_OSN_TAKT;
    }
// фиксируем время начала состояния
memcpy(&DK[CUR_DK].control.start, &CT, sizeof(SYSTEMTIME));
// определяем время окончания
if ((DK[CUR_DK].CUR.source==PLAN) || (DK[CUR_DK].CUR.source==TVP)){
  // время начала текущей фазы
  const time_t timeStartPh = getCurrentTimeBeginPhase();    // время начала текущей фазы
  const time_t timeCurrent = CT.tm_hour*3600 + CT.tm_min*60 + CT.tm_sec;// время когда в эту фазу попали
  DWORD TimeDelta;
  if(timeStartPh<=timeCurrent)TimeDelta = timeCurrent - timeStartPh;
                         else DK[CUR_DK].control.startLen = DK[CUR_DK].control.len = 0;
  if(TimeDelta<=osn_takt_time[CUR_DK])
    DK[CUR_DK].control.startLen = DK[CUR_DK].control.len  = osn_takt_time[CUR_DK]-TimeDelta;
    else
    DK[CUR_DK].control.startLen = DK[CUR_DK].control.len = 0; // время вышло, сбросс
  // YF
  if(TEST_STA(&DK[CUR_DK].CUR, SPEC_PROG, SPEC_PROG_YF,0))osn_takt_time[CUR_DK]=1;
  // время окончания
  TIME_PLUS(&CT, &DK[CUR_DK].control.endPhase,DK[CUR_DK].control.len);
  }else{
    //static BYTE step = 0;
    // Для ALARM... - длительность - не определена
    TIME_PLUS(&CT, &DK[CUR_DK].control.end, 10);
    DK[CUR_DK].control.len = 1;
    //установить защитное время в режиме ВПУ
    if((DK[CUR_DK].CUR.source==VPU)&&(DK[CUR_DK].OLD.source==VPU)){
      if(nextPhase!=DK[CUR_DK].OLD.faza){
        const DWORD Tmin = getTimeGuard();
        DK[CUR_DK].control.startLen = DK[CUR_DK].control.len = Tmin;
        // время окончания
        TIME_PLUS(&CT, &DK[CUR_DK].control.endPhase,DK[CUR_DK].control.len);
        }
      }
    }
return (0);
}
//----------------------------------------------------------------------------//
// устанавдивает следующее состояние на удаленных СУСО
// корректирует время окончания, еапример, для ТВП или срочных запросов
// Включает коммуникацию и потерю связи. Учитывает общее время начала и
// возможность продления текущего состояния из-за неустойчивой связи
static void SET_NEXT_STATE(void)
{
D_W("SET_NEXT_STATE()\n");
// повторение фаз после ТВП
if(DK[CUR_DK].CUR.source==TVP)
  if(DK[CUR_DK].NEXT.source==PLAN){
    if(DK[CUR_DK].CUR.prog_faza==DK[CUR_DK].NEXT.prog_faza){
      DK[CUR_DK].NEXT.presence=false;
      DK[CUR_DK].PLAN.cur.set = true;
      GO_PLAN();
      }
  }
// ищем фазу после ВПУ
if((DK[CUR_DK].CUR.source==VPU)&&(DK[CUR_DK].NEXT.source==PLAN)){
  DK[CUR_DK].NEXT.prog_faza = getNextVisibledPhaseOfPlan(DK[CUR_DK].PLAN.cur.prog);
  DK[CUR_DK].PLAN.cur.prog_faza = DK[CUR_DK].PLAN.next.prog_faza = DK[CUR_DK].NEXT.prog_faza;
  }
//проверка на вызов любого режима не из PLAN
if (DK[CUR_DK].CUR.source > DK[CUR_DK].NEXT.source){
  D_W("More prioritet\n");
  D_W("short time\n");
  //вызвали SPEC_PROG?
  if (DK[CUR_DK].CUR.work == SPEC_PROG){
    memcpy(&DK[CUR_DK].control.end, &CT, sizeof(SYSTEMTIME));
    CUR_NEXT();
    }else{
    checkTimeGuard();
    }
  }
}
//------------------------------------------------------------------------------
static void Check_Low_Level_Spec_Prog(void)
{
    static BYTE  req_n;
    req_n = DK[CUR_DK].REQ.prior_req;
    if (req_n == PLAN)
      return;
    //
    if (DK[CUR_DK].REQ.prior_req <= DK[CUR_DK].CUR.source)
     if (DK[CUR_DK].REQ.req[req_n].work == SPEC_PROG)
     {
         //включаем спец. прогу
         memcpy(&DK[CUR_DK].control.end, &CT, sizeof(SYSTEMTIME));
         //
         DK[CUR_DK].control.len=0;
         //
         CUR_NEXT();
     }
}
/*----------------------------------------------------------------------------*/
// переключатель состояний ДК. автомат переключения силовых выходов
static bool CONTROL(void)
{
  // Тикаем считаем время в сек.
  if(DK[CUR_DK].control.len)
      DK[CUR_DK].control.len--;
  // проверка неотложных запросов
  Check_Low_Level_Spec_Prog();
  //автомат перключения
  const BYTE statControl = DK[CUR_DK].control.STA;
  switch (statControl)
        {
                case STA_INIT:
                {
                    // устанавливаем время CUR=OS
                    memcpy(&DK[CUR_DK].control.start, &CT, sizeof(SYSTEMTIME));
                    TIME_PLUS(&DK[CUR_DK].control.start, &DK[CUR_DK].control.end, 2);
                    //
                    DK[CUR_DK].control.len = 2;
                    DK[CUR_DK].control.STA = STA_SPEC_PROG;
                    SET_SPEC_PROG_LEDS();
                    break;
                }
                //
                case STA_SPEC_PROG:
                {
                   if (TIME_END())
                   {
                      CUR_NEXT();
                      break;
                   }
                   // смотрим новые запросы
                   if (DK[CUR_DK].NEXT.presence)
                   {
                        SET_NEXT_STATE();
                   }
                   break;
                }
                // шаг основного такта
                case STA_OSN_TAKT:
                {
                /*------------------------------------------------------------*/
                // установим Тмин
                const DWORD Tmin = getTimeGuard();
                if ((DK[CUR_DK].control.startLen - DK[CUR_DK].control.len) >  Tmin){// защитный интервал по Тмин зел
                  //флаги об окончании Тмин  в фазе
                  clearStatusRestart();
                  }
                /*------------------------------------------------------------*/
                   // смотрим новые запросы
                   if (DK[CUR_DK].NEXT.presence)
                   {
                        SET_NEXT_STATE();
                   }
                   if (TIME_PHASE_END())//закончилось время
                   {
                      //закончилось время основного такта, фиксируем значения начала про. тактов
                      const time_t timeStartTpr = getCurrentTimeBeginPhase()+osn_takt_time[CUR_DK];
                      //фиксируем следующее состояние
                      DK[CUR_DK].NEXT.set = true;
                      GEN_TAKTS();
                      //Нет пром. тактов - выходим
                      if (Tprom_len[CUR_DK]==0)
                      {
                         CUR_NEXT();
                         break;
                      }
                      // текущее время
                      const time_t timeCurrent = CT.tm_hour*3600 + CT.tm_min*60 + CT.tm_sec;// время когда в эту фазу попали
                      DWORD TimeDelta=0;
                      if(timeStartTpr<=timeCurrent)TimeDelta = timeCurrent - timeStartTpr;
                      if(TimeDelta<=Tprom_len[CUR_DK])
                        DK[CUR_DK].control.startLen = DK[CUR_DK].control.len  = Tprom_len[CUR_DK]-TimeDelta;
                        else
                        DK[CUR_DK].control.startLen = DK[CUR_DK].control.len = 0; // время вышло, сбросс
                      //
                      //DK[CUR_DK].control.len = Tprom_len[CUR_DK];
                      //
                      //в control.end - было окончание текущего состояния - основного такта
                      memcpy(&DK[CUR_DK].control.start, &DK[CUR_DK].control.end, sizeof(SYSTEMTIME));
                      // Окончание пром. тактов
                      TIME_PLUS(&DK[CUR_DK].control.start, &DK[CUR_DK].control.end, DK[CUR_DK].control.len);
                      // находимся в режиме ВПУ
                      if(DK[CUR_DK].CUR.source==VPU)
                          DK[CUR_DK].control.len = Tprom_len[CUR_DK];
                      // чистим индексы
                      for (int i=0;i < DK[CUR_DK].PROJ->Directs.countDirectCCG; i++)
                        {
                        DK[CUR_DK].control.prom_indx[i]=0;
                        DK[CUR_DK].control.prom_time[i]=0;
                        }
                      //
                      SET_PROM_STATE_LEDS();
                      DK[CUR_DK].control.STA = STA_PROM_TAKTS;
                   }
                    break;
                }
                // шаг промежуточного такта
                case STA_PROM_TAKTS:
                {
                  /*-------------------------------*/
                  clearStatusRestart(); //очищаем флаг
                  /*-------------------------------*/
                  if (TIME_PHASE_END()) // смотрим время и переходим  TIME_END()
                     {
                       // переходим
                       CUR_NEXT();
                       break;
                     }
                     //+1s
                     for(int i_n=0; i_n < DK[CUR_DK].PROJ->Directs.countDirectCCG; i_n++)
                     {

                       DK[CUR_DK].control.prom_time[i_n]++; // инкремент ++ 1 с
                       // номер пром такта
                       const int prom_indx = DK[CUR_DK].control.prom_indx[i_n];
                       // получим время в мин. пром тактов преобразуем в секунды
                       const int TimeProm = prom_takts[CUR_DK][i_n][prom_indx].time*60;
                       if ( DK[CUR_DK].control.prom_time[i_n]>=TimeProm){ // проверить при выходе
                            DK[CUR_DK].control.prom_indx[i_n]++; //следующий пром. такт
                            DK[CUR_DK].control.prom_time[i_n]=0; // нулим таймер
                            }
                     }
                     //
                     SET_PROM_STATE_LEDS();
                     //
                     break;
                }
        }
if((statControl==STA_PROM_TAKTS)||(statControl==STA_OSN_TAKT))return false; // УДЗСГ вкл.
return true; // УДЗСГ выкл.
}
/*----------------------------------------------------------------------------*/
/*
Основная автомат работы ДК
//REQUESTS(); - проверка всех запросов в структуре
//MODEL();  - логика работы ДК
//CONTROL(); - переключаем фазы
*/
/*----------------------------------------------------------------------------*/
// тактируем раз в 0.5с вызов из модуля light
unsigned short DK_MAIN(void)
{
unsigned short fire_light;
DS1390_TIME time;
//
GetTime_DS1390(&time);
//
CT.tm_hour = time.hour;
CT.tm_mday = time.date;
CT.tm_min = time.min;
CT.tm_sec = time.sec;
CT.tm_mon = time.month;
CT.tm_year = time.year;
// сброс портов
GREEN_PORT=0;
RED_PORT  =0;
YEL_PORT  =0;
// перебираем логические ДК если их больше одного
for (int i_dk=0; i_dk<dk_num; i_dk++)
  {
  CUR_DK = i_dk;// current DK
  // запросики
  REQUESTS();
  // логика работы ДК
  MODEL();
  // исполнитель - фактически переключает фазы
  bool fudzon = CONTROL();
  //
  #ifdef KURS_DK
    fire_light = Update_STATES(false,fudzon);
  #endif
  }
//включить силовые выходы
SET_OUTPUTS();
//чистим статусы ADC
Clear_UDC_Arrays();
return (fire_light);
}
/*----------------------------------------------------------------------------*/
/******************************************************************************
                          Выделены функции
*******************************************************************************/
/*----------------------------------------------------------------------------*/
void DK_ALARM_OC(void)
{
    DK[CUR_DK].REQ.req[ALARM].spec_prog = SPEC_PROG_OC;
    DK[CUR_DK].REQ.req[ALARM].work = SPEC_PROG;
    DK[CUR_DK].REQ.req[ALARM].source = ALARM;
    DK[CUR_DK].REQ.req[ALARM].presence = true;
}
void DK_ALARM_undo(void)
{
Clear_STATE(&(DK[CUR_DK].REQ.req[ALARM]));
}
void DK_Service_OS(void)
{
    DK[CUR_DK].REQ.req[SERVICE].spec_prog = SPEC_PROG_OC;
    DK[CUR_DK].REQ.req[SERVICE].work = SPEC_PROG;
    DK[CUR_DK].REQ.req[SERVICE].source = SERVICE;
    DK[CUR_DK].REQ.req[SERVICE].presence = true;
}
/*----------------------------------------------------------------------------*/
void DK_Service_YF(void)
{
    DK[CUR_DK].REQ.req[SERVICE].spec_prog = SPEC_PROG_YF;
    DK[CUR_DK].REQ.req[SERVICE].work = SPEC_PROG;
    DK[CUR_DK].REQ.req[SERVICE].source = SERVICE;
    DK[CUR_DK].REQ.req[SERVICE].presence = true;
}
/*----------------------------------------------------------------------------*/
void DK_Service_undo(void)
{
Clear_STATE(&(DK[CUR_DK].REQ.req[SERVICE]));
// установили флаги ДК ОС
for (int i_dk=0; i_dk<DK_N; i_dk++)
           DK[i_dk].OSSOFT = false;
}
/*----------------------------------------------------------------------------*/
void DK_Service_KK(void)
{
    DK[CUR_DK].REQ.req[SERVICE].spec_prog = SPEC_PROG_KK;
    DK[CUR_DK].REQ.req[SERVICE].work = SPEC_PROG;
    DK[CUR_DK].REQ.req[SERVICE].source = SERVICE;
    DK[CUR_DK].REQ.req[SERVICE].presence = true;
}
/*----------------------------------------------------------------------------*/
void DK_Service_faza(const unsigned long faz_i)
{
    DK[CUR_DK].REQ.req[SERVICE].work = SINGLE_FAZA;
    DK[CUR_DK].REQ.req[SERVICE].faza = faz_i;
    DK[CUR_DK].REQ.req[SERVICE].source = SERVICE;
    DK[CUR_DK].REQ.req[SERVICE].presence = true;
}
/*----------------------------------------------------------------------------*/
// указатели на структуру
/*----------------------------------------------------------------------------*/
// получить указатель на структуру ExpUDZ TEXPRESSIONS
const TEXPRESSIONS *retPointExpUDZ(void)
{
return &PROJ[CUR_DK].ExpUDZ;
}
// получить указатель на всю структуру
const TPROJECT *retPointPROJECT(void)
{
return &PROJ[CUR_DK];
}
/*----------------------------------------------------------------------------*/
// return current ID
BYTE retCurrenetID(void)
{
return PROJ[CUR_DK].surd.ID_DK_CUR;
}
/*----------------------------------------------------------------------------*/
// время окончание пром тактов или фазы
DWORD retTimePhase(void)
{
return DK[CUR_DK].control.len;
}
//посмотреть текщий запрос
DK_STATE retCurrentREQ(void)
{
return DK[CUR_DK].REQ.req[VPU].source;
}