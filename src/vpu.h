/*****************************************************************************
*
* � 2015 Cyber-SB. Written by Alex Tuzhikov.
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

typedef enum _Type_ANS_{ansNoAnsw=0x01,ansOk=0x02,ansErr=0x04}Type_ANS;
typedef enum _Type_CMD_{cmdLed=0x01,cmdButtun=0x02,cmdLedSetup=0xF}Type_CMD;
typedef enum _Type_LED_{ledOff=0x00,ledBlink1 = 0x01,ledBlink2 = 0x02,
                        ledOn = 0x03}TYPE_LED_SATUS;
typedef enum _Type_BUTT_{bOff,bDown,bUp,bOn,bEnd}Type_BUTT;
typedef enum _Type_BUT_{ButPhase1=0,ButPhase2=1,ButPhase3=2,ButPhase4=3,ButPhase5=4,ButPhase6=5,
                        ButPhase7=6,ButPhase8=7,ButTlOff=8,ButAUTO=9,ButManual=10}Type_BUTTON;
typedef enum _Type_STATUS_VPU_{tlPhase1=0,tlPhase2=1,tlPhase3=2,tlPhase4=3,
                               tlPhase5=4,tlPhase6=5,tlPhase7=6,tlPhase8=7,
                               tlOS=8,tlAUTO=9,tlManual=10,tlEnd=11,tlNo=12,tlError=15}Type_STATUS_VPU;
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
  int           bOnIndx;  //������ ������� ������
  BOOL          RY;       //���� ������� ����������
  BOOL          myRY;     //������ ���������� - �� �����
}TVPU;
// ��������� ��� �������� ��������� ����������� �� ����
typedef union _VPU_LED_SEND{
  BYTE LED1:2;
  BYTE LED2:2;
  BYTE LED3:2;
  BYTE LED4:2;
  BYTE LED5:2;
  BYTE LED6:2;
  BYTE LED7:2;
  BYTE LED8:2;
  WORD LED_STATUS;
}TVPU_LED_SEND;

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
//////////
// data exchange beetween master and slave
typedef struct{
  // ������ ���� //  0- OFF, 1- OK
  BYTE                  statusNet;
  ////  DK number
  BYTE                  idDk;
  // ��� �������� ��� ������ ��. 0- OFF, 1 - ON
  BYTE                  vpuOn;
  // ��������� ���
  Type_STATUS_VPU       vpu;
} MASTER_SLAVE_VPU;
typedef struct{
  MASTER_SLAVE_VPU   m_to_s; //���������� �� ������� ��� slave
  MASTER_SLAVE_VPU   s_to_m; //������������ �� ��� slave
} VPU_EXCHANGE;
//////////////
extern VPU_EXCHANGE  vpu_exch;
extern int           cur_vpu; //����� �������� ���
extern VPU_EXCHANGE  vpu_exchN[VPU_COUNT];
extern TVPU          dataVpuN[VPU_COUNT];
/*external functions*/
void vpu_init();   //��� ������� ����������� �� ������ main.c � ������� static void startup() TN_kernel
void uart1_int_handler(); //��� ������� ����������� �� ������ tn_user.c � ������� void hw_uart1_int_handler()���������� ���������� �� uart
//const TVPU *retDateVPU(void);
void updateCurrentDatePhase(const BYTE stNet,const BYTE idDk,const BYTE vpuOn,
                            const BYTE vpuST); // ��������� VPU
BYTE retRequestsVPU(void);// ��� �������
Type_STATUS_VPU retStateVPU(void);
BYTE retOnVPU(void);
void setStatusLed(const WORD stLed);
WORD retStatusLed(void);
// ��������� ��������� ���
void retPhaseToText(char *pStr,const BYTE phase);
WORD retTextToPhase(char *pStr);

#endif // __VPU_H__
