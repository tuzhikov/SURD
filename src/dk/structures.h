//
#include "../types_define.h"

#ifndef structuresH
#define structuresH
//------------
#define KURS_DK

 #pragma pack (1)

 //
 #define DK_N              1    // ���-�� ��
 #define MAX_DK            32   // ���-�� �� � ����
 #define WeeksInYear       54
 #define MaxWeeksPlans     16  //����� ��������� ������
 #define MaxDaysPlans      16  //����� �������� ������
 #define MaxCalendar       32  //����� ��������� ��� �����

 #define MaxTVP            1   //������������ ����� ���
 #define MaxDirects        70 //������������ ����� �����������
 #define MaxProgram        1  //������������ ����� ��������
 #define MaxProgFase       32   //������������ ����� ��� � ���������
 #define MaxFase           32  //������������ ����� ���
  //
 #define MaxMULBlocks      8  //������������ ����� ������
 #define MaxLightSlots     12  //����� ������ ������� � ������� �����
 #define MaxLightPlans     12  //����� ������ �������

 #define MaxTramvays       1    //����� ���������� ����������
 ////  {���� 512, 12 ���� �� ������}
 #define MaxTextLen        512    //������������ ���������� �����������
 #define MaxTextLen_Ex     128
 /*taks*/
 #define TaktsN            4        // ����� ������ � ����. ����
 #define TaktsALL          5        // ����� ������ � ����
 // ����-���������
 #define NO_PROG           -1
 #define SPEC_PROG_NO      0   //�� ����-���������
 #define SPEC_PROG_YF      1   //��
 #define SPEC_PROG_OC      2   //OC
 #define SPEC_PROG_KK      3   //KK
 // ��-�� ���
 #define FAZA_PROP_FIKS    0   //������������� ����
 #define FAZA_PROP_COMB    1   //���������������
 #define FAZA_PROP_TVP     2   //�������� ���

/*----------------------------------------------------------------------------*/
// ���������
// ������������
#define P_ZD1     0
#define P_ZD2     1
#define P_KD      2
// ��������� �����������
typedef enum __STATES_LIGHT
{
	ALL_OFF       = 0x00,   // ��� ���������
	GREEN         = 0x01,	// ������
	GREEN_YELLOW  = 0x02,	// ������-������
	RED           = 0x03,   // �������
	RED_YELLOW    = 0x04,   // �������-�����
	YELLOW        = 0x05,   // Ƹ����
	YELLOW_FLASH  = 0x06,	// Ƹ���� ��������
	RESERVED
} STATES_LIGHT;
/*----------------------------------------------------------------------------*/
typedef struct  _TPROMTAKT{
  BYTE Zd1;     // ����� ���������� �����
  BYTE Zd2;
  BYTE KD;
  }TPROMTAKT;
//������ ����
#define SHABL_FAZA_DIR_RED     0
#define SHABL_FAZA_DIR_GREEN   1
/*----------------------------------------------------------------------------*/
typedef struct _TSHABLONFASA{
  BYTE  Direct[MaxDirects];  //���� ��������� ����������� � ����
                              //0x01-��������  0x0 -���������  (������� ����)
  }TSHABLONFASA;
// ��������� ���
typedef struct _TVPSOURCE{
   BYTE        pres; //0 - ��� ���, 1,2 - ������\������ ������
  }TVPSOURCE;
// ��������� ���� � ���������
typedef struct _TFASA{
   BYTE      FasaProperty;        //  �������� ����
   WORD      Tosn;                //  �������� �����
   WORD      Tmin;                //  ����������� �����
   BYTE      NumFasa;             //  ����� ��������� ����
   TPROMTAKT prom[MaxDirects];    // ���� ����� ��� ������� ����������
   //TVPSOURCE tvps[MaxTVP];
  }TFASA;
 // FasaProperty
 //������������� -  0;
 //���������������, 0x01
 //��� - 0x02
// ��������� ��������� �������� �� ������� �����
typedef struct _TCALENDARTIME{
  BYTE   NumProgramWorks;  //  � ���������   1-32
  BYTE   SpecProg;         // ���� � ����� ����-��������
  DWORD  BeginTimeWorks;   //  ����� ������ ������ � �������� �� 00:00:00
  }TCALENDARTIME;
/*----------------------------------------------------------------------------*/
typedef struct{
  BYTE            AmountCalendTime;          //���-�� ��������
  TCALENDARTIME   CalendTime[ MaxCalendar ]; // ��������� �������� ���������� ���������
  }TCALENDAR;
/*----------------------------------------------------------------------------*/
typedef struct{
   BYTE Calendar[7 ]; // ������(�������) �������� ������ �� ������ DaysPlans
                      // 0 - ��, 6 - ��
  }TWEEKCALENDAR;
/*----------------------------------------------------------------------------*/
typedef struct{
   BYTE     YearCalendar[WeeksInYear]; // ������(�������) ��������� ������ �� ������ WeeksCalendars
}TYEARCALENDAR;
/*----------------------------------------------------------------------------*/
typedef struct{
   BYTE   AmountFasa; 		   //���������� ��� � ���������
   TFASA  fazas[MaxProgFase];
}TPROGRAM;
/*----------------------------------------------------------------------------*/
typedef struct{
    TPROGRAM             Program[ MaxProgram ]; // ����� ��������
    DWORD                ProgramsSize;	        // ������ ��������
    unsigned long        CRC32;
} TPROGRAMS;
/*----------------------------------------------------------------------------*/
//�����������
//��� ����������� ���=0 �����=1
//������ - ���������� ������
//MaxMULBlocks= ����� ���������� �����
/*----------------------------------------------------------------------------*/
typedef enum {
	MUL_RED       = 0x00,   // �������
        MUL_YELLOW    = 0x01,	// ������
	MUL_GREEN     = 0x02,	// ������
	MUL_COLOR_COUNT
  }TYPE_COLOR; //
/*----------------------------------------------------------------------------*/
// ���� �����
// ����� - 0-�������, 1 - �������, 2 - ������
typedef struct{
  BYTE          napr;  // 0 - ��� ��������, 1 - ������ �����������, ....
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
//�����������
//��� ����������� ������=0 ��������=1  �������=2 �����������=3
 typedef enum {
	DIR_CCG      = 0x00,   // ����������� CCG
	DIR_UDZCG    = 0x01,   // ����������� ���
        DIR_COUNT
}DIR_TYPE;
/*----------------------------------------------------------------------------*/
// �������� �� ���� ������ ��� ������, ��������� ������� �� ����������
 typedef struct _TNAPROUT{
  BYTE       group;   //����� ������� ������  0 - ��� ��������, 1 - ������ ������
  TYPE_COLOR color;   //����� ������ ������ 0,1,2
  BYTE       control; //���� ���������� ��������
  BYTE       NumberDk;//����� ��
  }TNAPROUT;
/*----------------------------------------------------------------------------*/
/*���������� ���������� ��� ������� �����������*/
typedef struct _TNAPROUTALL{
  TNAPROUT  red;   // ������� ������ �����������
  TNAPROUT  yel;
  TNAPROUT  green;
  }TNAPROUTALL;
/*----------------------------------------------------------------------------*/
/*���� �����������*/
typedef struct _TONEDIRECT{
  DIR_TYPE    Type;
  TNAPROUTALL out;
  }TONEDIRECT;
/*----------------------------------------------------------------------------*/
/*��������� ��� �����������*/
typedef struct _TDIRECTS{
  BYTE  countDirectCCG;// ������� ����������� ���
  TONEDIRECT    OneDirect[MaxDirects]; // MaxDirects=32
  }TDIRECTS;
/*----------------------------------------------------------------------------*
��������� ����� ������������ ������ �����
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
// �������� ���� ����� �� ������� ���
typedef struct _TEXPRESSIONS_ {
        BYTE Count;       // ������� �������� �����
        TONE_EXPRESSION Expression[MAX_UDZCG];
}TEXPRESSIONS;
/*----------------------------------------------------------------------------*/
typedef struct _TGUARD{
        // >0 - ����� ����� �������� ����������� �����������
        BYTE  ConfMatrix[MaxDirects][MaxDirects]; //
        BYTE  ConfMatrixFlag[MaxDirects][MaxDirects]; //  �����
        BYTE  red_min;   // �����������
        BYTE  yellow;
        BYTE  yellow_min;   // ����
        BYTE  redyellow;
        BYTE  green_yellow;
        BYTE  green_min;
        WORD  faza_max;
        BYTE  green_fill;        // ���� ���������� ����. ������ 0-�������, 1 - �������
        BYTE  type_cycle;        // ��� ����. ����� ��� �����������.
        BYTE  restart_interval;  // �������� �����������
        BYTE  restarts;          // ���-�� ������� �����������
        BYTE  time_clear;        // ����� ������� ������
        BYTE  kk_len;            // �� ������������
        BYTE  gpsON;             // ��������� �������� GPS
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
  DWORD Pass; //������
  WORD TimeDelay;
  BYTE Count;
  }TSURD;
/*----------------------------------------------------------------------------*/
typedef struct _TCOMMENT_INNER2{
  BYTE     rings;        // ���-�� ����� � �������
  BYTE     synhro_mode;  //���� ����������� ������
  WORD     syhhro_add;   // ���������� �����
  }TCOMMENT_INNER2;
/*----------------------------------------------------------------------------*/
typedef struct _TCOMMENT_INNER{
       WORD     preambule; //0xABBA
       TCOMMENT_INNER2   in;
   }TCOMMENT_INNER;
/*----------------------------------------------------------------------------*/
typedef struct _TCOMMENT_EX{
     unsigned char        comment[MaxTextLen_Ex];
     TCOMMENT_INNER       inner;  //��������� ���������
     unsigned char        comm_add[MaxTextLen-sizeof(TCOMMENT_INNER) - MaxTextLen_Ex];
    }TCOMMENT_EX;
/*----------------------------------------------------------------------------*/
// ��������� ������� ��� ��
typedef struct _TPROJECT{
   WORD                 ProjectSize;	                // ������ �������
   BYTE                 AmountDirects;	                // ������������ ����������� ���� � ���
   TDIRECTS             Directs;	                // �����������
   TEXPRESSIONS         ExpUDZ;                         // ����� ��� �����
   BYTE                 AmountMuls;                     // ���-�� ��
   TSHABLONFASA         Fasa[MaxFase] ;                 // ���� - �������
   TCALENDAR            DaysPlans[MaxDaysPlans];        // ����� ������� ������
   TWEEKCALENDAR        WeeksPlans[MaxWeeksPlans];      // ����� ��������� ������
   TYEARCALENDAR        Year;                           // ������� ����
   TPROGRAM             Program;                        // ����� ��������
   TGUARD               guard;                          // ��������� ������������
   BYTE                 maxDK;
   TCOMMENT_EX          comments;
   TJORNAL              jornal;                         // ��������� ��������������
   TSURD                surd;                           // ��������� ����
   unsigned long        CRC32_programs;                 //�� ��������� ��������
   unsigned long        CRC32;
  }TPROJECT;
/*----------------------------------------------------------------------------*/
#endif






