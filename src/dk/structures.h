//
#include "../types_define.h"

#ifndef structuresH
#define structuresH
//------------
#define KURS_DK

 #pragma pack (1)

 //
 #define DK_N              1    // кол-во ДК
 #define MAX_DK            32   // кол-во ДК в СУРД
 #define WeeksInYear       54
 #define MaxWeeksPlans     16  //Число недельных планов
 #define MaxDaysPlans      16  //Число суточных планов
 #define MaxCalendar       32  //Число разбиений для суток

 #define MaxTVP            1   //Максимальное число ТВП
 #define MaxDirects        70 //Максимальное число направлений
 #define MaxProgram        1  //Максимальное число программ
 #define MaxProgFase       32   //Максимальное число фаз в программа
 #define MaxFase           32  //Максимальное число фаз
  //
 #define MaxMULBlocks      8  //Максимальное число блоков
 #define MaxLightSlots     12  //Число слотов яркости в дневном плане
 #define MaxLightPlans     12  //Число планов яркости

 #define MaxTramvays       1    //число трамвайных светофоров
 ////  {Было 512, 12 ушло на аддоны}
 #define MaxTextLen        512    //длительность текстового комментария
 #define MaxTextLen_Ex     128
 /*taks*/
 #define TaktsN            4        // число тактов в пром. фазе
 #define TaktsALL          5        // число тактов в фазе
 // спец-программы
 #define NO_PROG           -1
 #define SPEC_PROG_NO      0   //не спец-программа
 #define SPEC_PROG_YF      1   //ЖМ
 #define SPEC_PROG_OC      2   //OC
 #define SPEC_PROG_KK      3   //KK
 // св-ва фаз
 #define FAZA_PROP_FIKS    0   //фиксированная фаза
 #define FAZA_PROP_COMB    1   //комбинированная
 #define FAZA_PROP_TVP     2   //вызывная ТВП

/*----------------------------------------------------------------------------*/
// ПромТакты
// наименование
#define P_ZD1     0
#define P_ZD2     1
#define P_KD      2
// состояния направлений
typedef enum __STATES_LIGHT
{
	ALL_OFF       = 0x00,   // Все выключены
	GREEN         = 0x01,	// Зелёный
	GREEN_YELLOW  = 0x02,	// Зелёный-желтый
	RED           = 0x03,   // Красный
	RED_YELLOW    = 0x04,   // Красный-жёлтый
	YELLOW        = 0x05,   // Жёлтый
	YELLOW_FLASH  = 0x06,	// Жёлтый мигающий
	RESERVED
} STATES_LIGHT;
/*----------------------------------------------------------------------------*/
typedef struct  _TPROMTAKT{
  BYTE Zd1;     // Время выполнения такта
  BYTE Zd2;
  BYTE KD;
  }TPROMTAKT;
//шаблон фазы
#define SHABL_FAZA_DIR_RED     0
#define SHABL_FAZA_DIR_GREEN   1
/*----------------------------------------------------------------------------*/
typedef struct _TSHABLONFASA{
  BYTE  Direct[MaxDirects];  //Флаг активного направления в фазе
                              //0x01-активное  0x0 -пассивное  (красный цвет)
  }TSHABLONFASA;
// источники ТВП
typedef struct _TVPSOURCE{
   BYTE        pres; //0 - нет ТВП, 1,2 - первая\вторая кнопка
  }TVPSOURCE;
// Структура фазы в программе
typedef struct _TFASA{
   BYTE      FasaProperty;        //  свойства фазы
   WORD      Tosn;                //  основное время
   WORD      Tmin;                //  минимальное время
   BYTE      NumFasa;             //  номер шаблонной фазы
   TPROMTAKT prom[MaxDirects];    // пром такты для каждого напрвления
   //TVPSOURCE tvps[MaxTVP];
  }TFASA;
 // FasaProperty
 //Фиксированная -  0;
 //Комбинированная, 0x01
 //ТВП - 0x02
// Структура раскладки программ по времени суток
typedef struct _TCALENDARTIME{
  BYTE   NumProgramWorks;  //  № программы   1-32
  BYTE   SpecProg;         // флаг и номер спец-программ
  DWORD  BeginTimeWorks;   //  Время начала работы в секундах от 00:00:00
  }TCALENDARTIME;
/*----------------------------------------------------------------------------*/
typedef struct{
  BYTE            AmountCalendTime;          //кол-во отрезков
  TCALENDARTIME   CalendTime[ MaxCalendar ]; // Временная диапазон выполнения программы
  }TCALENDAR;
/*----------------------------------------------------------------------------*/
typedef struct{
   BYTE Calendar[7 ]; // номера(индексы) суточных планов из списка DaysPlans
                      // 0 - Вс, 6 - Сб
  }TWEEKCALENDAR;
/*----------------------------------------------------------------------------*/
typedef struct{
   BYTE     YearCalendar[WeeksInYear]; // номера(индексы) недельных планов из списка WeeksCalendars
}TYEARCALENDAR;
/*----------------------------------------------------------------------------*/
typedef struct{
   BYTE   AmountFasa; 		   //Количество фаз в программе
   TFASA  fazas[MaxProgFase];
}TPROGRAM;
/*----------------------------------------------------------------------------*/
typedef struct{
    TPROGRAM             Program[ MaxProgram ]; // набор программ
    DWORD                ProgramsSize;	        // Размер программ
    unsigned long        CRC32;
} TPROGRAMS;
/*----------------------------------------------------------------------------*/
//Направления
//Тип направления ССГ=0 УДЗСГ=1
//Модули - Сигнальные группы
//MaxMULBlocks= Число чигнальных групп
/*----------------------------------------------------------------------------*/
typedef enum {
	MUL_RED       = 0x00,   // Красный
        MUL_YELLOW    = 0x01,	// желтый
	MUL_GREEN     = 0x02,	// Зелёный
	MUL_COLOR_COUNT
  }TYPE_COLOR; //
/*----------------------------------------------------------------------------*/
// один выход
// цвета - 0-красный, 1 - зеленый, 2 - желтый
typedef struct{
  BYTE          napr;  // 0 - нет привязки, 1 - первое направление, ....
  TYPE_COLOR    color;
  }TONECHANNEL;
/*----------------------------------------------------------------------------*/
typedef  struct _TONEMUL{
  TONECHANNEL        red;
  TONECHANNEL        yel;
  TONECHANNEL        green;
  }TONEMUL;
/*----------------------------------------------------------------------------*/
typedef struct _TMULS{
  TONEMUL        OneMUL[MaxMULBlocks];
  }TMULS;
/*----------------------------------------------------------------------------*/
//Направления
//Тип направления Трансп=0 Пешеходн=1  Стрелка=2 Реверсивное=3
 typedef enum {
	DIR_CCG      = 0x00,   // направление CCG
	DIR_UDZCG    = 0x01,   // направление УДЗ
        DIR_COUNT
}DIR_TYPE;
/*----------------------------------------------------------------------------*/
// привязка ко всем портам для проета, подругому сделать не получилось
 typedef struct _TNAPROUT{
  BYTE       group;   //номер силовой группы  0 - нет привязки, 1 - первая группа
  TYPE_COLOR color;   //номер выхода группы 0,1,2
  BYTE       control; //флаг отключения контроля
  BYTE       NumberDk;//номер Дк
  }TNAPROUT;
/*----------------------------------------------------------------------------*/
/*логические напрвления для каждого контроллера*/
typedef struct _TNAPROUTALL{
  TNAPROUT  red;   // красный сигнал направления
  TNAPROUT  yel;
  TNAPROUT  green;
  }TNAPROUTALL;
/*----------------------------------------------------------------------------*/
/*одно направление*/
typedef struct _TONEDIRECT{
  DIR_TYPE    Type;
  TNAPROUTALL out;
  }TONEDIRECT;
/*----------------------------------------------------------------------------*/
/*описываем все направления*/
typedef struct _TDIRECTS{
  BYTE  countDirectCCG;// счетчик направлений ССГ
  TONEDIRECT    OneDirect[MaxDirects]; // MaxDirects=32
  }TDIRECTS;
/*----------------------------------------------------------------------------*
Описываем маску формирования выхода УДЗСГ
*----------------------------------------------------------------------------*/
#define MAX_OPERATION 2
#define MAX_OPERAND   3
#define MAX_UDZCG     64
typedef enum{
        AND,OR,AND_NO,OR_NO,LONE,LNULL,END_LOGIC_OP
}LOGIC_OP;

typedef struct _TOperand_ {
        BYTE        NotAcv;
        BYTE        NumberDir;
        TYPE_COLOR  TypeCol;
        DIR_TYPE    TypeDir;
}TOperand;
typedef struct _TONEEXPRESSION_{
        LOGIC_OP logic[MAX_OPERATION];
        TOperand operand[MAX_OPERAND];
        TOperand result;
}TONE_EXPRESSION;
// описание всех масок по выходам УДЗ
typedef struct _TEXPRESSIONS_ {
        BYTE Count;       // счетчик активных масок
        TONE_EXPRESSION Expression[MAX_UDZCG];
}TEXPRESSIONS;
/*----------------------------------------------------------------------------*/
typedef struct _TGUARD{
        // >0 - время между зелеными конфликтных направлений
        BYTE  ConfMatrix[MaxDirects][MaxDirects]; //
        BYTE  ConfMatrixFlag[MaxDirects][MaxDirects]; //  флаги
        BYTE  red_min;   // минимальный
        BYTE  yellow;
        BYTE  yellow_min;   // тест
        BYTE  redyellow;
        BYTE  green_yellow;
        BYTE  green_min;
        WORD  faza_max;
        BYTE  green_fill;        // флаг заполнения пром. тактов 0-красным, 1 - зеленым
        BYTE  type_cycle;        // тип пром. такта для отображения.
        BYTE  restart_interval;  // интервал перезапуска
        BYTE  restarts;          // кол-во попыток перезапуска
        BYTE  time_clear;        // время очистки аварий
        BYTE  kk_len;            // КК длительнсоть
        BYTE  gpsON;             // включение проверки GPS
  }TGUARD;
/*----------------------------------------------------------------------------*/
typedef struct _TJORNAL{
  BYTE  power_on;
  BYTE  power_off;
  BYTE  inputs;
  signed char  gmt;
  BYTE  faz;
  BYTE  alarm;
  }TJORNAL;
/*----------------------------------------------------------------------------*/
#define MAX_BYTE_IP  16
#define MAX_BYTE_MAC 18

typedef struct{
  BYTE  ID_DK_CUR;// current ID dk
  unsigned char  ip[MAX_DK][MAX_BYTE_IP];   // ip project
  unsigned char  mac[MAX_DK][MAX_BYTE_MAC]; // mac project
  WORD  PORT;
  WORD  TIMEOUT;
  DWORD Pass; //пароль
  WORD TimeDelay;
  BYTE Count;
  }TSURD;
/*----------------------------------------------------------------------------*/
typedef struct _TCOMMENT_INNER2{
  BYTE     rings;        // кол-во колец в проекте
  BYTE     synhro_mode;  //флаг синхронного режима
  WORD     syhhro_add;   // добавочное время
  }TCOMMENT_INNER2;
/*----------------------------------------------------------------------------*/
typedef struct _TCOMMENT_INNER{
       WORD     preambule; //0xABBA
       TCOMMENT_INNER2   in;
   }TCOMMENT_INNER;
/*----------------------------------------------------------------------------*/
typedef struct _TCOMMENT_EX{
     unsigned char        comment[MaxTextLen_Ex];
     TCOMMENT_INNER       inner;  //вложенная структура
     unsigned char        comm_add[MaxTextLen-sizeof(TCOMMENT_INNER) - MaxTextLen_Ex];
    }TCOMMENT_EX;
/*----------------------------------------------------------------------------*/
// Структура проекта для ДК
typedef struct _TPROJECT{
   WORD                 ProjectSize;	                // Размер проекта
   BYTE                 AmountDirects;	                // используемых направлений всех с УДЗ
   TDIRECTS             Directs;	                // Направления
   TEXPRESSIONS         ExpUDZ;                         // Маски для УДЗСГ
   BYTE                 AmountMuls;                     // кол-во СГ
   TSHABLONFASA         Fasa[MaxFase] ;                 // Фазы - шаблоны
   TCALENDAR            DaysPlans[MaxDaysPlans];        // набор дневных планов
   TWEEKCALENDAR        WeeksPlans[MaxWeeksPlans];      // набор недельных планов
   TYEARCALENDAR        Year;                           // годовой план
   TPROGRAM             Program;                        // набор программ
   TGUARD               guard;                          // параметры безопасности
   BYTE                 maxDK;
   TCOMMENT_EX          comments;
   TJORNAL              jornal;                         // настройки журналирования
   TSURD                surd;                           // настройки СУРД
   unsigned long        CRC32_programs;                 //от структуры программ
   unsigned long        CRC32;
  }TPROJECT;
/*----------------------------------------------------------------------------*/
#endif






