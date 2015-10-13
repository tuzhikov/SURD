/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __GPS_H__
#define __GPS_H__

#include "../memory/ds1390.h"

typedef struct _GPS_TIME
{
    unsigned char sec;
    unsigned char min;
    unsigned char hour;
    unsigned char date;
    unsigned char month;
    unsigned char year;
}GPS_TIME;

typedef struct _GPS_INFO
{

    GPS_TIME time_pack;       // Время считанное из пакета
    GPS_TIME time_now;        // Время присвоенное по синхроимпульсу PPS

    unsigned char health;     // Состояние модуля
    unsigned char fix_valid;  // Координаты установлены
    unsigned char time_valid; // Время синхронизировано

    struct
    {
        float Latitude;
        float Longitude;
        float Altitude;
    } Position;

    struct
    {
        float Velocity;
        float Course;
    } Velocity;
    unsigned long crc_error;
    unsigned long pack_found;
    unsigned char delta_time;

}GPS_INFO;

/*----------------------------------------------------------------------------*/
void GPS_init();
void uart0_int_handler();
void Get_gps_info(GPS_INFO *gps);
BOOL Get_GPS_valid(void);
BOOL Send_Data_uart(const char *buf, unsigned char len);
void GPS_PPS_int_handler(void);
//BOOL Get_GPS_Time(DS1390_TIME * time);
//BOOL Get_time_sync(void);
BOOL Synk_TIME(void);
unsigned char getValueGMT(void);
void setValidGMT(const BOOL val);

//extern unsigned char GPS_synk_flag;//=false;

#endif // __GPS_H__
