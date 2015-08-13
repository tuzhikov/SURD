/*****************************************************************************
*
* � 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __SIM900_H__
#define __SIM900_H__

#include "sim900_protokol.h"
#include "../types_define.h"

#define AT_    "AT+IPR="
#define DEF_BR 115200         /* Default SIM baud rate */
#define AT_end "\r\n"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define TEST(AT_IPR, DEF_BR, AT_IPR_end) AT_IPR""STR(DEF_BR)AT_IPR_end


#define IMEI_SIZE 16
#pragma pack (1)
#define SIM_TX_BUFF_SIZE    512
#define SIM_RX_BUFF_SIZE    800

//��������� ��� �������� ������ ������� ��� �������� � GSM-���������
typedef struct __PROV_DATA
{
	char IP[16];
	char port[6];
	char APN[24];
	char name[12];
	char pass[12];
} AP_PROV_DATA;

typedef enum __SIM_RETVAL
{
   SIM_NOERR   = 0,
   SIM_CALERR  = 1,
   SIM_GSM_ERR = 2,
   SIM_SERVER_ERR = 3
} SIM_RETVAL;



typedef struct __SIM900_CB
{
    U08 state;         // ������� ��������� ������ SIM300
    U08 time;

    U08 id[IMEI_SIZE];   // IMEI

    U08 buf[800];      // ������� �����

/*    U08 tel[12];       // ������-�������
    U08 dev_pass[8];   // ������ ���������� ��� �����������
    U08 serv_pass[8];  // ������ ������� ��� �����������
*/
    BOOL autoriz;      // ����������� �� �������

    U16 pack_rx;       // ����� �������� � ������ ����

    U32 GPRS_connect_count; // ������ �� ����������� �� GPRS

    BOOL GPRS_need_connect; // ����� ���������� GPRS
    BOOL bSend;             // ��� ���������� SIM900 ������� ������

    BOOL ready_GPRS_need_connect; // ��������� ������������ ��� ����������

    U08  GPRS_CSD;          // ��� �������������� ����������

    BOOL ALARM;             // ���������� �������� ������ �������
    U16 Send_Packet_count;  // �������� ������ ��������

    U08 SMS_number;         // ����� SMS ��� ���������
//    U08 Del_count;          // ���������� ������� ��� ��������
//    POWER_STATE power;

    U08 sim300_error;
    U08 try_GPRS_connect;
    U16 count_try_GPRS_connect;

    U16 wake_up_counter;

    U08 sms_error;          // ������ ������ SMS
    U08 sim900_Level;       // ��������� ��������� ������� ������� GSM

    U08 reboot;
    U08 sinhro;
    U08 CSD_try_count;
    U16 timer_CSD;

    U08 Send_SMS;

    U32 call_period;
    U08 connect;


    BOOL need_log;

    U32 power_off_count;
    BOOL flags_power_off;


    U08 led_flash;

} SIM900;

typedef struct __SIM_COMMAND{
 	char* command;           // AT �������
	char* ans_OK;            // ���������� �����
	char  go_OK;             // ������� �� ����������� ������
	char* ans_ERR;           // �� ���������� �����
	char  go_ERR;            // ������� �� �� ����������� ������
	unsigned char  time_out; // ����� �������� ������
	char  go_time_out;       // ������� ���� ��� ������
	BOOL (*fnc_PROC)(void);  // ��������� �� ������� ���������
	BOOL (*fnc_OK)(void);    // ��������� �� ������� ��������� � ������ �������������� ������
	BOOL (*fnc_ERR)(void);   // ��������� �� ������� ��������� � ������ �������������� ������
        BOOL (*fnc_TIME)(void);  // ��������� �� ������� ��������� � ������ TimeOut
}SIM_COMMAND;

typedef struct _PROJ_SMS_STRUCT
{
    SMS_CONFIG    sms_config;  // ��������� ���������������� ����� SMS
}PROJ_SMS_STRUCT;


void SIM900_init();
void uart2_int_handler();

extern PROJ_SMS_STRUCT  PROJ_SMS;


#endif // __SIM900_H__
