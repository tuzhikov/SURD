/*----------------------------------------------------------------------------*/
// Fixed Alex Tuzhikov 2015
/*----------------------------------------------------------------------------*/
#include "takts.h"

#define COEF_SEC 60
/*----------------------------------------------------------------------------*/
TPROJECT  *project[DK_N];
TPROGRAM  PROG_NEXT;       //структура следующей программы
const TPROGRAM  *proga;
int       cur_prog[DK_N];  //текущая программа
int       next_prog[DK_N]; //следующая
prom_takts_faz_type      prom_takts[DK_N];   // пром. такты одной фазы
osn_takts_faz_type       osn_takt[DK_N];     // основные такты  фазы
DWORD                    osn_takt_time[DK_N];//длина основного такта
DWORD                    Tprom_len[DK_N];    // продолжительность пром. фазы рассчитанной
                                             // в функции Build_Takts
/*--------------- Functions---------------------------------------------------*/
void initTAKTS(TPROJECT *proj)
{
project[CUR_DK] = proj;
}
/*----------------------------------------------------------------------------*/
// возвращает ссылку программу
static TPROGRAM *Locate_Prog(const int i_prog)
{
if (i_prog!=NO_PROG){
  if (i_prog==cur_prog[CUR_DK])
    return (&project[CUR_DK]->Program);
  if (i_prog==next_prog[CUR_DK])
    return (&PROG_NEXT);
  }
//
dbg_puts("Locate_Prog error");
dbg_trace();
tn_halt();
return 0;
}
/*----------------------------------------------------------------------------*/
// получение номера шаблонной фазы с программы и фазы в рпограмме
// 2 варианта
// 1) реальная программа и cur_faz - номер в ней
// 2) нет программы cur_faz - номер шаблонной фазы (нельзя использоывать при вызове ВПУ, вызываем не существующий шаблон)
static int Get_SH_FAZ(const int c_prog,const int cur_faz)
{
int sh_faza = 0;
//
if (c_prog!=NO_PROG){
  sh_faza = Locate_Prog(c_prog)->fazas[cur_faz].NumFasa;
  }else
    sh_faza = cur_faz;
//
return(sh_faza);
}
/*----------------------------------------------------------------------------*/
// смотрит тип шаблона фазы - красный(0) или зеленый(1)
// i_faz - номер фазы в текущей программе
static int Get_Red_Green_Type_Fasa(const int c_prog,const int i_napr,const int i_faz)
{
int sh_faza = Get_SH_FAZ(c_prog, i_faz);
// номер шаблонной фазы по
if (project[CUR_DK]->Fasa[sh_faza].Direct[i_napr])
    return 1;
  else
    return 0;
}
/*----------------------------------------------------------------------------*/
// получение значений параментров пром. тактов
static int GET_P(const int c_prog,const int c_napr,const int cur_faz,const BYTE prom_param)
{
int ret_i=0;
//
if (c_prog!=NO_PROG){
  switch (prom_param)
    {
    case P_ZD1:
      ret_i = Locate_Prog(c_prog)->fazas[cur_faz].prom[c_napr].Zd1;
      break;
    case P_ZD2:
      ret_i = Locate_Prog(c_prog)->fazas[cur_faz].prom[c_napr].Zd2;
      break;
    case P_KD:
      ret_i = Locate_Prog(c_prog)->fazas[cur_faz].prom[c_napr].KD;
      break;
    }
  }
return (ret_i);
}
/*----------------------------------------------------------------------------*/
static int GET_Tosn(const int c_prog,const int cur_faz)
{
int ret_i=0;
//
if (c_prog!=NO_PROG){
  ret_i = Locate_Prog(c_prog)->fazas[cur_faz].Tosn;
  }
return (ret_i);
}
/*----------------------------------------------------------------------------*/
// вычисляем длительность Тпром для перехода между фазами
// cur_faz - порядковый номер фазы в программе
static int Calculate_Tprom_max(const int c_prog,const int n_prog,const int cur_faz,const int next_faz)
{
        int     ret_i, prom_i, prom_max, i_napr;
        int     per_faz;   //переход между фазами
        int     cur_faz_sh, next_faz_sh; // номера шаблонной фазы

        ////////////////
        cur_faz_sh = Get_SH_FAZ(c_prog, cur_faz);
        next_faz_sh = Get_SH_FAZ(n_prog, next_faz);
        //
        ret_i = 0;
        prom_max = 0;
        ////////////////////////
        // 1) смотрим Тпром по тактам программы
        ///////////// cur_prog
        // по всем направлениям
        int napr_n = project[CUR_DK]->Directs.countDirectCCG;
        for (i_napr = 0; i_napr < napr_n; i_napr ++)
        {
          //per_faz = napr_faz[i_napr][cur_faz] - napr_faz[i_napr][next_faz];
          per_faz = Get_Red_Green_Type_Fasa(c_prog,i_napr, cur_faz) -
                    Get_Red_Green_Type_Fasa(n_prog,i_napr, next_faz);
          prom_i = 0;
          // Индекс перехода
          // 1-0=1  - переход от зеленого к красному
          // 0-1=-1 - переход от красного к зеленому
          // 0-0=0  , 1-1 = 0 - нет изменения основного такта
          //////////////////
          // переход З-К  , сумма ЗМ+Ж+К
           if (per_faz == 1)
           {
              if (project[CUR_DK]->Directs.OneDirect[i_napr].Type==DIR_CCG)
              {
                    prom_i = GET_P(c_prog, i_napr, cur_faz, P_ZD2) +
                             //proga->fazas[cur_faz].prom[i_napr].Zd2 +
                       project[CUR_DK]->guard.green_yellow +
                       project[CUR_DK]->guard.yellow +
                       GET_P(c_prog, i_napr, cur_faz, P_KD);
                       //proga->fazas[cur_faz].prom[i_napr].KD ;
              }
              else
              {
                    prom_i = GET_P(c_prog, i_napr, cur_faz, P_ZD2) +
                             //proga->fazas[cur_faz].prom[i_napr].Zd2 +
                             GET_P(c_prog, i_napr, cur_faz, P_KD)+
                             project[CUR_DK]->guard.green_yellow;

              }
              //
           }//if per ==1
           /////
           // переход К-З , сумма КЖ+З
           if (per_faz == -1)
           {
              if (project[CUR_DK]->Directs.OneDirect[i_napr].Type==DIR_CCG)
              {
                    prom_i = GET_P(n_prog, i_napr, next_faz, P_ZD1) +
                             //GET_P(c_prog, i_napr, cur_faz, P_KD) +
                             //proga->fazas[cur_faz].prom[i_napr].Zd1 +
                             project[CUR_DK]->guard.redyellow;
              }
              else
              {
                    prom_i = GET_P(n_prog, i_napr, next_faz, P_ZD1);
                             //proga->fazas[cur_faz].prom[i_napr].Zd1;

              }
              //
           }//if per ==1
           /////
           // переход К-К , З-З
           if (per_faz == 0)
           {

           }//if per ==0
           /////



           /////
           if (prom_i > prom_max)
              prom_max = prom_i;

        }//for i_napr
        //////////////////////////////////
        //////////////////////////////////
        // 2) смотрим Тпром по матрице конфлииктов
        int n1,n2;
        //int prom_max_confl=0;
        prom_i=0;
        //направления 1и 2 фаз
        for (n1 = 0; n1 < napr_n; n1 ++)
        {
          if (project[CUR_DK]->Fasa[cur_faz_sh].Direct[n1]==SHABL_FAZA_DIR_GREEN)
            for (n2 = 0; n2 < napr_n; n2 ++)
            {
               if (project[CUR_DK]->Fasa[next_faz_sh].Direct[n2]==SHABL_FAZA_DIR_GREEN)
               if (project[CUR_DK]->guard.ConfMatrixFlag[n1][n2])
               {
                   //есть конфликт направлений
                   prom_i =  GET_P(c_prog, n1, cur_faz, P_ZD2) +
                             //proga->fazas[cur_faz].prom[n1].Zd2 +
                             project[CUR_DK]->guard.green_yellow +
                             GET_P(n_prog, n2, next_faz, P_ZD1) +
                             //proga->fazas[next_faz].prom[n2].Zd1 +
                             project[CUR_DK]->guard.ConfMatrix[n1][n2];
                   ///
                   if (prom_i > prom_max)
                        prom_max = prom_i;
               }
            }//for n2
        }//for n1
        /////////////////
        ret_i = prom_max;
        //
        return (ret_i);

}
/*----------------------------------------------------------------------------*/
// Вычисление расширенного конфликтного максимума=Conf+Zd1
// cur_faz - порядковый номер фазы в программе
// n1 - направление для которого смотрится расширенный конфликт
static int Calculate_Ex_Conf_Max(const int c_prog,
                                 const int n_prog,
                                 const int n1,
                                 const int cur_faz,
                                 const int next_faz)
{
int n2;
int prom_i=0;
int prom_max=0;
int cur_faz_sh, next_faz_sh; // номера шаблонной фазы

int napr_n = project[CUR_DK]->Directs.countDirectCCG;
//
cur_faz_sh = Get_SH_FAZ(c_prog,cur_faz);//proga->fazas[cur_faz].NumFasa;
next_faz_sh = Get_SH_FAZ(n_prog,next_faz);// proga->fazas[next_faz].NumFasa;
         //направления 1и 2 фаз
          if (project[CUR_DK]->Fasa[cur_faz_sh].Direct[n1]==SHABL_FAZA_DIR_GREEN)
            for (n2 = 0; n2 < napr_n; n2 ++)
            {
               if (project[CUR_DK]->Fasa[next_faz_sh].Direct[n2]==SHABL_FAZA_DIR_GREEN)
               if (project[CUR_DK]->guard.ConfMatrixFlag[n1][n2])
               {
                   //есть конфликт направлений
                   prom_i =  //GET_P(c_prog, n2, cur_faz, P_ZD1) +
                             GET_P(n_prog, n2, next_faz, P_ZD1) +

                             //proga->fazas[next_faz].prom[n2].Zd1 +
                             project[CUR_DK]->guard.ConfMatrix[n1][n2];
                   ///
                   if (prom_i > prom_max)
                        prom_max = prom_i;
               }
            }//for n2
return (prom_max);
}
/*----------------------------------------------------------------------------*/
// очистка массива тактов
static void Clear_Takts(void)
{
for (int i=0; i < MaxDirects; i++)
  {
  osn_takt[CUR_DK][i] = ALL_OFF;
  for (int iii=0; iii < TaktsN; iii++)
    {
    prom_takts[CUR_DK][i][iii].col = ALL_OFF;
    prom_takts[CUR_DK][i][iii].time = 0;
    }
  }
}
//------------------------------------------------------------------------------
static void Load_Progs(const int c_prog,const int n_prog)
{
// грузим проги
// текущую - в проект,
if (c_prog!=NO_PROG){
  if (!DK[CUR_DK].no_LOAD_PROG_PLAN)
  flash_rd(FLASH_PROGS, CUR_DK*sizeof(TPROGRAMS) + c_prog*sizeof(TPROGRAM),
      (unsigned char*)&project[CUR_DK]->Program, sizeof(TPROGRAM));
  cur_prog[CUR_DK] = c_prog;
  }
//
  if (n_prog!=NO_PROG){
    if (!DK[CUR_DK].no_LOAD_PROG_PLAN)
      flash_rd(FLASH_PROGS, CUR_DK*sizeof(TPROGRAMS) + n_prog*sizeof(TPROGRAM),
        (unsigned char*)&PROG_NEXT, sizeof(TPROGRAM));
    //
    next_prog[CUR_DK] = n_prog;
    }
}
//------------------------------------------------------------------------------
// Построение массива тактов для одной фазы :
// c_prog - текущая программа
// n_prog - программа следующей фазы
void Build_Takts(int c_prog, int n_prog, int  cur_faz,int  next_faz)
{
        int     i_napr;
        int     per_faz;               // переход между фазами
                                       // (без добавления продлевающего основного сигнала)
        int     prom_max;              // значение Тпром.макс
        int     ex_conf;               // значение расширенного конфликта на Zd1
        bool    trans_napr;            // является ли данное направление транспортным
        int     cur_faz_typ, nex_faz_typ;
        //int
        int Ytakt , Rtakt, GFtakt,RYtakt,Gtakt  ;    // значение пром.тактов в секундах
        //////////
        Clear_Takts();
        ///
        Ytakt=project[CUR_DK]->guard.yellow;
        GFtakt=project[CUR_DK]->guard.green_yellow;
        RYtakt=project[CUR_DK]->guard.redyellow;
        //
        Load_Progs(c_prog, n_prog);
        int napr_n = project[CUR_DK]->Directs.countDirectCCG;
         ///////////////////
         ////////////////////////
               prom_max = Calculate_Tprom_max(c_prog, n_prog, cur_faz, next_faz);
               Tprom_len[CUR_DK] = prom_max*COEF_SEC;
               //////
               // по всем направлениям
               for (i_napr = 0; i_napr < napr_n; i_napr ++)
               {
                    cur_faz_typ = Get_Red_Green_Type_Fasa(c_prog, i_napr, cur_faz);
                    nex_faz_typ = Get_Red_Green_Type_Fasa(n_prog, i_napr, next_faz);
                    //
                    per_faz = cur_faz_typ - nex_faz_typ;
                    //
                    if ( project[CUR_DK]->Directs.OneDirect[i_napr].Type == DIR_CCG)
                          trans_napr = true;
                        else
                          trans_napr = false;
                    //

                    /////////////////////////////////////////
                    // переход З-К  , сумма ЗМ+Ж+К
                    if (per_faz == 1)
                    {

                       osn_takt[CUR_DK][i_napr] = GREEN;  //цвет - зеленый
                       osn_takt_time[CUR_DK] = GET_Tosn(c_prog, cur_faz)*COEF_SEC;
                             //GET_P(c_prog, i_napr, cur_faz, P_ZD1)
                             //proga->fazas[cur_faz].Tosn;
                       /////
                       ex_conf = Calculate_Ex_Conf_Max(c_prog, n_prog, i_napr, cur_faz,next_faz );
                       //////////////////////////////
                       ///////////////////////////////
                       if (trans_napr)
                       if (project[CUR_DK]->guard.green_fill)
                       {
                          // заполнение пустот зеленым
                          Rtakt = ex_conf - Ytakt;
                          if (Rtakt<0) Rtakt=0;
                          //
                          Gtakt = prom_max - Rtakt- Ytakt - GFtakt;
                          if (Gtakt<0) Gtakt=0;
                          //
                          int pt_i=0;  //номер пром.такта
                          if (Gtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = GREEN;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Gtakt;
                            pt_i++;
                          }
                          ///
                          if (GFtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = GREEN_YELLOW;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = GFtakt;
                            pt_i++;
                          }
                          ///
                          if (Ytakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = YELLOW;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Ytakt;
                            pt_i++;
                          }
                          ///
                          if (Rtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = RED;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Rtakt;
                            pt_i++;
                          }
                          ///
                       }
                       /////////////
                       /////////////
                       if (trans_napr)
                       if (project[CUR_DK]->guard.green_fill==0)
                       {
                          // заполнение пустот КРАСНЫМ
                          //
                          Gtakt =  GET_P(c_prog, i_napr, cur_faz, P_ZD2);
                                   // proga->fazas[cur_faz].prom[i_napr].Zd2;
                          //
                          Rtakt = prom_max-Ytakt-GFtakt-Gtakt;
                          //if (Rtakt<0) Rtakt=0;

                          //prom_max - Rtakt- Ytakt - GFtakt;
                          //if (Gtakt<0) Gtakt=0;
                          //
                          int pt_i=0;  //номер пром.такта
                          if (Gtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = GREEN;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Gtakt;
                            pt_i++;
                          }
                          ///
                          if (GFtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = GREEN_YELLOW;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = GFtakt;
                            pt_i++;
                          }
                          ///
                          if (Ytakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = YELLOW;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Ytakt;
                            pt_i++;
                          }
                          ///
                          if (Rtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = RED;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Rtakt;
                            pt_i++;
                          }
                          ///
                       }
                       /////////////
                       /////////////
                       // НЕ транспортное направление
                       if (!trans_napr)
                       if (project[CUR_DK]->guard.green_fill)
                       {
                          // заполнение пустот зеленым
                          Rtakt = ex_conf;// - Ytakt;
                          if (Rtakt<0) Rtakt=0;
                          //
                          Gtakt = prom_max - Rtakt- GFtakt;
                          if (Gtakt<0) Gtakt=0;
                          //
                          int pt_i=0;  //номер пром.такта
                          if (Gtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = GREEN;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Gtakt;
                            pt_i++;
                          }
                          ///
                          if (GFtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = GREEN_YELLOW;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = GFtakt;
                            pt_i++;
                          }
                          ///
                          if (Rtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = RED;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Rtakt;
                            pt_i++;
                          }
                          ///
                       }
                       /////////////
                       /////////////
                       // НЕ транспортное направление
                       if (!trans_napr)
                       if (project[CUR_DK]->guard.green_fill==0)
                       {
                          // заполнение пустот КРАСНЫМ
                          //
                          Gtakt = GET_P(c_prog, i_napr, cur_faz, P_ZD2);
                                //proga->fazas[cur_faz].prom[i_napr].Zd2;
                          //
                          Rtakt = prom_max-GFtakt-Gtakt;
                          if (Rtakt<0) Rtakt=0;

                          //prom_max - Rtakt- Ytakt - GFtakt;
                          //if (Gtakt<0) Gtakt=0;
                          //
                          int pt_i=0;  //номер пром.такта
                          if (Gtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = GREEN;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Gtakt;
                            pt_i++;
                          }
                          ///
                          if (GFtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = GREEN_YELLOW;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = GFtakt;
                            pt_i++;
                          }
                          ///
                          if (Rtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = RED;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Rtakt;
                            pt_i++;
                          }
                          ///
                       }
                       /////////////
                       /////////////


                    }//if per ==1
                    //////////////////////////////////////////
                    /////////////////////////////////////////
                    // переход К-З  , сумма КЖ+З
                    if (per_faz == -1)
                    {
                         osn_takt[CUR_DK][i_napr] = RED;  //цвет - зеленый
                         osn_takt_time[CUR_DK] = GET_Tosn(c_prog,cur_faz)*COEF_SEC;
                                //proga->fazas[cur_faz].Tosn;
                         Gtakt =  GET_P(n_prog, i_napr, next_faz, P_ZD1);// +

                       ////////////
                       ////////////
                       // транспортное
                       if (trans_napr)
                       {

                                    //GET_P(c_prog, i_napr, cur_faz, P_ZD1);
                                   //proga->fazas[cur_faz].prom[i_napr].Zd1;
                          //
                          Rtakt = prom_max - Gtakt - RYtakt;
                          if (Rtakt<0) Rtakt=0;
                          //
                          int pt_i=0;  //номер пром.такта
                          if (Rtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = RED;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Rtakt;
                            pt_i++;
                          }
                          ///
                          if (RYtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = RED_YELLOW;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = RYtakt;
                            pt_i++;
                          }
                          ///
                          if (Gtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = GREEN;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Gtakt;
                            pt_i++;
                          }
                          ///
                       }
                       /////////////
                       /////////////
                       // транспортное, зеленым
                       if (!trans_napr)
                       {
                          //Gtakt = GET_P(n_prog, i_napr, next_faz, P_ZD1);
                                   //proga->fazas[cur_faz].prom[i_napr].Zd1;
                          //
                          Rtakt = prom_max - Gtakt;
                          if (Rtakt<0) Rtakt=0;
                          //
                          int pt_i=0;  //номер пром.такта
                          if (Rtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = RED;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Rtakt;
                            pt_i++;
                          }
                          ///
                          if (Gtakt)
                          {
                            prom_takts[CUR_DK][i_napr][pt_i].col = GREEN;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][pt_i].time = Gtakt;
                            pt_i++;
                          }
                          ///
                       }
                       /////////////
                       /////////////


                    }//if per ==-1
                    //////////////////////////////////////////
                    /////////////////////////////////////////
                    // Одноцветный переход З-З или  К-К
                    if (per_faz == 0)
                    {
                         if (Get_Red_Green_Type_Fasa(c_prog, i_napr, cur_faz))
                         {
                            osn_takt[CUR_DK][i_napr] = GREEN;
                            prom_takts[CUR_DK][i_napr][0].col = GREEN;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][0].time = prom_max;
                         }
                         else
                         {
                            osn_takt[CUR_DK][i_napr] = RED;
                            prom_takts[CUR_DK][i_napr][0].col = RED;  //цвет - зеленый
                            prom_takts[CUR_DK][i_napr][0].time = prom_max;
                         }
                         ////
                         osn_takt_time[CUR_DK] = GET_Tosn(c_prog,cur_faz)*COEF_SEC;
                          //proga->fazas[cur_faz].Tosn;;
                         ////



                    }//if per == 0
               }// for i_napr
}