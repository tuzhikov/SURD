/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __DS1390_H__
#define __DS1390_H__

#define DS1390_REG_100THS               0x00
#define DS1390_REG_SECONDS              0x01
#define DS1390_REG_MINUTES              0x02
#define DS1390_REG_HOURS                0x03
#define DS1390_REG_DAY                  0x04
#define DS1390_REG_DATE                 0x05
#define DS1390_REG_MONTH_CENT           0x06
#define DS1390_REG_YEAR                 0x07

#define DS1390_REG_ALARM_100THS         0x08
#define DS1390_REG_ALARM_SECONDS        0x09
#define DS1390_REG_ALARM_MINUTES        0x0A
#define DS1390_REG_ALARM_HOURS          0x0B
#define DS1390_REG_ALARM_DAY_DATE       0x0C
 
#define DS1390_REG_CONTROL              0x0D
#define DS1390_REG_STATUS               0x0E
#define DS1390_REG_TRICKLE              0x0F

typedef struct __DS1390_DATA_BUF_
{
    unsigned char msec;
    unsigned char sec;
    unsigned char min;
    unsigned char hour;
    unsigned char day;
    unsigned char date;
    unsigned char month;
    unsigned char year;
    unsigned char alarm_mlsec;
    unsigned char alarm_sec;
    unsigned char alarm_min;
    unsigned char alarm_hour;
    unsigned char alarm_date;
    unsigned char control;
    unsigned char status;
    unsigned char trickle_charger;
}DS1390_DATA_BUF;


typedef struct _DS1390_TIME
{
    unsigned short msec;
    unsigned char sec;
    unsigned char min;
    unsigned char hour;
    unsigned char day;
    unsigned char date;
    unsigned char month;
    unsigned short year;
}DS1390_TIME;



BOOL ds1390_init();
BOOL SetTime_DS1390(DS1390_TIME * time);
BOOL GetTime_DS1390(DS1390_TIME * time);
void DS1390_int_handler(void);
extern char get_day(DS1390_TIME * time);
char get_week_num(DS1390_TIME * time);

#define EVENT_SINHRO_TIME 1


#endif // __DS1390_H__