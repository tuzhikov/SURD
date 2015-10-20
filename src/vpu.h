/*****************************************************************************
*
* © 2015 Cyber-SB. Written by Alex Tuzhikov.
*
******************************************************************************/

#ifndef __VPU_H__
#define __VPU_H__

#include "tn_user.h"
#include "stellaris.h"
#include "debug/debug.h"
#include "types_define.h"
/* type VPU*/
#define MAX_BUTTON                  11
#define MAX_BUTTON_PHASE            8
#define MAX_LED                     11
#define ADR_VPU                     1
#define MAX_VPU_FAZE                8
#define VPU_COUNT                   1
#define NO_EVENTS                   0xFF

typedef enum _Type_ANS_{ansNull=0x00,ansNoAnsw=0x01,ansOk=0x02,ansErr=0x04}Type_ANS;
typedef enum _Type_CMD_{cmdLed=0x01,cmdButtun=0x02,cmdLedSetup=0xF}Type_CMD;
typedef enum _Type_LED_{ledOff=0x00,ledBlink1 = 0x01,ledBlink2 = 0x02,
                        ledOn = 0x03}TYPE_LED_SATUS;
typedef enum _Type_BUTT_{bOff,bDown,bUp,bOn,bEnd}Type_BUTT;
typedef enum _Type_BUT_{ButPhase1=0,ButPhase2=1,ButPhase3=2,ButPhase4=3,ButPhase5=4,ButPhase6=5,
                        ButPhase7=6,ButPhase8=7,ButTlOff=8,ButAUTO=9,ButManual=10,END_BUTTON}Type_BUTTON;
/*typedef enum _Type_STATUS_VPU_{tlPhase1=0,tlPhase2=1,tlPhase3=2,tlPhase4=3,
                               tlPhase5=4,tlPhase6=5,tlPhase7=6,tlPhase8=7,
                               tlOS=8,tlAUTO=9,tlManual=10,tlEnd=11,tlNo=12,tlError=15}Type_STATUS_VPU;*/
typedef enum _Type_STATUS_VPU_{
  tlPhase1 = 0,tlPhase2 = 1,tlPhase3 = 2,tlPhase4 = 3,tlPhase5 = 4,tlPhase6 = 5,tlPhase7 = 6,tlPhase8 = 7,
  tlPhase9 = 8,tlPhase10= 9,tlPhase11=10,tlPhase12=11,tlPhase13=12,tlPhase14=13,tlPhase15=14,tlPhase16=15,
  tlPhase17=16,tlPhase18=17,tlPhase19=18,tlPhase20=19,tlPhase21=20,tlPhase22=21,tlPhase23=22,tlPhase24=23,
  tlPhase25=24,tlPhase26=25,tlPhase27=26,tlPhase28=27,tlPhase29=28,tlPhase30=29,tlPhase31=30,tlPhase32=31,
  tlOS=32,tlAUTO=33,tlManual=34,tlEnd=35,tlNo=36,tlError=37}Type_STATUS_VPU;

#pragma pack(push)  /* push current alignment to stack */
#pragma pack(1)
typedef struct _VPU_BUTTON_{
  Type_BUTT On;
}TVPU_BUTTON;
typedef struct _VPU_LED_{
  TYPE_LED_SATUS On;
}TVPU_LED;
typedef struct _VPU_{
  BYTE address;
  Type_STATUS_VPU satus;
  TVPU_BUTTON   rButton[MAX_BUTTON];
  TVPU_LED      rLed[MAX_LED];
  TVPU_LED      led[MAX_LED];
  int           bOnIndx;        //индекс нажатой кнопки
  BYTE          nextPhase;      //фаза следующая
  BYTE          stepbt;         // тест
  BOOL          RY;             //Флаг ручного управления
  BOOL          myRY;           //ручное управление - мы рулим
}TVPU;
#pragma pack(pop)
extern TVPU dataVpu;

/*Type of request message without data*/
#pragma pack(push)  /* push current alignment to stack */
#pragma pack(1)
typedef struct _MODBUS_REQUEST_{
  BYTE add;
  BYTE cmd;
  INT_TO_TWO_CHAR dataStart;
  INT_TO_TWO_CHAR dataLength;
}TMOBUS_REQUEST;
#pragma pack(pop)
/*additive to the request*/
typedef struct _MODBUS_ADD_REQUEST_{
  BYTE Length;
  BYTE byte_one;
  BYTE byte_two;
  BYTE byte_three;
}MODBUS_ADD_REQUEST;
/*Type of answer message*/
typedef struct _MODBUS_ANSWER_{
  BYTE add;
  BYTE cmd;
  BYTE dataLenght;
  BYTE *data;
}TMOBUS_ANSWER;
typedef struct _VPU_COMMAND_{
  Type_CMD cmd;            // command
  U8 dataStartHi;
  U8 dataStartLow;
  U8 dataLengthHi;
  U8 dataLengthLow;
  U8 (*setFunc)(void *p,U8 len);  // A pointer to the handling function
  U8 FlagEnd;
}VPU_COMMAND;
// data exchange beetween master and slave
typedef struct{
  // Статус сети //  0- OFF, 1- OK
  BYTE                  statusNet;
  // DK number
  BYTE                  idDk;
  // ВПУ включено или запрос РУ. 0- OFF, 1 - ON
  BYTE                  vpuOn;
  // Сотсояние ВПУ
  Type_STATUS_VPU       vpu;
} MASTER_SLAVE_VPU;
typedef struct{
  MASTER_SLAVE_VPU   m_to_s; //Управление от мастера для slave
  MASTER_SLAVE_VPU   s_to_m; //Сигнализация от ВПУ slave
} VPU_EXCHANGE;
//
extern VPU_EXCHANGE  vpu_exch;
/*external functions*/
void vpu_init();   //эта функция вызываеться из модуля main.c в функции static void startup() TN_kernel
void uart1_int_handler(); //эта функция вызываеться из модуля tn_user.c в функции void hw_uart1_int_handler()обработчит прерываний от uart
void updateCurrentDatePhase(const BYTE stNet,const BYTE idDk,const BYTE vpuOn,
                            const BYTE vpuST); // состояние VPU
void ReturnToWorkPlan(void);  // вернуться в режим ПЛАН
BYTE retRequestsVPU(void);    // ВПУ запросы
Type_STATUS_VPU retStateVPU(void);
void setStatusLed(const WORD stLed);
BYTE retOnVPU(void);
WORD retStatusLed(void);
// текcтовое состояние ВПУ
void retPhaseToText(char *pStr,const BYTE leng,const BYTE phase);
WORD retTextToPhase(char *pStr);

#endif // __VPU_H__
