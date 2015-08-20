/*****************************************************************************
*
* � 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __CMD_CH_H__
#define __CMD_CH_H__

#include "../lwip/lwiplib.h"
#include "../types_define.h"

#define CMD_DQUE_SZ     32   // max cmd's fifo size
#define CMD_NFO_SZ      32  // max cmd's count
#define MAX_ARGV_SZ     8   // cmd's argument vector size (in one command)
#define CMD_FL_ACT      0x0001

//step automate
//typedef enum {NUL,ONE,TWO,THREE,FOR,FIVE,END}STEP_NET;
//type command
typedef enum {ALL_DK,SET_PHASE,END_CMD}TYPE_CMD_BROADCAST;
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
// external value
extern struct cmd_nfo   g_cmd_nfo[CMD_NFO_SZ];
extern BOOL ETH_RECV_FLAG;
/*----------------------------------------------------------------------------*/
void        cmd_ch_init();
err_t       udp_sendstr(struct cmd_raw* cmd_p, char const* s);
err_t       udp_sendbuf(struct cmd_raw* cmd_p, char const* buf, int len);
void        get_cmd_ch_ip(struct ip_addr* ipaddr);
/*----------------------------------------------------------------------------*/
void setStatusDk(const BYTE nDk);
void clearStatusDk(const BYTE nDk);
BOOL getAllDk(void);
void clearAllStatusDk(void);
BOOL checkMessageDk(const BYTE id,const DWORD pass,const DWORD idp);


#endif // __CMD_CH_H__
