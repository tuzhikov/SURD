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
// ���� ������
#define RET_OK          0

// ��� ������� ������� -
// ������ ������ ��������
#define TIME_LAG        5
//
typedef enum __GO_STATE
{
        STA_INIT,
        STA_GET_REQ, //��������� �������
        STA_FIRST,
        STA_WORK,
        STA_CUR_NEXT,   //������� ����� �����������
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
//��������� ��
//2) ��������� ���� ���������
//1) ��������� ����. ��������� - ��, ��, ��
//0) ��������� ������ ���� (����� ���)

typedef enum __WORK_STATE
{
        SPEC_PROG    = 0x00,   //
	SINGLE_FAZA  = 0x01,	//
        PROG_FAZA    = 0x02,    //

}WORK_STATE;
//
typedef enum __DK_STATE
{
        ALARM         = 0x00,   // ������ �� �����
	TUMBLER       = 0x01,	// ��������
        SERVICE       = 0x02,   // ������� ����� �����-���������
	VPU           = 0x03,	// ���
	TVP           = 0x04,   // ���
	PLAN          = 0x05,   // ����
        DK_STATE_COUNT
}DK_STATE;
// ���������
typedef struct __STATE
{
  BOOL             presence;   // ����������� ��������� -
                               // ���������(�������) ����� �� ����,
  BOOL             update ;    //���� - �������� ������ ������ ����� ������
                               // ��. ������� �� �������� �� ��
  BOOL             set;        // ����-�����������
                               // ��������� ���������� � �� ����� ���� ��������
                               // ��������������� ��� ������ ����. ������
                               // � ����� �������� ����� - �������� PLAN, ���
                               // ��������� �����������.
  BOOL             connect;    // ���� ������� ������� SET_NEXT_STATE
                               //
  /// ���������
  WORK_STATE       work;       // ��� ��������� (����. ����, ����. ����� ..)
  BYTE             faza;       // ����� ���������� ����
  BYTE             prog;       // ����� ��������� -1,0,1..
  BYTE             spec_prog;  // ���� ����. ��������� 0,1,2
  BYTE             prog_faza;  // ����� ���� � ���������
  ///
  WORD             len;        //������������, ���� >0 - �����������
                               // ��� ��� ��� ������������ ��������� �����
  //
  DK_STATE         source;     //�������� ���������
  //DK_STATE         dk_state; // ���������� ��������� �� ��� ���� ���������  ???
  //
  //struct tm        start;
  //SYSTEMTIME       start;    //����� ������ ���������
  //SYSTEMTIME       end;      //����� ���������
  //
} STATE;
//
typedef struct __MODULE_STATE
{
        GO_STATE    STA;  //���������
        STATE       cur;  // �� ��� ������������� � DK[CUR_DK].NEXT
        STATE       next; // ������� �� �����
        BOOL        enabled;// ���������
} MODULE_STATE;


/////////////
// ��������� ��������� - ��������� �����������
// ����������, ������� ������ � ������� ������� � ��� ������
// ����� ����.��������� ���������
typedef struct __REQUEST
{
  DK_STATE  prior_req; //������� �������� ������������
                       //STATE  ALARM;
                       //STATE  TUMBLER;
                       //STATE  SERVICE;
                       //STATE  VPU;
                       //STATE  TVP;
                       //STATE  PLAN;
   STATE    req[DK_STATE_COUNT];
 }REQUEST;
// ������� �������� ���������
typedef struct __CONTROLLER
{
        GO_STATE        STA;  //���������
        //
        DWORD           Tproga;    // ����� ������� ���������(�����)
        DWORD           BeginTimeWorks; // ����� ������ �������� ����-�����
        DWORD           Tosn;    // ����� ��������� �����
        WORD            Tprom;   // ����� ���� ������
        //
        STATES_LIGHT    napr[MaxDirects]; //������� ��������� �����������
        BYTE            prom_indx[MaxDirects];  //������ � ������� prom_takts
        // �������
        BYTE            prom_time[MaxDirects]; //�������
        //
        DWORD           len; //������ �������� ���������
        //
        SYSTEMTIME      start; //����� ������ ���������
        SYSTEMTIME      end;   //����� ��������� �������� ���������
        SYSTEMTIME      endPhase; //����� ��������� �������� ��������� ����
        //
        int             prom_timer; //������
        //
        BOOL            all_answer; // ��� ��������
} CONTROLLER;
// ������ SURD
typedef struct{
  DWORD sendStatusDK;   // ���������� ��� �������� � ����, ����� tmpStatusNet
  DWORD sendStatusSURD; // ���������� ��� �������� � ����, ����� tmpStatusSURD
  DWORD tmpStatusNet;// ��� �� 32 �������� ���. ������ � slave
  DWORD tmpStatusSURD; // ������ ���� �������� "������"
  BYTE  flagNetworkStatus; // ������� ������ ����� ����� ������ ��
  BYTE  flagStatusSURD;    // ������ ����, ����� ����� ������ ��
  BYTE  flagLocalStatusSURD; //
}STATUS_SURD;
// ��������� - ��
typedef struct
{
       BYTE             work;    //�� ��������
       TPROJECT         *PROJ;   //������
       BYTE             tumblerOS;   // ���� �������� �������� OS
       BYTE             tumblerAUTO; // ���� �������� �������� AUTO
       BYTE             OSHARD;  // ���������� ������� ��
       BYTE             OSSOFT;  // ���������� ���������� ��
       BYTE             YF; // ����� YF
       BYTE             AUTO; // ������ IP �� ���������
       BYTE             StatusDK; // ����� ��������� �� (������� ���� GPS no,��������� ��������, ��������)
       BYTE             test;         //����-�����
       BYTE             test_init;    // ���� ��� ������� �������
       BYTE             flash;     //������ ���������� �������-���������
       BYTE             synhro_mode; //���������� ����� ������ -
       DWORD            cur_slot_start; // ������ �������� �����
       // ��� ������������� �� ������� ������ ���������
       // ���� ��������� - � PROG_PLAN �� ����� ��������� �� FLASH
       BYTE             no_LOAD_PROG_PLAN;
       BYTE             no_FAULT; //��������� �� ����������� �� �������
       //
       BYTE             proj_valid;
       BYTE             progs_valid;
       //
       BOOL             LOG_VALID;      // ����������������� �������
       // ���� ������ � ������� � ������������� �������
       BYTE             U_sens_fault_log[8];
       BYTE             U_sens_power_fault_log;
       // ������� ���������
       STATE            CUR;     // ������� ���������
       STATE            OLD;     // ����������
       STATE            NEXT;    //c��������
       //
       REQUEST          REQ;      //�������
       CONTROLLER       control;  // ��������, ������ �������
       // ��������� ���������
       MODULE_STATE       PLAN;
       MODULE_STATE       TVP;
       MODULE_STATE       VPU;
       MODULE_STATE       SERVICE;
       // ����� ���� ������ �������
       TVPSOURCE        tvps[MaxTVP];        //   ������� ���
       // ���� ������������� ��������� ������� ������� ���
       BYTE      tvp_en[MaxTVP];
       //�������� ������� ��� �  ���� ���������
       BYTE      tvp_faza[MaxTVP];
       // ������ SURD
       STATUS_SURD StatusSurd;
}__DK;
/*----------------------------------------------------------------------------*/
//
/*----------------------------------------------------------------------------*/
extern  TPROJECT  PROJ[DK_N];
extern  BYTE  CUR_DK;   // ������ ���������� �� ����� �������
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
// �������� �������
////////////////////////////////////////////////////////////////////////////////////
void DK_ALARM_OC(void);
void DK_ALARM_undo(void);
void DK_Service_OS(void);
void DK_Service_YF(void);
void DK_Service_KK(void);
void DK_Service_undo(void);
void DK_Service_faza(unsigned long faz_i);
// ��������� �� ������ �������� ��
const TEXPRESSIONS *retPointExpUDZ(void);
// ��������� �� ������ �������� ��
const TPROJECT *retPointPROJECT(void);
// current ID DK
BYTE retCurrenetID(void);
// ���������� � ������� ������� � ����
DWORD retTimePhase(void);
DK_STATE retCurrentREQ(void);


#endif
