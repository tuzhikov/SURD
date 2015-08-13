/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
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

//Структура для хранения данных сервера для коннекта и GSM-оператора
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
    U08 state;         // Текущее состояние работы SIM300
    U08 time;

    U08 id[IMEI_SIZE];   // IMEI

    U08 buf[800];      // Рабочий буфер

/*    U08 tel[12];       // Мастер-телефон
    U08 dev_pass[8];   // Пароль устройства для подключения
    U08 serv_pass[8];  // Пароль сервера для подключения
*/
    BOOL autoriz;      // Авторизация на сервере

    U16 pack_rx;       // Число принятых в пакете байт

    U32 GPRS_connect_count; // Таймер до подключения по GPRS

    BOOL GPRS_need_connect; // Нужно подключать GPRS
    BOOL bSend;             // Ждём готовности SIM900 принять данные

    BOOL ready_GPRS_need_connect; // пробовать подключиться при готовности

    U08  GPRS_CSD;          // Тип установленного соединения

    BOOL ALARM;             // Необходимо передать сигнал тревоги
    U16 Send_Packet_count;  // Ожидание обмена пакетами

    U08 SMS_number;         // Номер SMS для обработки
//    U08 Del_count;          // Количество записей для удаления
//    POWER_STATE power;

    U08 sim300_error;
    U08 try_GPRS_connect;
    U16 count_try_GPRS_connect;

    U16 wake_up_counter;

    U08 sms_error;          // Ошибки чтения SMS
    U08 sim900_Level;       // Последний считанный уровень сигнала GSM

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
 	char* command;           // AT команда
	char* ans_OK;            // Правильный ответ
	char  go_OK;             // Переход по правильному ответу
	char* ans_ERR;           // Не правильный ответ
	char  go_ERR;            // Переход по не правильному ответу
	unsigned char  time_out; // Время ожидания ответа
	char  go_time_out;       // Переход если нет ответа
	BOOL (*fnc_PROC)(void);  // Указатель на функцию обработки
	BOOL (*fnc_OK)(void);    // Указатель на функцию обработки в случае положительного ответа
	BOOL (*fnc_ERR)(void);   // Указатель на функцию обработки в случае отрицательного ответа
        BOOL (*fnc_TIME)(void);  // Указатель на функцию обработки в случае TimeOut
}SIM_COMMAND;

typedef struct _PROJ_SMS_STRUCT
{
    SMS_CONFIG    sms_config;  // структура конфигурирования через SMS
}PROJ_SMS_STRUCT;


void SIM900_init();
void uart2_int_handler();

extern PROJ_SMS_STRUCT  PROJ_SMS;


#endif // __SIM900_H__
