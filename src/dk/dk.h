//---------------------------------------------------------------------------

#ifndef dkH
#define dkH
//---------------------------------------------------------------------------
#include "structures.h"
#include "../tnkernel/tn.h"
#include "../light/light.h"
#include "../utime.h"

typedef struct tm SYSTEMTIME;

//#define SYSTEMTIME      (struct tm)
// коды ошибок
#define RET_OK          0

// лаг времени системы -
// запрет делать операции
#define TIME_LAG        5
//
typedef enum __GO_STATE
{
        STA_INIT,
        STA_GET_REQ, //получение запроса
        STA_FIRST,
        STA_WORK,
        STA_CUR_NEXT,   //переход между состояниями
        STA_SET_NEXT,
        STA_OSN_TAKT,
        STA_PROM_TAKTS,
        STA_SPEC_PROG,
        STA_PLAN_CALC_NEXT,
        STA_SEE_REQUEST,
        STA_KK,
        STA_EXIT
} GO_STATE;
////
//Состояния ДК
//2) Выполняем фазу программы
//1) Выполняем спец. программу - ЖМ, ОС, КК
//0) Выполняем просто фазу (режим ВПУ)

typedef enum __WORK_STATE
{
        SPEC_PROG    = 0x00,   //
	SINGLE_FAZA  = 0x01,	//
        PROG_FAZA    = 0x02,    //

}WORK_STATE;
//
typedef enum __DK_STATE
{
        ALARM         = 0x00,   // запрос от АЛАРМ
	TUMBLER       = 0x01,	// тумблеры
        SERVICE       = 0x02,   // запросы через связь-компьютер
	VPU           = 0x03,	// ВПУ
	TVP           = 0x04,   // ТВП
	PLAN          = 0x05,   // ПЛАН
        DK_STATE_COUNT
}DK_STATE;
// состояние
typedef struct __STATE
{
  BOOL             presence;   // присутствие состояния -
                               // состояния(запроса) может не быть,
  BOOL             update ;    //флаг - поступил другой запрос этого уровня
                               // ПР. тумблер ОС сменился на ЖМ
  BOOL             set;        // флаг-установлено
                               // состояние определено и не может быть изменено
                               // устанавливается при начале Пром. тактов
                               // А также обратная связь - сообщить PLAN, что
                               // состояние установлено.
  BOOL             connect;    // была вызвана успешно SET_NEXT_STATE
                               //
  /// состояния
  WORK_STATE       work;       // что выполнять (шабл. фазу, спец. прогу ..)
  BYTE             faza;       // номер абсолютной фазы
  BYTE             prog;       // номер программы -1,0,1..
  BYTE             spec_prog;  // флаг спец. программы 0,1,2
  BYTE             prog_faza;  // номер фазы в программе
  ///
  WORD             len;        //длительность, если >0 - учитывается
                               // для фаз это длительность основного такта
  //
  DK_STATE         source;     //источник состояния
  //DK_STATE         dk_state; // глобальное состояние ДК при этом состоянии  ???
  //
  //struct tm        start;
  //SYSTEMTIME       start;    //время начала состояния
  //SYSTEMTIME       end;      //время окончания
  //
} STATE;
//
typedef struct __MODULE_STATE
{
        GO_STATE    STA;  //состояние
        STATE       cur;  // то что устанавливаем в DK[CUR_DK].NEXT
        STATE       next; // будущее по плану
        BOOL        enabled;// разрешено
} MODULE_STATE;


/////////////
// запросная структура - последние поступившие
// формальная, говорит только о наличии запроса и что делать
// какую фазу.программу устновить
typedef struct __REQUEST
{
  DK_STATE  prior_req; //текущий наиболее приоритетный
                       //STATE  ALARM;
                       //STATE  TUMBLER;
                       //STATE  SERVICE;
                       //STATE  VPU;
                       //STATE  TVP;
                       //STATE  PLAN;
   STATE    req[DK_STATE_COUNT];
 }REQUEST;
// таймеры текущего состояния
typedef struct __CONTROLLER
{
        GO_STATE        STA;  //состояние
        //
        DWORD           Tproga;    // длина текущей программы(цикла)
        DWORD           BeginTimeWorks; // время начала текущего тайм-слота
        DWORD           Tosn;    // длина основного такта
        WORD            Tprom;   // длина пром тактов
        //
        STATES_LIGHT    napr[MaxDirects]; //текущие состояния направлений
        BYTE            prom_indx[MaxDirects];  //индекс в массиве prom_takts
        // таймера
        BYTE            prom_time[MaxDirects]; //таймеры
        //
        DWORD           len; //таймер текущего состояния
        //
        SYSTEMTIME      start; //время начала состояния
        SYSTEMTIME      end;   //время окончания текущего состояния
        SYSTEMTIME      endPhase; //время окончания текущего состояния фазы
        //
        int             prom_timer; //таймер
        //
        BOOL            all_answer; // все ответили
} CONTROLLER;
// Статус SURD
typedef struct{
  DWORD sendStatusDK;   // переменная для передачи в сеть, копия tmpStatusNet
  DWORD sendStatusSURD; // переменная для передачи в сеть, копия tmpStatusSURD
  DWORD tmpStatusNet;// мах ДК 32 собирает инф. мастер о slave
  DWORD tmpStatusSURD; // статус СУРД собирает "мастер"
  BYTE  flagNetworkStatus; // сетевой статус общий после опроса ДК
  BYTE  flagStatusSURD;    // статус СУРД, общий после опроса ДК
  BYTE  flagLocalStatusSURD; //
}STATUS_SURD;
// Структура - ДК
typedef struct
{
       BYTE             work;    //ДК работает
       TPROJECT         *PROJ;   //проект
       BYTE             tumblerOS;   // флаг нажатого тумблера OS
       BYTE             tumblerAUTO; // флаг нажатого тумблера AUTO
       BYTE             OSHARD;  // установлен тумблер ОС
       BYTE             OSSOFT;  // установлен программно ОС
       BYTE             YF; // нажат YF
       BYTE             AUTO; // сбросс IP по умолчанию
       BYTE             StatusDK; // общее стостяние ДК (включем если GPS no,конфликты зеленого, красного)
       BYTE             test;         //тест-режим
       BYTE             test_init;    // флаг для очистки выводов
       BYTE             flash;     //начато обновление проекта-программа
       BYTE             synhro_mode; //синхронный режим работы -
       DWORD            cur_slot_start; // начало текущего слота
       // это синхронизация по времени начала программы
       // если объявлено - в PROG_PLAN не будет грузиться из FLASH
       BYTE             no_LOAD_PROG_PLAN;
       BYTE             no_FAULT; //глобально не реагировать на авариии
       //
       BYTE             proj_valid;
       BYTE             progs_valid;
       //
       BOOL             LOG_VALID;      // работоспособность журнала
       // флаг записи в журнале о неисправности датчика
       BYTE             U_sens_fault_log[8];
       BYTE             U_sens_power_fault_log;
       // история состояний
       STATE            CUR;     // текущее состояние
       STATE            OLD;     // предыдущее
       STATE            NEXT;    //cледующее
       //
       REQUEST          REQ;      //запросы
       CONTROLLER       control;  // времянка, нижний уровень
       // структуры состояний
       MODULE_STATE       PLAN;
       MODULE_STATE       TVP;
       MODULE_STATE       VPU;
       MODULE_STATE       SERVICE;
       // какие есть вообще запросы
       TVPSOURCE        tvps[MaxTVP];        //   запросы ТВП
       // флаг необходимости обработки данного запроса ТВП
       BYTE      tvp_en[MaxTVP];
       //привязка запроса ТВП к  фазе программы
       BYTE      tvp_faza[MaxTVP];
       // Статус SURD
       STATUS_SURD StatusSurd;
}__DK;
/*----------------------------------------------------------------------------*/
//
/*----------------------------------------------------------------------------*/
extern  TPROJECT  PROJ[DK_N];
extern  BYTE  CUR_DK;   // важная переменная по свему проекту
extern __DK DK[DK_N];
extern BYTE   dk_num; //
extern SYSTEMTIME   CT; // control time
/*----------------------------------------------------------------------------*/
void Init_DK(void);
unsigned short DK_MAIN(void);
unsigned short Update_STATES(const bool flash,const bool fudzon);
int Seconds_Between(SYSTEMTIME *tt, SYSTEMTIME *tl);
void TIME_PLUS(SYSTEMTIME *tt, SYSTEMTIME *tplus, DWORD sec_plus);
////////////////////////////////////////////////////////////////////////////////////
// Выделены функции
////////////////////////////////////////////////////////////////////////////////////
void DK_ALARM_OC(void);
void DK_ALARM_undo(void);
void DK_Service_OS(void);
void DK_Service_YF(void);
void DK_Service_KK(void);
void DK_Service_undo(void);
void DK_Service_faza(unsigned long faz_i);
// указатели на данные текущего ДК
const TEXPRESSIONS *retPointExpUDZ(void);
// указатели на данные текущего ДК
const TPROJECT *retPointPROJECT(void);
// current ID DK
BYTE retCurrenetID(void);
// информации о текущем времени и фазе
DWORD retTimePhase(void);
DK_STATE retCurrentREQ(void);


#endif
