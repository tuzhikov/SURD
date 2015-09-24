/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __CMD_CH_H__
#define __CMD_CH_H__

#include "../lwip/lwiplib.h"
#include "../types_define.h"

#define CMD_DQUE_SZ     32  // max cmd's fifo size
#define CMD_NFO_SZ      32  // max cmd's count
#define MAX_ARGV_SZ     15  // cmd's argument vector size (in one command)
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
// структура для команд
struct cmd_nfo
{
    struct cmd_nfo* next;
    unsigned int    cmd_fl;
    char*           cmd_txt;
    char*           cmd_help;
    void            (*cmd_func)(struct cmd_raw* cmd_p, int argc, char** argv);
};
// структура ВПУ для всех  сетевых устройств
typedef struct _NET_VPU_{
  BYTE id;
  BYTE vpuOn;
  BYTE phase;
  WORD stLED;
}NET_VPU;
typedef struct _VIR_VPU{
 NET_VPU vpu[MAX_VIR_VPU];
 BYTE active; // работающий ВПУ
}VIR_VPU;
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
BOOL getStatusSURD(void);
void setStatusSURD(const BOOL flag);
void clearStatusSURD(void);
DWORD retStatusNetDk(void);
void  setStatusNetDk(DWORD st);
DWORD retStatusDk(void);
void setStatusDk(DWORD st);
void setStatusOneDk(const BYTE nDk);
BOOL getStatusOneDk(const BYTE nDk);
void clearStatusOneDk(const BYTE nDk);
BOOL getAllDk(void);
BOOL retNetworkOK(void);
void flagClaerNetwork(void);
void flagSetNetwork(const BOOL flag);
void clearStatusDk(void);
BOOL checkMasterMessageDk(const BYTE id,const DWORD pass,
                          const DWORD idp,const BYTE vpuOn,
                          const BYTE vpuPhase,const WORD stled);
BOOL checkSlaveMessageDk(const DWORD idp,const DWORD pass,const BOOL stnet,const BOOL sdnet);

#endif // __CMD_CH_H__
