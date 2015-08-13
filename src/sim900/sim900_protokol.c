//#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sim900_protokol.h"
#include "../dk/dk.h"
#include "../dk/structures.h"
#include "sim900.h"
#include "../crc32.h"
#include "../gps/gps.h"


extern SIM900 sim;
static float string_to_float ( unsigned char * buf);

extern unsigned char regim;
extern unsigned char regim_new;

//PROJ_SMS_STRUCT  PROJ_SMS;

const char letter[26][2]=
{
    {'a','A'},
    {'b','B'},
    {'c','C'},
    {'d','D'},
    {'e','E'},
    {'f','F'},
    {'g','G'},
    {'h','H'},
    {'i','I'},
    {'j','J'},
    {'k','K'},
    {'l','L'},
    {'m','M'},
    {'n','N'},
    {'o','O'},
    {'p','P'},
    {'q','Q'},
    {'r','R'},
    {'s','S'},
    {'t','T'},
    {'u','U'},
    {'v','V'},
    {'w','W'},
    {'x','X'},
    {'y','Y'},
    {'z','Z'},
};



SMS_COMMAND sms_command[15]=
{
    {"RED",      'u', 0,    1,   65535,     0},
    {"A",        'u', 0,    1,   65535,     1},
    {"BR",       'b', 0,    1,     100,     6},
    {"B",        'u', 0,    1,   65535,     2},
    {"MOD",      'c', 0,    1,       3,     3},
    {"RES",      ' ', 0,    0,       0,  0xFF},    
    {"TELBAT",   's', 0,   12,      12,     4},
    {"TEL",      's', 0,   12,      12,     5},
    {"READ",     ' ', 0,    0,       0,  0xFF},
    {"ID",       's', 0,    1,       5,    11}, 
    {"UMAX",     'f', 0,  123,     130,     7},
    {"UMIN",     'f', 0,  110,     120,     8}, 
    {"MASTER",   'c', 0,    0,       1,     9},
    {"PER",      'u', 0,    0,    3000,    10},
    {"PASS",     's', 0,    4,       4,  0xFF},

};

void Init_sms_command(SMS_CONFIG *config)
{
    memcpy(config, &PROJ_SMS.sms_config, sizeof(SMS_CONFIG));
    
    sms_command[0].ptr_data = &config->red;
    sms_command[1].ptr_data = &config->a;
    sms_command[2].ptr_data = &config->light;
    sms_command[3].ptr_data = &config->b;
    sms_command[4].ptr_data = &config->mod;
    sms_command[5].ptr_data = &config->res;
    sms_command[6].ptr_data = config->telbat;
    sms_command[7].ptr_data = config->tel;
    sms_command[8].ptr_data = &config->read;    
    sms_command[9].ptr_data = config->id;
    sms_command[10].ptr_data = &config->umax;
    sms_command[11].ptr_data = &config->umin;
    sms_command[12].ptr_data = &config->master;
    sms_command[13].ptr_data = &config->per;
    sms_command[14].ptr_data = &config->pass;

    
    memset(&config->pass, 0, sizeof(config->pass));
    config->read = 0;
    config->res  = 0;
}


unsigned short str_from_unicod(char *data)
{
    unsigned short t;
    unsigned short i=0;
    
    //А вдруг UNICODE ???
    for (t=0; t < 500; t++)
    {
        if (data[t]==0x0A)
        {
            data[i] = 0;
            break;
        }

        if (data[t] != 0 && data[t] != 4) 
            data[i++] = data[t];
    }
    return i;
}


void str_to_hight(char *data)
{
    while(*data)
    {
        for (unsigned char y = 0; y < 26; y++)
        {
            if (*data == letter[y][0])
            {
                *data = letter[y][1];
                break;
            }
        }
        data++;
    }    
}


BOOL parse_sms(unsigned char *sms, SMS_CONFIG *config)
{
    unsigned short len = strlen((char const*)sms);
    unsigned short ptr_word=0;
    unsigned short ptr_data;
    unsigned long temp;
    BOOL result = TRUE;

    if (!sms_command[0].ptr_data)
        return FALSE;
    
    for (unsigned short i = 0; i<len; i++)
        if (sms[i] == ',' || sms[i] == ' ')
            sms[i] = 0;

    while(ptr_word < len)
    {
        if ((sms[ptr_word]>='0' && sms[ptr_word]<='9')
           &&(sms[ptr_word+1]>='0' && sms[ptr_word+1]<='9')
           &&(sms[ptr_word+2]>='0' && sms[ptr_word+2]<='9')
           &&(sms[ptr_word+3]>='0' && sms[ptr_word+3]<='9'))
        {
          memcpy(sms_command[13].ptr_data, &sms[ptr_word],4);
          break;
        }

        while(sms[++ptr_word] != 0);
        while(sms[++ptr_word] == 0);
    }
    
    ptr_word = 0;
    
    while (ptr_word < len)
    {
        if (sms[ptr_word] != 0)
        {
            for (unsigned char i = 0; i < sizeof(sms_command)/sizeof(SMS_COMMAND); i++)
            {
                if (!memcmp(&sms[ptr_word], sms_command[i].word, strlen((char const*)sms_command[i].word)))
                {
                    ptr_data = ptr_word+strlen((char const*)sms_command[i].word);
                    switch (sms_command[i].type)
                    {
                        case 'b':
                            if (sscanf((char const*)&sms[ptr_data], "%u", &temp) == 1)
                                if (temp >= sms_command[i].min && temp <= sms_command[i].max)
                                    *(unsigned char*)sms_command[i].ptr_data = temp;
                                else 
                                    result = FALSE;
                        break;
                        
                        case 'u':
                            if (sscanf((char const*)&sms[ptr_data], "%u", &temp) == 1)
                                if (temp >= sms_command[i].min && temp <= sms_command[i].max)
                                    *(unsigned long*)sms_command[i].ptr_data = temp;
                                else 
                                    result = FALSE;
                        break;

                        case 'c':
                            temp = sms[ptr_data]-'0';
                            if (temp >= sms_command[i].min && temp <= sms_command[i].max)
                                *(unsigned char*)sms_command[i].ptr_data = temp;
                            else 
                                result = FALSE;
                        break;

                        case 's':
                            temp = strlen((char const*)&sms[ptr_data]);
                            if (temp >= sms_command[i].min && temp <= sms_command[i].max)
                            {    
                                memcpy(sms_command[i].ptr_data, &sms[ptr_data], temp);
                                *(((unsigned char*)sms_command[i].ptr_data)+temp) = 0;
                            }
                            else 
                                result = FALSE;
                        break;

                        case 'f':
                            temp = (long)(string_to_float((unsigned char*)&sms[ptr_data])*10);
                            if (temp >= sms_command[i].min && temp <= sms_command[i].max)
                                *(char*)sms_command[i].ptr_data = temp;
                            else 
                                result = FALSE;
                        break;
                        
                        case ' ':
                            *(unsigned char*)sms_command[i].ptr_data = 1;
                        break;
                    }
                    break; // end for
                }// end if
            } // end for
                    
            while(sms[ptr_word++] && ptr_word < len);
        }
        else
            ptr_word++;
    }
    
    return result;
}

void Get_config_list(char *buf)
{
    buf[0]=0;
    char str[15];

    for (unsigned char y=0; y<sizeof(sms_command)/sizeof(SMS_COMMAND); y++)
    {    
        for (unsigned char i=0; i<sizeof(sms_command)/sizeof(SMS_COMMAND); i++)
        {
            if (sms_command[i].type != ' ' && y==sms_command[i].out_order)
            {
                strcat(buf,(char const*)sms_command[i].word);
                switch (sms_command[i].type)
                {
                    case 'u':
                        snprintf(str, 15, "%u,", *(long*)sms_command[i].ptr_data);
                    break;
                        
                    case 'b':
                        snprintf(str, 15, "%u,", *(unsigned char*)sms_command[i].ptr_data);
                    break;

                    case 'c':
                        snprintf(str, 15, "%u,", *(char*)sms_command[i].ptr_data);
                    break;
            
                    case 's':
                        snprintf(str, 15, "%s,", (char*)sms_command[i].ptr_data);
                    break;
            
                    case 'f':
                        snprintf(str, 15, "%d.%d,", *(char*)sms_command[i].ptr_data/10, *(char*)sms_command[i].ptr_data%10);
                    break;
                }
                strncat(buf, str, sizeof(str));
                break;
            }
        }
    }
    
    buf[strlen(buf)-1] = 0; // Удалим последнюю запятую
}



//Отправка SMS на мастер-телефон
BOOL Send_SMS_to_master(unsigned char *buf, unsigned char *tel)
{
   
    if (PROJ_SMS.sms_config.telbat[0] == '+') 
    {   
        for (unsigned char i = 1; i<12;i++)
            if (PROJ_SMS.sms_config.telbat[i]<'0' || PROJ_SMS.sms_config.telbat[i]>'9')
                return FALSE;

        buf[0] = 0;
        
        sprintf((char*)buf, "WARNING! ID=%s, IMEI=%s Uacc < %d.%d V", 
                PROJ_SMS.sms_config.id, sim.id, (unsigned int)PROJ_SMS.sms_config.umin/10,
                (unsigned int)PROJ_SMS.sms_config.umin%10);
        
        memcpy (tel, PROJ_SMS.sms_config.telbat, 13);
 
        return TRUE;
    }

    return FALSE;          
}








BOOL Packet_RS(unsigned char * buf)
{
    
    BOOL send = TRUE;
    U08 * data = buf + sizeof(pack_header_t);  // указатель на данные


    U32 CRC;
    U16 pack_size = buf[1]+buf[2]*256;

    switch (buf[0])
    {
        case 0:
            regim_new = ((pack_sinhro_t*)data)->regim_new;
            //if (!Get_GPS_valid())
            {
                stime(&((pack_sinhro_t*)data)->time);
            }

            sim.sinhro = 1;
            sim.connect = 1;            
            buf[0] = 1;
        break;
    
        case 1:
            sim.sinhro = 1; 
            sim.connect = 1;
            send = FALSE;
        break;
        
        case 0xFF:
            ((pack_sinhro_t*)data)->regim_new = regim_new;
        
            //tn_task_sleep (1005 - tn_msec_tick());
            ((pack_sinhro_t*)data)->time = time(NULL);
  
            buf[0] = 0;
            buf[1] = 3+2+sizeof(pack_sinhro_t);
            buf[2] = 0;
        break;
    }
    if (send)
    {
	pack_size = buf[1]+buf[2]*256;
	CRC = crc32_calc((U08*)buf, pack_size - sizeof(pack_tail_t));
	buf[pack_size-2] = (U08)CRC;
	buf[pack_size-1] = (U08)(CRC>>8);
    }

    return send;
}



static float string_to_float ( unsigned char * buf)
{
    unsigned long data = 0;
    unsigned long del = 1;
    unsigned char i = 0;
    BOOL dot = FALSE;
    
    while((buf[i]>='0' && buf[i]<='9') || (buf[i] == '.' && !dot))
    {
        if (buf[i] == '.')
        {
            dot = TRUE;
        }else
        {
            data *= 10;
            data += buf[i]-'0';

            if (dot)
                del*=10;
        }  
        if (i++ > 10)
            break;
    };
    return (float)data/del;
}