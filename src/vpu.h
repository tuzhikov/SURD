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
#define MAX_LED                     11
#define ADR_VPU                     1

typedef enum _Type_ANS_{ansNoAnsw=0x00,ansOk=0x01,ansErr=0x02}Type_ANS;
typedef enum _Type_CMD_{cmdLed=0x01,cmdButtun=0x02,cmdLedSetup=0xF}Type_CMD;
typedef enum _Type_LED_{ledOff=0x00,ledBlink1 = 0x01,ledBlink2 = 0x02,
                        ledOn = 0x03}TYPE_LED_SATUS;
typedef enum _Type_BUTT_{bOff,bDown,bUp,bOn,bEnd}Type_BUTT;
typedef enum _Type_BUT_{ButPhase1,ButPhase2,ButPhase3,ButPhase4,ButPhase5,ButPhase6,
                        ButPhase7,ButPhase8,ButTlOff,ButYllBlink,ButManual}Type_BUTTON;
typedef enum _Type_STATUS_VPU_{tlNo,tlPhase1,tlPhase2,tlPhase3,tlPhase4,
                               tlPhase5,tlPhase6,tlPhase7,tlPhase8,
                               tlOff,tlYellBlink,tlManual,tlEnd}Type_STATUS_VPU;
//typedef enum _MASK_BYTE_{0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01}MASK_BYTE;
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
//////////
// data exchange beetween master and slave
typedef struct{
  BYTE                  statusNEt; //  0- OFF, 1- OK
  BYTE                  idDk;     //     
  BYTE                  vpuOn;  // 0- OFF, 1 - ON
  Type_STATUS_VPU       vpu; 
} MASTER_SLAVE_VPU;
typedef struct{
  MASTER_SLAVE_VPU   m_to_s; //master to slave status
  MASTER_SLAVE_VPU   s_to_m; //slave to master VPU status
} VPU_EXCHANGE;
//////////////
extern VPU_EXCHANGE  vpu_exch;

/*external functions*/
void vpu_init();   //��� ������� ����������� �� ������ main.c � ������� static void startup() TN_kernel
void uart1_int_handler(); //��� ������� ����������� �� ������ tn_user.c � ������� void hw_uart1_int_handler()���������� ���������� �� uart
const TVPU *retDateVPU(void);
// ��� �������
BYTE retRequestsVPU(void);
// ��������� ��������� ���
void retTextStatusVPU(char *pStr,const Type_STATUS_VPU status);

#endif // __VPU_H__
