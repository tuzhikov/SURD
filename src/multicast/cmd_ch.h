/*****************************************************************************
*
* � 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __CMD_CH_H__
#define __CMD_CH_H__

#include "../lwip/lwiplib.h"
#include "../types_define.h"

#define CMD_DQUE_SZ     32  // max cmd's fifo size
#define CMD_NFO_SZ      32  // max cmd's count
#define MAX_ARGV_SZ     25  // cmd's argument vector size (in one command)
#define CMD_FL_ACT      0x0001
#define MAX_QURY_SURD   3
#define MAX_VIR_VPU     32
//step automate
//typedef enum {NUL,ONE,TWO,THREE,FOR,FIVE,END}STEP_NET;
//type command
typedef enum {ONE_DK,SET_PHASE,SET_STATUS,GET_STATUS,END_CMD}TYPE_CMD;
//
struct  cmd_raw
{
    struct udp_pcb* upcb;
    struct pbuf*    buf_p;
    struct ip_addr* raddr;
    u16_t           rport;
};
// ��������� ��� ������
struct cmd_nfo
{
    struct cmd_nfo* next;
    unsigned int    cmd_fl;
    char*           cmd_txt;
    char*           cmd_help;
    void            (*cmd_func)(struct cmd_raw* cmd_p, int argc, char** argv);
};
// ��������� ��� ��� ����  ������� ���������
typedef struct _NET_VPU_{
  BYTE id;
  BYTE vpuOn;
  BYTE phase;
  WORD stLED;
}NET_VPU;
typedef struct _VIR_VPU{
 NET_VPU vpu[MAX_VIR_VPU];
 BYTE active; // ���������� ���
}VIR_VPU;
// ������� ������ �� UDP ����������
typedef struct _POL_SLIPS{
  BYTE glOk;   // �������� �� ������ ������
  BYTE glErr;  // �������� �� ������ ������
  DWORD sumOk; // �������� �������� �� ������
  DWORD sumErr;// �������� �������� �� ������
}POL_SLIPS;
extern POL_SLIPS pol_slips;
/*----------------------------------------------------------------------------*/
// external value
extern struct cmd_nfo   g_cmd_nfo[CMD_NFO_SZ];
extern BOOL ETH_RECV_FLAG;
/*----------------------------------------------------------------------------*/
void        cmd_ch_init();
err_t       udp_sendstr(struct cmd_raw* cmd_p, char const* s);
err_t       udp_sendbuf(struct cmd_raw* cmd_p, char const* buf, int len);
void        get_cmd_ch_ip(struct ip_addr* ipaddr);
/*----------------------------------------------------------------------------*/
BOOL getStatusDK(void);
void setStatusDK(const BOOL flag);
void clearStatusDK(void);
DWORD retStatusNetSend(void);
void  setStatusNetSend(DWORD st);
DWORD retStatusNet(void);
void setStatusNet(DWORD st);
void setStatusOneDk(const BYTE nDk,const BOOL sSurd,const BYTE actSurd);
BOOL getStatusOneDk(const BYTE nDk);
void clearStatusOneDk(const BYTE nDk);
BOOL getAllDk(void);
BOOL getFlagNetwork(void);
void setFlagNetwork(const BOOL flag);
//
BOOL getAllSURD(void);
DWORD retStatusSurdSend(void);
void  setStatusSurdSend(DWORD st);
void clearStSurd(void);
DWORD retStSurd(void);
void setStSurd(DWORD st);
BOOL getStSurdOneDk(const BYTE nDk);
void clearStSurdOneDk(const BYTE nDk);
//���������� ��������� ������� ���� �� UDP
BOOL getFlagStatusSURD(void);
void setFlagStatusSURD(const BOOL flag);
//���������� ��������� ������� ���� ������� ��
BOOL getFlagLocalStatusSURD(void);
BYTE getValueLocalStatusSURD(void);
void setValueLocalStatusSURD(const BYTE flag);
//
void clearStatusNet(void);
//void clearStatusSurdSend(void);
//void clearStatusNetSend(void);
BOOL checkMasterMessageDk(const BYTE id,const DWORD pass,const DWORD idp,
                          const BYTE surdOn,const BYTE vpuOn,const BYTE vpuPhase,
                          const WORD stled,const BYTE valSurd);
BOOL checkSlaveMessageDk(const DWORD idp,const DWORD pass,const DWORD stnet,
                         const DWORD sdnet,const DWORD tmin);
// Tmin ����
void  setStTminOneDk(const BYTE nDk,const BYTE stTmin);
void  clearStTminOneDk(const BYTE nDk);
BOOL  getAllTmin(void);
void  clearStTmin(void);
DWORD retStTmin(void);
void  setStTmin(DWORD st);
BOOL  retFlagTminEnd(void);
void  setFlagTminEnd(const BOOL fl);
DWORD retStTminSend(void);
void  setStTminSend(DWORD st);

#endif // __CMD_CH_H__
