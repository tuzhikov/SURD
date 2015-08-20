/// KURS low level

#include "kurs.h"
#include "dk.h"
#include "structures.h"
#include "takts.h"
#include "string.h"
#include "../utime.h"
#include "../memory/ds1390.h"
#include "../multicast/cmd_fn.h"
#include "../memory/memory.h"
#include "../pins.h"
//#include "types_define.h"

// ��������� ������� ������
BYTE     GREEN_PORT;
BYTE     RED_PORT;
BYTE     YEL_PORT;
// ����� ���������� ���������� �������
BYTE     GREEN_PORT_CONF;
BYTE     RED_PORT_CONF;
BYTE     YEL_PORT_CONF;
// ���������� ���������
BYTE     GREEN_PORT_ERR;
BYTE     RED_PORT_ERR;
// ������ ���������� ��� UDZG
typedef enum{
  ALLOFF           = 0x00, // ��� ���������
  TURN             = 0x01, // ������� �������(�)
  DIRECT           = 0x02, // ������� ����� (�)
  FOND             = 0x04, // ��� ����� (�)
  ENDSTAT
}STATES_UDZCG; // ��������� ���
static STATES_LIGHT staLightCCG[MaxDirects]; // ������ ������� ��������� ����������� ���
static BYTE statLigthUDZCG[MaxDirects]; //������ ������� ��������� �����
//----------------------------------------------------------------------------//
// functions descriptions
//----------------------------------------------------------------------------//
//
void setConflict(unsigned char color, unsigned char group)
{
if (group) group--; // ������ � ����
      else return;  // �� ������ �����
//
switch (color)
    {
    case MUL_RED:    RED_PORT_CONF = RED_PORT_CONF | (1<<group);return;      //RED
    case MUL_YELLOW: YEL_PORT_CONF = YEL_PORT_CONF | (1<<group);return;      //YEL
    case MUL_GREEN:  GREEN_PORT_CONF = GREEN_PORT_CONF | (1<<group); return; //GREEN
    }
}
//----------------------------------------------------------------------------//
static void Collect_Conflicts(void)
{
    unsigned char   col, gr, con;
    //
    for (int in=0; in< PROJ[CUR_DK].Directs.countDirectCCG; in++)
    {
        col = PROJ[CUR_DK].Directs.OneDirect[in].out.green.color;
        gr = PROJ[CUR_DK].Directs.OneDirect[in].out.green.group;
        con = PROJ[CUR_DK].Directs.OneDirect[in].out.green.control;
        if (!con)
           setConflict(col,gr);
        //
        col = PROJ[CUR_DK].Directs.OneDirect[in].out.yel.color;
        gr = PROJ[CUR_DK].Directs.OneDirect[in].out.yel.group;
        con = PROJ[CUR_DK].Directs.OneDirect[in].out.yel.control;
        if (!con)
           setConflict(col,gr);
        //
        col = PROJ[CUR_DK].Directs.OneDirect[in].out.red.color;
        gr = PROJ[CUR_DK].Directs.OneDirect[in].out.red.group;
        con = PROJ[CUR_DK].Directs.OneDirect[in].out.red.control;
        if (!con)
           setConflict(col,gr);
    }
}
//----------------------------------------------------------------------------//
void Prepare_KURS_Structures()
{
Collect_Conflicts();
}
//----------------------------------------------------------------------------//
void Clear_LED()
{
GREEN_PORT=0;
RED_PORT = 0;
YEL_PORT = 0;
}
//----------------------------------------------------------------------------//
// ������������� ����� ������
// group = 0..<=MaxMULBlocks. 0 - �� ��������.
// gr_col = 0..<MaxChannels, collor
// stat - ��������� ���\����
void Set_LED(int group, int gr_col, BOOL stat)
{
if (group==0)return; // �� ����� �����
//��������� ��� �������
group--; // ����� � ����
switch (gr_col)
  {
  case MUL_RED://RED
    {
    if (stat) RED_PORT |= (1<<group);
         else RED_PORT &= ~(1<<group);
    break;
    }
  case MUL_YELLOW://YEL
    {
    if (stat) YEL_PORT |= (1<<group);
         else YEL_PORT &= ~(1<<group);
    break;
    }
  case MUL_GREEN://GREEN
    {
    if (stat) GREEN_PORT |= (1<<group);
         else GREEN_PORT &= ~(1<<group);
    break;
    }
  }// end switch
}
//------------------------------------------------------------------------------
// ��������� ������� � ����������� �� ����
static void Filter_Signal(TONEDIRECT  *diR, bool red_s, bool yel_s, bool green_s)
{
    /*const DIR_TYPE   d_type = diR->Type;
    const int red_g = diR->out.red.group;
    const int red_gc = diR->out.red.color;
    const int yel_g = diR->out.yel.group;
    const int yel_gc = diR->out.yel.color;
    const int green_g = diR->out.green.group;
    const int green_gc = diR->out.green.color;*/
    DIR_TYPE   d_type = diR->Type;
    int red_g, red_gc;
    int yel_g, yel_gc;
    int green_g,  green_gc;
   //////////////
    red_g = diR->out.red.group; red_gc = diR->out.red.color;
    yel_g = diR->out.yel.group; yel_gc = diR->out.yel.color;
    green_g = diR->out.green.group; green_gc = diR->out.green.color;

    //
    switch (d_type)
    {
      case DIR_CCG:case DIR_UDZCG:
      {
         Set_LED(red_g, red_gc, red_s);
         Set_LED(yel_g, yel_gc, yel_s);
         Set_LED(green_g, green_gc, green_s);
         break;
      }
    }
}
//----------------------------------------------------------------------------//
void setDirectCCG(const int i_napr,TONEDIRECT  *diR,const bool flash)
{
staLightCCG[i_napr] =  DK[CUR_DK].control.napr[i_napr]; // ������� ��������� ���
// ���������� �����
switch(staLightCCG[i_napr])
  {
  case ALL_OFF:       Filter_Signal(diR,false,false,false);break;
  case GREEN:         Filter_Signal(diR,false,false, true);break;
  case GREEN_YELLOW:  Filter_Signal(diR,false, true, true);break;
  case RED:           Filter_Signal(diR, true,false,false);break;
  case RED_YELLOW:    Filter_Signal(diR, true, true,false);break;
  case YELLOW:        Filter_Signal(diR,false, true,false);break;
  case YELLOW_FLASH:  Filter_Signal(diR,false,flash,false);break;
  default:            Filter_Signal(diR,false,false,false);break;
  }
}
// ���� �������� � ���������� ���������---------------------------------------//
// return ��������� ���������
bool checkOneOperandCCG(const TYPE_COLOR maskDir,const STATES_LIGHT curDir,bool inv)
{
bool result=false;
//�������� ��������� ����������� ��� � ������
if((maskDir==MUL_RED)&&
   ((curDir==RED)||(curDir==RED_YELLOW)))result=true;
if((maskDir==MUL_YELLOW)&&
   ((curDir==YELLOW)||(curDir==YELLOW_FLASH)||
    (curDir==GREEN_YELLOW)||(curDir==RED_YELLOW)))result=true;
if((maskDir==MUL_GREEN)&&
   ((curDir==GREEN)||(curDir==GREEN_YELLOW)))result=true;
//���������� ��������
if(inv)result=!result;
return result;
}
//
bool checkOneOperandUDZCG(const TYPE_COLOR maskDir,const BYTE statDir,bool inv)
{
bool result=false;
// ��������
if((maskDir==MUL_RED)&&
   ((statDir==TURN)||(statDir==(TURN|FOND))||
    (statDir==(TURN|DIRECT))||(statDir==(TURN|DIRECT|FOND))))result=true;

if((maskDir==MUL_YELLOW)&&
   ((statDir==DIRECT)||(statDir==(DIRECT|FOND))||
   (statDir==(TURN|DIRECT))||(statDir==(TURN|DIRECT|FOND))))result=true;

if((maskDir==MUL_GREEN)&&
   ((statDir==FOND)||(statDir==(TURN|FOND))||
    (statDir==(DIRECT|FOND))||(statDir==(TURN|DIRECT|FOND))))result=true;
// ��������
if(inv)result=!result;
return result;
}
// �������� ��� �������� �� �������� �� �������-------------------------------//
static bool checkTWO(bool opr1,bool opr2,LOGIC_OP oper)
{
switch (oper)
  {
  case AND:     return opr1&opr2;
  case OR:      return opr1|opr2;
  case OR_NO:   return ~(opr1|opr2);
  case AND_NO:  return ~(opr1&opr2);
  }
return false;
}
// �������� ��������� ���������-----------------------------------------------//
static bool checkExpressions(bool opr1,bool opr2,bool opr3,
                      LOGIC_OP  oper1,LOGIC_OP oper2)
{
bool res = checkTWO(opr1,opr2,oper1);
res = checkTWO(res,opr3,oper2);
return res;
}
// ������ ������ ��������� ����� ���------------------------------------------//
static STATES_UDZCG retStatUDZ(const TYPE_COLOR col)
{
switch(col)
  {
  case MUL_RED:   return TURN;
  case MUL_YELLOW:return DIRECT;
  case MUL_GREEN: return FOND;
  }
return ALLOFF;
}
// ��������� ����������� ��������� �����--------------------------------------//
STATES_UDZCG retLogicalExpressions(const TONE_EXPRESSION *logic)
{
STATES_UDZCG statUDZCG = ALLOFF;
bool resultExp[MAX_OPERAND] = {false,false,false};

for(int i=0;i<MAX_OPERAND;i++)
  {
  const TYPE_COLOR maskDir  = logic->operand[i].TypeCol;  // ���� ����������� �������
  const BYTE       nDir     = logic->operand[i].NumberDir;// ����� ����������� ��������
  const DIR_TYPE   nDirType = logic->operand[i].TypeDir;  // ��� ����������� �������
  bool inv = logic->operand[i].NotAcv;// �������� ��������
  // ������� ��������� ��������
  if(maskDir==MUL_COLOR_COUNT){ // log 1
    if(!inv)resultExp[i] = true; // inversion 1
    continue;
    }
  if(maskDir==MUL_COLOR_COUNT+1){// log 0
    if(inv)resultExp[i] = true;  // inversion 0
    continue;
    }
  // ��������� ������� � ����������� ���
  if(nDirType==DIR_CCG){
    const STATES_LIGHT statDir = staLightCCG[nDir]; // ������� ��������� �� ��� �� ���� ������������
    if(checkOneOperandCCG(maskDir,statDir,inv))
        resultExp[i] = true;
    continue;
    }
  // ��������� ������� � ����������� �����
  if(nDirType==DIR_UDZCG){
    const BYTE statDir = statLigthUDZCG[nDir];
    if(checkOneOperandUDZCG(maskDir,statDir,inv))
        resultExp[i] = true;
    continue;
    }
  }
// ��������� ���������
bool result = checkExpressions(resultExp[0],resultExp[1],resultExp[2],
                               logic->logic[0],logic->logic[1]);
//������� �������� ����������
if(logic->result.NotAcv){
  // �������� ��� ���������
  if(!result){
    statUDZCG = retStatUDZ(logic->result.TypeCol);
    }
  }else{
  if(result){
    statUDZCG = retStatUDZ(logic->result.TypeCol);
    }
  }
return statUDZCG;
}
// ���������� ����������� ����� �� ��������� ������� �� �������---------------//
/*
idUDZCG - ����� ��������� �����
*diR - ��������� �� ������������� � �������
fOff - ���� ���������� �����
*/
void setDirectUDZCG(const int  idUDZCG,TONEDIRECT  *diR,const bool fOff)
{
const TEXPRESSIONS *pDateUDZCG = retPointExpUDZ();
STATES_UDZCG statUDZCG = ALLOFF;
// �������� ���� ����� UDZCG � �������
if(!fOff){
  for(int i=0; i<pDateUDZCG->Count;i++)
    {
    const BYTE nDirUDZ = pDateUDZCG->Expression[i].result.NumberDir;
    if(idUDZCG == nDirUDZ){ // ���� ����������� ����������
      statUDZCG |= retLogicalExpressions(&pDateUDZCG->Expression[i]);
      }
    }
  }else{
  statUDZCG = ALLOFF;
  }
statLigthUDZCG[idUDZCG] = (BYTE)statUDZCG; //��������� ��������� �������� �����
// ���������� PORT �����
switch(statUDZCG)
  {
  case ALLOFF:            Filter_Signal(diR,false,false,false);break;
  case TURN:              Filter_Signal(diR, true,false,false);break;
  case (TURN|FOND):       Filter_Signal(diR, true,false, true);break;
  case DIRECT:            Filter_Signal(diR,false, true,false);break;
  case (DIRECT|FOND):     Filter_Signal(diR,false, true, true);break;
  case FOND:              Filter_Signal(diR,false,false, true);break;
  case (TURN|DIRECT):     Filter_Signal(diR, true, true,false);break;
  case (TURN|DIRECT|FOND):Filter_Signal(diR, true, true, true);break;
  }
}
//----------------------------------------------------------------------------//
void Write_DX(BYTE dat)
{
if(dat & (1<<0))pin_on(OPIN_D0); else pin_off(OPIN_D0);
if(dat & (1<<1))pin_on(OPIN_D1); else pin_off(OPIN_D1);
if(dat & (1<<2))pin_on(OPIN_D2); else pin_off(OPIN_D2);
if(dat & (1<<3))pin_on(OPIN_D3); else pin_off(OPIN_D3);
if(dat & (1<<4))pin_on(OPIN_D4); else pin_off(OPIN_D4);
if(dat & (1<<5))pin_on(OPIN_D5); else pin_off(OPIN_D5);
if(dat & (1<<6))pin_on(OPIN_D6); else pin_off(OPIN_D6);
if(dat & (1<<7))pin_on(OPIN_D7); else pin_off(OPIN_D7);
}
//----------------------------------------------------------------------------//
// ����� � ��������
void SET_OUTPUTS(void)
{
//������� �������
pin_off(OPIN_R_LOAD);
pin_off(OPIN_G_LOAD);
pin_off(OPIN_Y_LOAD);
// write red
Write_DX(RED_PORT);
pin_on(OPIN_R_LOAD );
pin_off(OPIN_R_LOAD );
// write yellow
Write_DX(YEL_PORT);
pin_on(OPIN_Y_LOAD );
pin_off(OPIN_Y_LOAD );
// write green
Write_DX(GREEN_PORT);
pin_on(OPIN_G_LOAD );
pin_off(OPIN_G_LOAD );
}
//----------------------------------------------------------------------------//
void Light_TVP_Wait(int p_faz)
{
            // ���������� ����
            //int p_faz = DK[CUR_DK].TVP.cur.prog_faza;
            int sh_faz; //���������
            // ���� ����������� ����������
            for (int ina=0; ina< PROJ[CUR_DK].Directs.countDirectCCG; ina++)
            {
                if (PROJ[CUR_DK].Directs.OneDirect[ina].Type==DIR_UDZCG)
                {
                   sh_faz=PROJ[CUR_DK].Program.fazas[p_faz].NumFasa;
                   if (PROJ[CUR_DK].Fasa[sh_faz].Direct[ina])
                   {
                       //�, ��� ���� �����������
                       Set_LED(PROJ[CUR_DK].Directs.OneDirect[ina].out.yel.group,
                              PROJ[CUR_DK].Directs.OneDirect[ina].out.yel.color, true);
                   }
                }
            }
}
//----------------------------------------------------------------------------//
// ��������
void Update_STATES_KURS(const bool blink,const bool fUdzOFF)
{
const int maxAllDir = DK[CUR_DK].PROJ->AmountDirects; // ��� �����������
TONEDIRECT  *diR;

// ��������� �����������
for (int i_napr=0; i_napr<maxAllDir; i_napr++)
  {
  diR =  &DK[CUR_DK].PROJ->Directs.OneDirect[i_napr]; // �������� ����������� �� �������
  // ��� ����������� ���
  if(diR->Type==DIR_CCG){
    setDirectCCG(i_napr,diR,blink); // set output
    }
  if(diR->Type==DIR_UDZCG){
    setDirectUDZCG(i_napr,diR,fUdzOFF);
    }
  }// end for
// �����
if (DK[CUR_DK].TVP.enabled)
  if ((DK[CUR_DK].CUR.source==PLAN) || (DK[CUR_DK].CUR.source==TVP))
          for (int tvp_i=0; tvp_i< MaxTVP; tvp_i++)
          {
          if(DK[CUR_DK].tvps[tvp_i].pres)
              Light_TVP_Wait(DK[CUR_DK].tvp_faza[tvp_i]);
          }
}
//-----------------------------------------------------------------------------