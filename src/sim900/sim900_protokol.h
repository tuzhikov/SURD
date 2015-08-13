
#ifndef sim900_protokol_h
#define sim900_protokol_h

#include "../tn_user.h"
#include "../dk/structures.h"
#include "../memory/ds1390.h"
#include "../gps/gps.h"
#include "../utime.h"
#include "../gps/gps.h"

typedef struct _SMS_COMMAND
{
    unsigned char *word;   
    unsigned char type;
    void *ptr_data;
    unsigned long min;
    unsigned long max;
    unsigned char out_order;
}SMS_COMMAND;

 typedef struct _SMS_CONFIG
{
   unsigned long red;
   unsigned long a;
   unsigned long b;
   unsigned char mod;
   unsigned char res;
   unsigned char tel[13];
   unsigned char read;
   unsigned char telbat[13];
   unsigned char id[6];
   unsigned char  umax;  //???????? 123..130 (?????? ?? 10 - ???????? 12.3...13)
   unsigned char  umin;  //????????  110..120 ( ?????? ?? 10)
   unsigned char master;
   unsigned long per;
   unsigned char pass[5];
   unsigned char light;
}SMS_CONFIG;
////////////////


 
//////////////

//голова пакета
typedef struct {
    char code_cmd;           // код команды
    unsigned short lenght;   // Длинна пакета вместе с головой, телом и хвостом
}pack_header_t;

//хвост пакета
typedef struct {
    unsigned short crc;
}pack_tail_t;

//
typedef struct {
    unsigned char regim_new;
    time_t time;
}pack_sinhro_t;


void Init_sms_command(SMS_CONFIG *config);

unsigned short str_from_unicod(char *data);
void str_to_hight(char *data);
BOOL parse_sms(unsigned char *sms, SMS_CONFIG *config);
void Get_config_list(char *buf);
BOOL Send_SMS_to_master(unsigned char *buf, unsigned char *tel);

BOOL Packet_RS(unsigned char * buf);

#endif