/*----------------------------------------------------------------------------*/
#ifndef taktsH
#define taktsH
/*----------------------------------------------------------------------------*/
#include "structures.h"
#include "../light/light.h"
#include "../memory/memory.h"
#include "../debug/debug.h"
#include "../tnkernel/tn.h"
#include "dk.h"

#ifndef  __cplusplus
#define bool  unsigned char
#define true  1
#define false 0
#endif
/*-------- types--------------------------------------------------------------*/
typedef struct __TAKT_PROP{
  STATES_LIGHT  col;
  BYTE          time;
  }TAKT_PROP;
typedef  TAKT_PROP     prom_takts_faz_type[MaxDirects][TaktsN];
typedef  STATES_LIGHT  osn_takts_faz_type[MaxDirects];
/*--------external function --------------------------------------------------*/
void initTAKTS(TPROJECT *proj);
void Build_Takts(int c_prog, int n_prog, int  cur_faz,int  next_faz);
/*--------external variables--------------------------------------------------*/
extern int   cur_prog[DK_N];                 //������� ���������
extern prom_takts_faz_type prom_takts[DK_N]; // ����. ����� ����� ����
extern osn_takts_faz_type  osn_takt[DK_N];   // �������� �����  ����
extern int Tprom_len[DK_N];
extern int osn_takt_time[DK_N];              //����� ��������� �����
extern TPROGRAM PROG_NEXT;                   //��������� ��������� ���������
#endif
