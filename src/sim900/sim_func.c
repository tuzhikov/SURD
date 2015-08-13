/* *********************************************************************************************************************
     Project:         Resurs
     Project desc.:   Telemetry from GSM
     Author:          Bolid (c)

   *********************************************************************************************************************
     Distribution:

     TNKernel real-time kernel 2.4
     -----------------------------

     Copyright © 2011 Selyutin Alex
     All rights reserved.


     PIC24F Family Peripheral Support Library
     ----------------------------------------

     Copyright © 2010-2011 Bolid

   *********************************************************************************************************************
     Compiled Using:  Microchip C30 V.3.0
     Processor:       PIC24FJ64GA006

   *********************************************************************************************************************
     File:            sim_func.c
     Description:     SIM 900 GSM module

   *********************************************************************************************************************
     History:         2010/04/22 [GS]  File created

   ****************************************************************************************************************** */

#include <libpic30.h>
#include <stdio.h>
#include <string.h>
#include "protocol.h"

#include "..\\..\\tnkernel\\tnkernel.h"         /* TNKernel API       */
#include "..\\..\\csp\\pic24f\\pic24f_csp.h"    /* Peripheral library */
#include "..\\..\\bsp\\tele_gsm\\led_extr.h"       /* LEDs driver        */
#include "..\\..\\bsp\\tele_gsm\\asens_extr.h"     /* SENs driver        */
#include "..\\..\\bsp\\tele_gsm\\rele_extr.h"      /* RELE driver        */
#include "..\\..\\bsp\\tele_gsm\\sim_extr.h"
#include "..\\..\\bsp\\tele_gsm\\md5.h"
#include "..\\..\\bsp\\tele_gsm\\power_monitor.h"

#include "..\\..\\appl\\tele_gsm\\appl_param.h"
#include "..\\..\\appl\\tele_gsm\\appl_cfg.h"
#include "..\\..\\slffs\\slffs.h"



#define PWRKEY_OFF()     csp_gpio_Clr(SIM_ON_PORT, SIM_ON_PIN)
#define PWRKEY_ON()      csp_gpio_Set(SIM_ON_PORT, SIM_ON_PIN)
#define POWER_ON()       csp_gpio_Clr(SIM_POWER_PORT, SIM_POWER_PIN)
#define POWER_OFF()      csp_gpio_Set(SIM_POWER_PORT, SIM_POWER_PIN)

#define NEW_LED

extern RELE_CB   RELE_1;
extern RELE_CB   RELE_2;
extern AP_PROV_DATA     AD_Prov_Data;
extern sms_send_count_t sms_send;


UART_SBUF   TXbuff1, RXbuff1;
U08         TXbuff_mem1[SIM_TX_BUFF_SIZE];
U08         RXbuff_mem1[SIM_RX_BUFF_SIZE];


extern LED_CB    LED_2;
extern TN_MUTEX  RTC_Lock;
extern TN_MUTEX  PARAM_Lock;                   /* Parameters resource mutex     */

extern config_data_t      AD_Device_Config;
extern const AP_PROV_DATA    AC_Prov_Data;
extern sms_send_count_t sms_send;
extern POWER_MONITOR power_mn;

const char* Message[]={
            "U+12V - Error",    	                          // 0000 0000 0000 0001 - Нарушение основного источника питания
            "U ACC - Error",	                              // 0000 0000 0000 0010 - Нарушение резервного источника питания
            "U+12V - OK",                                     // 0000 0000 0000 0100 - Восстановление основного источника питания
            "U ACC - OK",                                     // 0000 0000 0000 1000 - Восстановление резервного источника питания
            "Counter 1 - Error",                              // 0000 0000 0001 0000 - Нарушение линии счетчика № 1
            "Counter 2 - Error",                              // 0000 0000 0010 0000 - Нарушение линии счетчика № 2
            "Counter 3 - Error",                              // 0000 0000 0100 0000 - Нарушение линии счетчика № 3
            "Counter 4 - Error",                              // 0000 0000 1000 0000 - Нарушение линии счетчика № 4
            "Counter 1 - OK",                                 // 0000 0001 0000 0000 - Востановление линии счетчика № 1
            "Counter 2 - OK",                                 // 0000 0010 0000 0000 - Востановление линии счетчика № 2
            "Counter 3 - OK",                                 // 0000 0100 0000 0000 - Востановление линии счетчика № 3
            "Counter 4 - OK",                                 // 0000 1000 0000 0000 - Востановление линии счетчика № 4
            "SMS - OK"                                        // 0001 0000 0000 0000 - SMS для не блокировки абонента
};


void bsp_sim_buff_flush (UART_SBUF *buf);
void DelLogCount(void);

void SimSendPack(void);
BOOL CheckServer(AP_PROV_DATA* AD_Prov_Data);
void FloatString (char* buf, float data);
void MakeSMS(char* buf);
void Del_SMS(void);


U16 sim_str_put2(U08* str);
BOOL TNumber_Check (U08 *buf, U08 N, U08 *dest);
BOOL Send_SMS_to_master(U08 *buf);

SIM300 sim;


BOOL Sim_Power_Off(void);
BOOL Sim_Power_On(void);
BOOL Sim_PKeyOff(void);
BOOL Sim_PKeyOn(void);
BOOL Sim_BitRateBACK(void);
BOOL Sim_BitRateDEF(void);
BOOL Sim_AT_GSN(void);
BOOL Sim_AT_CREG(void);
BOOL Sim_SMS_Read(void);
BOOL Sim_Led_on(void);
BOOL Sim_Led_Off(void);
BOOL Sim_Ring(void);
BOOL Sim_Connect(void);
BOOL Sim_GPRS(void);
BOOL Sim_AT_CSQ(void);
BOOL Sim_AT_CIPCSGP(void);
BOOL Sim_AT_CIPSTART(void);
BOOL Sim_GPRS_Conn(void);
BOOL Send_SMS(void);


BOOL Sim_Get_Pack (void);

/*

  typedef struct __SIM_COMMAND{
	char* command;
	char* ans_OK;
	char  go_OK;
	char* ans_ERR;
	char  go_ERR;
	unsigned char  time_out;
	char  go_time_out;
	BOOL (*fnc_PROC)(void);  // Указатель на функцию обработки
	BOOL (*fnc_OK)(void);    // Указатель на функцию обработки в случае положительного ответа 
	BOOL (*fnc_ERR)(void);   // Указатель на функцию обработки в случае отрицательного ответа
	BOOL (*fnc_TIME)(void);  // Указатель на функцию обработки в случае TimeOut
}SIM_COMMAND;

*/




const SIM_COMMAND sim_st[]=
{
	
    //  command               ans_OK             go_OK     ans_ERR           go_ERR   time  go_time  fnc_PROC         fnc_OK             fnc_ERR       fnc_TIME
	  {"AT+CPOWD=1\r\n",      "NORMAL",             1,     "",               0xFF,     10,      1,   0,               Sim_Power_Off,    0,            Sim_Power_Off     }, // 00 - Программное выключение SIM
	  {"",                    "",                   0,     "",               0xFF,      5,      2,   0,               0,                0,            Sim_Power_On      }, // 01 - Подаём питание
	  {"",                    "",                0xFF,     "",               0xFF,      5,      3,   0,               0,                0,            Sim_PKeyOff       }, // 02 - Выключаем Power_Key
	  {"",                    "RDY",                4,     "",               0xFF,     15,      4,   0,               Sim_PKeyOn,       0,            Sim_PKeyOn        }, // 03 - Выключаем Power_Key
      
      {"ATE0\r\n",            "OK",                 7,     "RDY",               4,     10,      5,   0,               0,                0,            Sim_BitRateBACK   }, // 04 - Выключить эхо

      {TEST(AT_,DEF_BR,AT_end),"OK",                6,     "",               0xFF,     10,      1,   0,               Sim_BitRateDEF,   0,            0                 }, // 05 - Установить скорость
      
      {"AT&W\r\n",            "OK",                 4,     "",               0xFF,     10,      0,   0,               0,                0,            0,                }, // 06 - Сохранить настройки
      {"AT+CMGF=1\r\n",       "OK",                 8,     "",               0xFF,     10,      0,   0,               0,                0,            0,                }, // 07 - настроить формат сообщений
      {"AT+CPIN?\r\n",        "+CPIN: READY",      10,     "+CPIN: SIM PIN",    9,     10,      0,   0,               0,                0,            0,                }, // 08 - запрос готовности SIM-карты
      
	  {"AT+CPIN=0000\r\n",    "OK",                 8,     "",               0xFF,     20,      0,   0,               0,                0,            0,                }, // 09 - ввести PIN-код
		
	  {"AT+GSN\r\n",          "",                  11,     "",				 0xFF,     20,      0,   Sim_AT_GSN,      0,                0,            0,                }, // 10 - Запрос IMEI
	  {"",                    "OK",                12,     "",				 0xFF,     20,      0,   0,               0,                0,            0,                }, // 11 - Результат IMEI

// Проверка регистрации в сети
      {"AT+CREG?\r\n",        "+CREG:",            13,     "",			     0xFF,   0xF0,      0,   Sim_AT_CREG,     0,                0,            0,                }, // 12 - Запрос готовности сети
	  {"",                    "OK",                14,     "",		         0xFF,     10,      0,   0,               0,                0,            0,                }, // 13 - Подтвердить, что сеть готова

//Проверка SMS
	  {"AT+CMGR=1\r\n",       "+CMGR:",            15,     "OK",               16,     20,     15,   0,               0,                Sim_Led_Off,  0,                }, // 14 - Если есть SMS номер 1
	  {"",                    "OK",                40,     "",			       17,     20,     17,   Sim_SMS_Read,    0,                0,            0,                }, // 15 - Ждём ОК
	  {"",                    "RING",              23,     "",			        0,     50,     18,   0,               0,                0,            Sim_Led_on        }, // 16 - Ждём звонка
	  {"AT+CMGD=1,0\r\n",     "OK",                18,     "",			     0xFF,     20,      0,   0,               0,                0,            0,                }, // 17 - Удаляем СМС и идём дальше
			
	  {"AT+CMGR=2\r\n",       "+CMGR:",            19,     "OK",  		       20,     20,     19,   0,               0,                Sim_Led_Off,  0,                }, // 18 - Если есть SMS номер 2
	  {"",                    "OK",                41,     "",			       21,     20,     21,   Sim_SMS_Read,    0,                0,            0,                }, // 19 - Ждём ОК
	  {"",                    "RING",              23,     "",			        0,     50,     22,   0,               0,                0,            Sim_Led_on        }, // 20 - Ждём звонка
	  {"AT+CMGD=2,0\r\n",     "OK",                23,     "",			     0xFF,     20,      0,   0,               0,                0,            0,                }, // 21 - Удаляем СМС и идём дальше

//Запрос уровня сигнала
      {"AT+CSQ\r\n",          "+CSQ:",             11,     "+CME ERROR",       11,     20,     11,   0,               Sim_AT_CSQ,       0,            0,                }, // 22 - запросить уровень сигнала

// Запрос типа звонка
	  {"AT+CLCC\r\n",         "+CLCC:",            24,     "",  			 0xFF,     20,      0,   0,               0,                0,            0,                }, // 23 - Определить тип звонка
	  {"",                    "OK",                25,     "RING",   	       26,     50,     26,   0,               Sim_Ring,         0,            0,                }, // 24 - Подтвердить, что сеть готова

      {"ATA\r\n",             "",                0xFF,     "ERROR",    		   14,     20,     27,   0,               0,                0,            0,                }, // 25 - поднять трубку
	  {"ATH\r\n",             "OK",                14,     "ERROR",  		   14,     20,     31,   0,               Sim_GPRS,         0,            0,                }, // 26 - положить трубку

	  {"",                    "CONNECT",           28,     "",				 0xFF,    150,     14,   0,               0,                0,            0,                }, // 27 - Дождаться установки соединения
	  {"",                    "",                0xFF,     "NO CARRIER",       26,    254,     29,   Sim_Connect,     0,                0,            0,                }, // 28 - Общение по CSD

      {"+++",                 "OK",                26,     "",				 0xFF,     10,     25,   0,               0,                0,            0,                }, // 29 - Перейти в коммандный режим

	  {"",                    "OK",                66,     "",				    0,     20,     66,   0,               0,                0,            0,                }, // 30 - Подтвердить, что сеть готова

// Подключаем GPRS
      {"AT+CGATT=1\r\n",      "OK",                32,     "ERROR",            14,    100,      0,   0,               0,                0,            0,                }, // 31
      {"",                    "OK",                33,     "ERROR",            38,    100,      0,   Sim_AT_CIPCSGP,  0,                0,            0,                }, // 32
      {"AT+CIPHEAD=0\r\n",    "OK",                34,     "ERROR",            14,     20,      0,   0,               0,                0,            0,                }, // 33
      {"AT+CIPMUX=0\r\n",     "OK",                35,     "ERROR",            14,     20,      0,   0,               0,                0,            0,                }, // 34
      {"AT+CIPMODE=0\r\n",    "OK",                36,     "ERROR",            14,     20,      0,   0,               0,                0,            0,                }, // 35
      {"",                    "OK",                42,     "ERROR",            14,    100,      0,   Sim_AT_CIPSTART, 0,                0,            0,                }, // 36
      {"",                    "",                  43,     "ERROR",            37,     50,     38,   Sim_GPRS_Conn,   0,                0,            0,                }, // 37

// Отключаем GPRS
      {"AT+CIPCLOSE\r\n",     "CLOSED",            39,     "ERROR",            39,     50,     39,   0,               0,                0,            0,                }, // 38
      {"AT+CIPSHUT\r\n",      "SHUT OK",           14,     "ERROR",            14,     50,     14,   0,               0,                0,            0,                }, // 39
  
// Отправка SMS
      {"",                    "+CMGS",             17,     "+CMS ERROR:",      12,     50,     12,   Send_SMS,        0,                0,            0,                }, // 40 - Отправка ответа на SMS1      
      {"",                    "+CMGS",             21,     "+CMS ERROR:",      12,     50,     12,   Send_SMS,        0,                0,            0,                }, // 41 - Отправка ответа на SMS2
      {"",                    "CONNECT OK",        37,     "ERROR",            37,     50,     38,   0,               0,                0,            0,                }, // 42
      {"",                    "SEND OK",           37,     "ERROR",            37,     50,     38,   0,               0,                0,            0,                }, // 43

      {"",                    "+CMGS",             22,     "+CMS ERROR:",      12,     50,     12,   Send_SMS,        0,                0,            0,                }, // 44 - Отправка Экстеной SMS

};


/* *********************************************************************************************************************
    Extern function prototypes
   ****************************************************************************************************************** */
void Logger_Clr_Cnt(void);


SIM_RETVAL bsp_sim_Conf (void)
{

    /* Config SIM900 PIO */
    csp_gpio_DirIn(SIM_RX_TRIS,      SIM_RX_PIN);
    csp_gpio_DirOut(SIM_TX_TRIS,     SIM_TX_PIN);
    csp_gpio_DirOut(SIM_ON_TRIS,     SIM_ON_PIN);
    csp_gpio_DirOut(SIM_POWER_TRIS,  SIM_POWER_PIN);
    
	PWRKEY_ON();
    POWER_OFF();             //Выключить питание

    /* Config used UART */

    csp_uart1_Cfg(UART_EN                  |
                  UART_IDLE_STOP           |
                  UART_IRDA_DIS            |
                  UART_MODE_SIMPLEX        |
                  UART_UEN_00              |
                  UART_WAKE_DIS            |
                  UART_LOOPBACK_DIS        |
                  UART_ABAUD_DIS           |
                  UART_UXRX_IDLE_ONE       |
                  UART_BRGH_SIXTEEN        |
                  UART_NO_PAR_8BIT         |
                  UART_1STOPBIT            ,

                  UART_INT_TX_EACH_CHAR    |
                 // UART_INT_TX_BUF_EMPTY |
                 // UART_INT_TX_LAST_CH   |
                  UART_SYNC_BREAK_DIS      |
                  UART_TX_EN               |
                  UART_INT_RX_CHAR         |
                  UART_ADR_DETECT_DIS
                 );

    Sim_BitRateDEF ();
    
    /* Config TX and RX cyclic buffers */

    TXbuff1.buff = TXbuff_mem1;
    TXbuff1.size = sizeof(TXbuff_mem1);
    bsp_sim_buff_flush(&TXbuff1);

    RXbuff1.buff = RXbuff_mem1;
    RXbuff1.size = sizeof(RXbuff_mem1);
    bsp_sim_buff_flush(&RXbuff1);

    /* Enable UART1 RX and TX interrupts */
    csp_uart1_TXIRQ_Cfg(TN_INTERRUPT_LEVEL, TRUE);
    csp_uart1_Err_Clr(UART_ERR_FERR | UART_ERR_OERR);
    csp_uart1_RXIRQ_Cfg(TN_INTERRUPT_LEVEL, TRUE);
    csp_uart1_ERIRQ_Cfg(TN_INTERRUPT_LEVEL, TRUE);
    
	sim.power_off_count = 0;
    
//    sim.tel[0] = 0;
    sim.state = 1;
    sim.reboot = 0;
    sim.time = 0;
	sim.need_log = FALSE;
    
    return SIM_NOERR;
}


void bsp_sim_buff_flush (UART_SBUF *buf)
{
    buf->pRD = buf->buff;
    buf->pWR = buf->buff;
    buf->cnt = 0;

    buf->stat.EMPTY = TRUE;
    buf->stat.FULL  = FALSE;
    buf->stat.ERR   = FALSE;
    buf->stat.OERR  = FALSE;
}
/*
U08 mas[256]={
     0x00,0x5E,0xBC,0xE2,0x61,0x3F,0xDD,0x83,0xC2,0x9C,0x7E,0x20,0xA3,0xFD,0x1F,0x41,   //0
     0x9D,0xC3,0x21,0x7F,0xFC,0xA2,0x40,0x1E,0x5F,0x01,0xE3,0xBD,0x3E,0x60,0x82,0xDC,   //1
     0x23,0x7D,0x9F,0xC1,0x42,0x1C,0xFE,0xA0,0xE1,0xBF,0x5D,0x03,0x80,0xDE,0x3C,0x62,   //2
     0xBE,0xE0,0x02,0x5C,0xDF,0x81,0x63,0x3D,0x7C,0x22,0xC0,0x9E,0x1D,0x43,0xA1,0xFF,   //3
     0x46,0x18,0xFA,0xA4,0x27,0x79,0x9B,0xC5,0x84,0xDA,0x38,0x66,0xE5,0xBB,0x59,0x07,   //4
     0xDB,0x85,0x67,0x39,0xBA,0xE4,0x06,0x58,0x19,0x47,0xA5,0xFB,0x78,0x26,0xC4,0x9A,   //5
     0x65,0x3B,0xD9,0x87,0x04,0x5A,0xB8,0xE6,0xA7,0xF9,0x1B,0x45,0xC6,0x98,0x7A,0x24,   //6
     0xF8,0xA6,0x44,0x1A,0x99,0xC7,0x25,0x7B,0x3A,0x64,0x86,0xD8,0x5B,0x05,0xE7,0xB9,   //7
     0x8C,0xD2,0x30,0x6E,0xED,0xB3,0x51,0x0F,0x4E,0x10,0xF2,0xAC,0x2F,0x71,0x93,0xCD,   //8
     0x11,0x4F,0xAD,0xF3,0x70,0x2E,0xCC,0x92,0xD3,0x8D,0x6F,0x31,0xB2,0xEC,0x0E,0x50,   //9
     0xAF,0xF1,0x13,0x4D,0xCE,0x90,0x72,0x2C,0x6D,0x33,0xD1,0x8F,0x0C,0x52,0xB0,0xEE,   //A
     0x32,0x6C,0x8E,0xD0,0x53,0x0D,0xEF,0xB1,0xF0,0xAE,0x4C,0x12,0x91,0xCF,0x2D,0x73,   //B
     0xCA,0x94,0x76,0x28,0xAB,0xF5,0x17,0x49,0x08,0x56,0xB4,0xEA,0x69,0x37,0xD5,0x8B,   //C
     0x57,0x09,0xEB,0xB5,0x36,0x68,0x8A,0xD4,0x95,0xCB,0x29,0x77,0xF4,0xAA,0x48,0x16,   //D
     0xE9,0xB7,0x55,0x0B,0x88,0xD6,0x34,0x6A,0x2B,0x75,0x97,0xC9,0x4A,0x14,0xF6,0xA8,   //E
     0x74,0x2A,0xC8,0x96,0x15,0x4B,0xA9,0xF7,0xB6,0xFC,0x0A,0x54,0xD7,0x89,0x6B,0x35    //F
};

char CRCn8(char * buf)
{
    U08 i;
    U08 CRC = 0x00;
    
    for (i = 0; i< buf[1] ; i++)
      CRC = mas[CRC ^ buf[i]];
    
    return CRC;
}
*/
U08 arr_p=0;

void TN_TASK bsp_sim_Handle (void)
{

/*
U08 arr1[11]={0x03, 0x0B, 0x00, 0x40, 0x30, 0x00, 0x00, 0x01,0xB0, 0x11, 0x53};

U08 i;*/
//POWER_ON();           //Даём питание

/*
for (i=0; i<sizeof(arr1); i++)
sim.buf[i] = arr1[i];

sendrs485 (sim.buf);
return;
*/
	while (1)
	{
if (sim.arr[sim.parr] != sim.state)
{
	sim.parr++;
	sim.parr &= 0x07;

	sim.arr[sim.parr] = sim.state;
}
sim.tm[sim.parr] = sim.time;

		if (!sim.time)
		{
			if (strlen(sim_st[sim.state].command))
				sim_str_put(&TXbuff1, (U08*)sim_st[sim.state].command, strlen((char*)sim_st[sim.state].command) );
		}
		else
			if (sim_st[sim.state].fnc_PROC)
			{
				//Если указана функция обработки
  				if (sim_st[sim.state].fnc_PROC())
	  			{
					sim.time = 0;
					continue;
				}
			}else
			if (!sim_str_get(&RXbuff1, sim.buf, 250 ))
  			{
/*
    if (sim.state ==4)
	{
		__asm__ volatile ("nop");
	}
*/
	    		if (strlen(sim_st[sim.state].ans_OK) && !memcmp (sim.buf, sim_st[sim.state].ans_OK, strlen(sim_st[sim.state].ans_OK)))
		    	{
  			   		if (!sim_st[sim.state].fnc_OK || sim_st[sim.state].fnc_OK()) // Если нет функции обработки положительного ответа
	  		    	{
  	  			  	  	sim.state = sim_st[sim.state].go_OK;
	  	  				sim.time = 0;
  	  	  			  	continue;
  	  			  	}
  			  	}else
	  			if (strlen(sim_st[sim.state].ans_ERR) && !memcmp (sim.buf, sim_st[sim.state].ans_ERR, strlen(sim_st[sim.state].ans_ERR)))
  	  			{
  	    			if (!sim_st[sim.state].fnc_ERR || sim_st[sim.state].fnc_ERR()) // Если нет функции обработки отрицательного ответа
	  	    		{
		  	      		sim.state = sim_st[sim.state].go_ERR;
			  	    	sim.time = 0;
				  	   	continue;
    				}
				}
			}
			  
  			if (sim_st[sim.state].time_out < sim.time++ )
	  		{
  	  		    if (!sim_st[sim.state].fnc_TIME || sim_st[sim.state].fnc_TIME())
  		            sim.state = sim_st[sim.state].go_time_out;
  	        	sim.time = 0;
	    	    continue;
			}
		break;
	}
}


BOOL Sim_Power_Off(void)
{
    bsp_led_Off(&LED_2);
    POWER_OFF();          //Выключить питание
    PWRKEY_ON();          //Включим POWER_ON
    bsp_sim_buff_flush(&TXbuff1);
    bsp_sim_buff_flush(&RXbuff1);
	return TRUE;
}


BOOL Sim_Power_On(void)
{
    //если нет питания

    if (power_mn.flags_power_off)
    {
		if (sim.power_off_count < 86400)
		{ 
        	bsp_led_Off(&LED_2);
        	return FALSE;
		}
    }

#ifdef NEW_LED
    bsp_led_Red(&LED_2);
#else
    bsp_led_Yellow(&LED_2);
#endif
    POWER_ON();           //Даём питание
    return TRUE;  
}

BOOL Sim_PKeyOff(void)
{
    //Выключение PWRKEY на 5 циклов
    PWRKEY_OFF();

    Sim_BitRateDEF();  // Установим скорость по умолчанию
 		return TRUE;
}


BOOL Sim_PKeyOn(void)
{
    bsp_sim_buff_flush(&RXbuff1);
    bsp_sim_buff_flush(&TXbuff1);    

    PWRKEY_ON();
    bsp_led_Off(&LED_2);

    return TRUE;
}


BOOL Sim_BitRateBACK (void)
{
	static const U32 bitrate[] = {9600UL, 19200UL, 38400UL, 57600UL, 115200UL};
	static U08 pbr = 4;

    U16 BitRate;
    // Перебираем скорости
    csp_util_UART_Baud_Calc(bitrate[pbr], UART_BRGH_SIXTEEN, csp_osc_FCY_Get(), &BitRate);
	if (++pbr >= sizeof(bitrate)/sizeof(U32))
		pbr = 0;
    csp_uart1_Baud_Set(BitRate);
 		return TRUE;
}


BOOL Sim_BitRateDEF (void)
{
    U16 BitRate;
    // Установить скорость поумолчанию
    csp_util_UART_Baud_Calc((U32)DEF_BR, UART_BRGH_SIXTEEN, csp_osc_FCY_Get(), &BitRate);
    csp_uart1_Baud_Set(BitRate);
 		return TRUE;
}


BOOL Sim_AT_GSN(void)
{
    U08 i;

    if (!sim_str_get(&RXbuff1, sim.buf, sizeof(sim.id)))
    {
        if (sim.buf[0] >= '0' && sim.buf[0] <= '9') // Нам нужны только цифры
        {
            i = 0;
            while (sim.buf[i] >= '0' && sim.buf[i] <= '9' && i < sizeof(sim.id))
            {
                sim.id[i] = sim.buf[i];
                i++;
            }

            //ошибка получения ID
            if (i < 14)
                return FALSE;

            while (i < sizeof(sim.id))
               sim.id[i++] = 0;

            sim.state = sim_st[sim.state].go_OK;
            return TRUE;
		}
    }
    return FALSE;
}



BOOL Sim_AT_CREG(void)
{
    if (!sim_str_get(&RXbuff1, sim.buf, 250))
    {
        if (sim.buf[9] == '1' || sim.buf[9] == '5')
        {
            // Регистрация в сети прошла
#ifndef NEW_LED
            bsp_led_Off(&LED_2);     // Погасить светодиод - регистрация в сети прошла
#endif
		  	    sim.state = sim_st[sim.state].go_OK;
            return TRUE;
        }
        bsp_led_Red(&LED_2);     // Ждём регистрацию в сети
    }
    
    if ((sim.time & 0x1F) == 0x1F)
        sim_str_put(&TXbuff1, (U08*)sim_st[sim.state].command, strlen((char*)sim_st[sim.state].command) );

    return FALSE;
}

BOOL Sim_SMS_Read(void)
{
    U16 t, i = 250;
    TN_UWORD sr;
     
    // читаем текст CMC и принимаем решения
    if (!sim_str_get(&RXbuff1, &sim.buf[250], 250))
    {
#ifndef NEW_LED
        bsp_led_Off(&LED_2);     // Погасить светодиод
#endif
        //А вдруг UNICODE ???
        for (t=250; t < 500; t++)
        {
            if (sim.buf[t] != 0 && sim.buf[t] != 4) 
                sim.buf[i++] = sim.buf[t];
            if (sim.buf[t]==0x0A)
                break;
        }
        
        // Ищем номер телефона с которого отправлено (если есть)
        // Преимущество у телефона с которого отправили СМС, а не у телефона указанного в SMS
        if (TNumber_Check(&sim.buf[2], 100, &sim.buf[200]) || TNumber_Check (&sim.buf[251], 100, &sim.buf[200]))
        {
        
            if (sim.buf[250] == '?' && (!memcmp(&sim.buf[200], AD_Device_Config.tel1, 12 ) || !memcmp(&sim.buf[200], AD_Device_Config.tel2,12 ) 
                                        || (AD_Device_Config.tel2[0]=='?' && AD_Device_Config.tel1[0]=='?'))) // Если запрос состояния прибора
            {
                sr = tn_sys_enter_critical();
                bsp_asens_ADC_Get(ASENS_AUX1_ID, (U32*)&sim.buf[400]);
                bsp_asens_ADC_Get(ASENS_AUX2_ID, (U32*)&sim.buf[404]);
                bsp_asens_ADC_Get(ASENS_AUX3_ID, (U32*)&sim.buf[408]);
                bsp_asens_ADC_Get(ASENS_AUX4_ID, (U32*)&sim.buf[412]);
                sim.buf[416] = bsp_rele_Get(&RELE_1);
                sim.buf[417] = bsp_rele_Get(&RELE_2);
                tn_sys_exit_critical(sr);                
              
              
                sprintf((char*)sim.buf, "IMEI=%s, C1=%lu, C2=%lu, C3=%lu, C4=%lu, R1=%d, R2=%d", sim.id, 
                                  *(U32*)&sim.buf[400], *((U32*)&sim.buf[404]),*((U32*)&sim.buf[408]),*((U32*)&sim.buf[412]), sim.buf[416], sim.buf[417]);

                sim.state = sim_st[sim.state].go_OK;
                return TRUE;
          
            }else
            if (sim.buf[250] == '!' || sim.buf[250] == 'Q')
            {
                if (sim.buf[251] == ' ') // Если пришли настройки прибора
                {
                    i=252; t=0;
                    
                    memset(&sim.buf[400], 0, sizeof(AD_Prov_Data));

                    while(sim.buf[i] == ' ' )
                        i++;
                    while(sim.buf[i] != ':' && t<sizeof(AD_Prov_Data.IP)-1)
                        ((AP_PROV_DATA*)(&sim.buf[400]))->IP[t++] = sim.buf[i++];
                    t=0;
	                i++;

                    while(sim.buf[i] != ' ' && t<sizeof(AD_Prov_Data.port)-1)
                        ((AP_PROV_DATA*)(&sim.buf[400]))->port[t++] = sim.buf[i++];
                    t=0;
                    while(sim.buf[i] == ' ' )
	                    i++;

                    while(sim.buf[i] != ' ' && t<sizeof(AD_Prov_Data.APN)-1)
                        ((AP_PROV_DATA*)(&sim.buf[400]))->APN[t++] = sim.buf[i++];
                    t=0;
                    while(sim.buf[i] == ' ' )
	                    i++;
					if (sim.buf[i] != '!' && sim.buf[i] != 'Q')
					{
                    	while(sim.buf[i] != ' ' && t<sizeof(AD_Prov_Data.name)-1)
	                       ((AP_PROV_DATA*)(&sim.buf[400]))->name[t++] = sim.buf[i++];
    	                t=0;
		                while(sim.buf[i] == ' ' )
	    	                i++;

            	        while(sim.buf[i] != ' ' && t<sizeof(AD_Prov_Data.pass)-1)
                	        ((AP_PROV_DATA*)(&sim.buf[400]))->pass[t++] = sim.buf[i++];
					}else // Если имя и пароль не заданы
					{
	                       ((AP_PROV_DATA*)(&sim.buf[400]))->name[t] = 0;
                	       ((AP_PROV_DATA*)(&sim.buf[400]))->pass[t] = 0;
					}

                    
                    // Проверим правильность сервера и то что сейчас настройки поумолчанию
                    if (CheckServer((AP_PROV_DATA*)&sim.buf[400]) && !memcmp(&AD_Prov_Data, &AC_Prov_Data, sizeof(AD_Prov_Data)) )
                    {
                        tn_mutex_lock(&PARAM_Lock, TN_WAIT_INFINITE);
                        memcpy(&AD_Prov_Data, &sim.buf[400], sizeof(AD_Prov_Data));
                        cl_Param_Save(&AP_Prov_Data);
						sim.GPRS_connect_count = 0xFFFFFFC0; // Для быстрого выхода на связь

                        memcpy(AD_Device_Config.tel1, &sim.buf[200], sizeof(AD_Device_Config.tel1));
                        if (!AD_Device_Config.send_period)
                            AD_Device_Config.send_period = 1;
                        cl_Param_Save(&AP_Device_Config);
                        tn_mutex_unlock(&PARAM_Lock);
                    }
                }

                sprintf((char*)sim.buf,"%s:%s %s %s %s tel.%s",AD_Prov_Data.IP, AD_Prov_Data.port, AD_Prov_Data.APN, AD_Prov_Data.name, AD_Prov_Data.pass, AD_Device_Config.tel1);

                sim.state = sim_st[sim.state].go_OK;
                return TRUE;
            }
        }
        sim.state = sim_st[sim.state].go_ERR;
        return TRUE;
    }
    return FALSE;
}


// Отправка SMS. Весь контекст для отправки должен быть в sim.buf, номер телефона в &sim.buf[200]
BOOL Send_SMS(void)
{
    if (sim.time == 1)
    {
         sim_str_get(&RXbuff1, &sim.buf[500], 10);
         sim_str_put(&TXbuff1,(U08*)"AT+CMGS=\"",9);
         sim_str_put(&TXbuff1,&sim.buf[200],12);
         sim_str_put2((U08*)"\"\r"); 
         return FALSE;
    }else
    {
        if (!sim_str_get(&RXbuff1, &sim.buf[250], 25))
        {
            if (!memcmp(&sim.buf[250], sim_st[sim.state].ans_OK, strlen(sim_st[sim.state].ans_OK)) )
            {
                sim.state = sim_st[sim.state].go_OK;
                return TRUE; 
            }
        }else
            if (!memcmp(&sim.buf[250], "> ", 2))
            {
                sim_str_put(&TXbuff1, sim.buf, strlen((char*)sim.buf));
                bsp_sim_char_put(&TXbuff1, 0x1A);
                sim.buf[250] = 0;
            }
        return FALSE;
    }
}


BOOL Sim_Led_on(void)
{
#ifndef NEW_LED
	if (sim.sim900_Level > 19)
    	bsp_led_Green(&LED_2);
	else if (sim.sim900_Level > 16)
	    bsp_led_Yellow(&LED_2);
	else
    	bsp_led_Red(&LED_2);
#else
	if (sim.sim900_Level > 19)
    	bsp_led_Green(&LED_2);
	else 
	    bsp_led_Yellow(&LED_2);
#endif    

    if ((AD_Device_Config.send_period && sim.GPRS_connect_count > (U32)AD_Device_Config.send_period*3600UL) || 
		sim.power_off_count > 86400)
    {
        sim.GPRS_connect_count = 0;
        sim.try_GPRS_connect = 0;
        sim.count_try_GPRS_connect = 0;
        sim.GPRS_need_connect = TRUE;
    }
        
    if (sim.GPRS_need_connect)
    {
		sim.power_off_count = 0;
        if (sim.try_GPRS_connect < 5)
        {
            if (120+sim.count_try_GPRS_connect >= (120UL<<sim.try_GPRS_connect))
            {
                sim.state = 31; // Подключаем GPRS   AT+CGATT=1\r\n
                return FALSE;
            }
        }else
        {
            sim.sim300_error = SIM_GSM_ERR;
            sim.state = sim_st[sim.state].go_ERR;
            sim.GPRS_need_connect = FALSE;
            return FALSE;
        }
    }

    return TRUE;
}


BOOL Sim_Led_Off(void)
{
#ifndef NEW_LED
    bsp_led_Off(&LED_2);     // Погасить светодиод - регистрация в сети прошла
#endif
    if (Send_SMS_to_master(sim.buf) )
    {
        // Нужно отправить Экстренное SMS
        sim.state = 44;
		sim.time = 0;
	    return FALSE;
        
    }    
    
    return TRUE;
}


BOOL Sim_Ring(void)
{
    // +CLCC: 1,1,4,0,0,"+79155064858",145,""

    if ((sim.buf[11] == '4') && (sim.buf[13] == '1')) // Если звонок входящий и вызов data
    {
        sim.autoriz = FALSE;    // Сбросить авторизацию
        sim.pack_rx = 0;
        bsp_led_Red(&LED_2);
        return TRUE;
    }
    
    sim.GPRS_need_connect = TRUE; // Был голосовой вызов - нужно подключаться
    sim.try_GPRS_connect = 0;
    sim.count_try_GPRS_connect = 0;
    
    bsp_led_Off(&LED_2);
    return FALSE;
}


BOOL Sim_AT_CSQ(void)
{
    // Преобразовываем и забираем значение уровня сигнала
    if (sim.buf[7] != ',' )
        sim.sim900_Level = (sim.buf[6]-'0')*10 + sim.buf[7]-'0';
    else
        sim.sim900_Level = sim.buf[6]-'0';
        
    
    if (power_mn.flags_power_off && !sms_send.Send_SMS && sim.power_off_count < 86400)
    {
        // Нужно выключить питание и спать
//        Sim_Power_Off();
        sim.state = 1; //Будем ждать питание 
    }
       
    return TRUE;
}


BOOL Sim_GPRS(void)
{
    if (sim.GPRS_need_connect)
    {
        sim.time = sim_st[sim.state].time_out;
        return FALSE;
    }
    return TRUE;
}

BOOL Sim_AT_CIPCSGP(void)
{
		if (sim.time == 1)
		{
         bsp_led_Green(&LED_2);
  		 sim.try_GPRS_connect++;
  		 sprintf((char*)sim.buf,"AT+CIPCSGP=1,\"%s\",\"%s\",\"%s\"\r\n", AD_Prov_Data.APN, AD_Prov_Data.name, AD_Prov_Data.pass);
  		 sim_str_put(&TXbuff1, sim.buf, strlen((char*)sim.buf) );
    }else
      if(!sim_str_get(&RXbuff1, sim.buf, 50))
      {
          if( !memcmp (sim.buf, sim_st[sim.state].ans_OK, strlen(sim_st[sim.state].ans_OK)))
          {
 			  	  	sim.state = sim_st[sim.state].go_OK;
 			  	  	return TRUE;
 			  	}else
          if( !memcmp (sim.buf, sim_st[sim.state].ans_ERR, strlen(sim_st[sim.state].ans_ERR)))
          {
 			  	  	sim.state = sim_st[sim.state].go_ERR;
 			  	  	return TRUE;
 			  	} 			  	
      }    
    return FALSE;
}


BOOL Sim_AT_CIPSTART(void)
{
		if (sim.time == 1)
		{
		 bsp_led_Off(&LED_2);

  		 sprintf((char*)sim.buf,"AT+CIPSTART=\"TCP\",\"%s\",\"%s\"\r\n", AD_Prov_Data.IP, AD_Prov_Data.port);
  		 sim_str_put(&TXbuff1, (U08*)sim.buf, strlen((char*)sim.buf) );
  		 sim.bSend = FALSE;
    }else
      if(!sim_str_get(&RXbuff1, sim.buf, 50))
      {
          if( !memcmp (sim.buf, sim_st[sim.state].ans_OK, strlen(sim_st[sim.state].ans_OK)))
          {
				sim.pack_rx = 0;
		        sim.autoriz = FALSE;    // Сбросить авторизацию
		  	  	sim.state = sim_st[sim.state].go_OK;
 		  	  	return TRUE;
 		  }
      }    
    return FALSE;
} 

BOOL Sim_GPRS_Conn(void) // Если коннект по GPRS
{
    sim.sim300_error = SIM_NOERR;
    if (sim.bSend)
    {
        if (bsp_sim_char_get(&RXbuff1) == '>')
        {  
		    	bsp_led_Red(&LED_2);
    		    sim_str_put(&TXbuff1, sim.buf, sim.buf[1]+sim.buf[2]*256);
    		    sim.bSend = FALSE;
    		    if (sim.GPRS_need_connect)
      			    sim.state = sim_st[sim.state].go_OK;
      			else
      			    sim.state = sim_st[sim.state].go_time_out;
 	      		sim.time = 0;
            return TRUE;
        }
    }else

    if (Sim_Get_Pack())  // Получен пакет - необходимо обработать
    {

        if (Packet_RS(sim.buf))
        {
	    	bsp_led_Green(&LED_2);
            sprintf((char*)&sim.buf[784],"AT+CIPSEND=%d\r",(U16)sim.buf[1]+sim.buf[2]*256);
            sim_str_put(&TXbuff1, &sim.buf[784], strlen((char*)&sim.buf[784]));
            sim.bSend = TRUE;
        }

    }else
    {
        if ( !memcmp (sim.buf, sim_st[sim.state].ans_ERR, strlen(sim_st[sim.state].ans_ERR)))
        {
      			sim.state = sim_st[sim.state].go_ERR;
 	      		sim.time = 0;
            return TRUE;
        }
    }
    return FALSE;
}



BOOL Sim_Connect (void) // Если коннект по CSD
{

    if (Sim_Get_Pack())  // Получен пакет - необходимо обработать
    {
    	bsp_led_Green(&LED_2);
        if (Packet_RS(sim.buf))
        {
		    sim_str_put(&TXbuff1, sim.buf, sim.buf[1]+sim.buf[2]*256);
            sim.time = 1;
	    	bsp_led_Off(&LED_2);
        }

    }else
    {
        if (!memcmp (sim.buf, sim_st[sim.state].ans_ERR, strlen(sim_st[sim.state].ans_ERR)))
        {
      			sim.state = sim_st[sim.state].go_ERR;
 	      		sim.time = 0;
            return TRUE;
        }
    }
    return FALSE;

}



void SimSendPack(void)
{

}


//Проверка на правильность настроек подключения к серверу
BOOL CheckServer(AP_PROV_DATA* AD_Prov_Data)
{
    U08 i = 0;
    U08 count =0;

    //Проверим, что IP адрес сервера верно задан
    while (AD_Prov_Data->IP[i] && i< sizeof(AD_Prov_Data->IP))
    {
      if (AD_Prov_Data->IP[i] == '.')
         count++;
      else
         if ((AD_Prov_Data->IP[i] < '0') ||(AD_Prov_Data->IP[i] > '9'))
            return FALSE;
      i++;
    };
    if (count != 3)
       return FALSE;

    i=0;
    //Проверим, что порт сервера верно задан
    while (AD_Prov_Data->port[i] && i < sizeof(AD_Prov_Data->port))
    {
      if ((AD_Prov_Data->port[i] < '0') || (AD_Prov_Data->port[i] > '9'))
         return FALSE;
      i++;
    };
    if (!i)
       return FALSE;

    //Проверим что заданы APN, имя и пароль
    if (!strlen(AD_Prov_Data->APN))
       return FALSE;

    return TRUE;
}

//Функция возвращает в буфере строку считанную из UART
//Если в буфере ничего нет - возвращается -1
//Если что-то было считано - возвращается количество считанных байт
//Если была считана строка символов в конце с '\r' - возвращается 0
S16 sim_str_get(UART_SBUF *buf, U08* str, U16 size)
{
    static S16 read_pos=0;

    //Если буфер пуст
    if (buf->stat.EMPTY == TRUE)
        return -1;

    csp_uart1_RXIRQ_DIS();

    while(size && (buf->stat.EMPTY == FALSE))
    {
        str[read_pos]=*(buf->pRD);
        buf->stat.FULL = FALSE;

        if (++buf->pRD >= (buf->buff + buf->size))
            buf->pRD = buf->buff;

        if (--buf->cnt == 0)
            buf->stat.EMPTY = TRUE;

        if (str[read_pos] == 0x0A) //Найден конец строки
        {
            if (read_pos)
            {
                str[read_pos]=0;
                read_pos = 0;        //Позицию в начало
                csp_uart1_RXIRQ_EN();
                return 0;
            }else
                continue;
        }

        if (str[read_pos] == 0x0D)
            continue;

        read_pos++;
        size--;
    };
    csp_uart1_RXIRQ_EN();

    if (size)
        return -1;

    read_pos = 0;//Позицию в начало
    return 0;
}


U16 sim_str_put2(U08* str)
{
    U16 size=0;
    while (str[size++] != '\r');

    return sim_str_put(&TXbuff1, str, size);
}


//Функция кладёт строку в буфер передачи UART. В случае успеха возвращает число положенных байт.
//В случае провала возвращает ноль.
U16 sim_str_put(UART_SBUF *buf, U08* str, U16 size)
{
    U16 i = 0;

    //Если буфер полон - вернём количество переданных байт
    if (buf->stat.FULL == TRUE)
        return 0;

    csp_uart1_TXIRQ_DIS();

    do
    {
      *(buf->pWR) = str[i++];

      if (++buf->cnt >= buf->size)
      {
          buf->stat.FULL = TRUE;
          csp_uart1_TXIRQ_EN();
          return i;
      }

      if (++buf->pWR >= (buf->buff + buf->size))
          buf->pWR = buf->buff;
    }while ( i < size);

    if (buf->stat.EMPTY == TRUE)
    {
        /* Начало передачи, если буфер пуст */
        buf->stat.EMPTY = FALSE;
        csp_uart1_Byte_Put(*(buf->pRD));
        buf->cnt--;

       if (++buf->pRD >= (buf->buff + buf->size))
           buf->pRD = buf->buff;
    }

    csp_uart1_TXIRQ_EN();
    return i;
}


U16 bsp_sim_char_put (UART_SBUF *buf, U08 ch)
{

    if (buf->stat.FULL == TRUE)
       return 0;
    csp_uart1_TXIRQ_DIS();
    *(buf->pWR) = ch;

    if (++buf->cnt >= buf->size)
        buf->stat.FULL = TRUE;

    if (++buf->pWR >= (buf->buff + buf->size))
        buf->pWR = buf->buff;

    if (buf->stat.EMPTY == TRUE)
    {
        /* Начало передачи, если буфер пуст */
        buf->stat.EMPTY = FALSE;
        csp_uart1_Byte_Put(*(buf->pRD));
        buf->cnt--;

        if (++buf->pRD >= (buf->buff + buf->size))
           buf->pRD = buf->buff;
    }

    csp_uart1_TXIRQ_EN();

    return 1;
}

S16 bsp_sim_char_get (UART_SBUF *buf)
{
    S16 ret = 0;

    if (buf->stat.EMPTY == TRUE)
       return -1;

    csp_uart1_RXIRQ_DIS();
    buf->stat.FULL = FALSE;
    ret = (S16)(*(buf->pRD));

    if (--buf->cnt == 0)
       buf->stat.EMPTY = TRUE;
    if (++buf->pRD >= (buf->buff + buf->size))
       buf->pRD = buf->buff;
    csp_uart1_RXIRQ_EN();

    return ret;
}

BOOL Sim_Get_Pack (void)
{
//    static U08 size = 0;
    S16 data;
    U16 size;
	static U08 count = 0;
    
    while (1)
    {
        data = bsp_sim_char_get(&RXbuff1);
        if (data == -1)
            break;

		count = 0;
        sim.buf[sim.pack_rx++] = (U08)data;
            
        if (!memcmp(sim.buf, "\r\nNO CARRI", 10) || !memcmp(sim.buf, "NO CARRIER", 10) || !memcmp(sim.buf, "+++", 3))
        {
             sim.pack_rx = 0;
             memcpy(sim.buf, "NO CARRIER" ,10);
             break; 
        }

		size=sim.buf[1]+sim.buf[2]*256;

        if (sim.pack_rx>=5 && size==sim.pack_rx)
        {
            sim.pack_rx = 0;
            data = CRC_math(sim.buf, size-2);
			if (data == sim.buf[size-2]+256*sim.buf[size-1])
                return TRUE;
        }
    }

	if (count++ > 15) // Даём пакету 3 секунды на приём - потом будем принимать с начала
		sim.pack_rx = 0;
   
    return FALSE;
}


void bsp_sim_TX_Service (void)
{
    CSPRETVAL berr;

    TXbuff1.stat.FULL = FALSE;

    for (;;)
    {
        if (TXbuff1.cnt)
        {
           berr = csp_uart1_Byte_Put(*(TXbuff1.pRD));

           if (berr == CSP_FULL)
              break;

           TXbuff1.cnt--;

           if (++TXbuff1.pRD >= (TXbuff1.buff + TXbuff1.size))
              TXbuff1.pRD = TXbuff1.buff;
        }
        else
        {
           TXbuff1.stat.EMPTY = TRUE;
           break;
        }
    }
}


void bsp_sim_RX_Service (void)
{
    S16 err;

    err = csp_uart1_Err_Get();

    if (err & UART_ERR_FERR)
    {
        RXbuff1.stat.ERR = TRUE;
        csp_uart1_Byte_Get();
    }

    if (err & UART_ERR_OERR)
    {
        RXbuff1.stat.ERR = TRUE;
        csp_uart1_Err_Clr(UART_ERR_OERR);
        return;
    }

    if (RXbuff1.stat.FULL)
    {
        RXbuff1.stat.OERR = TRUE;
        return;
    }

    RXbuff1.stat.OERR  = FALSE;

    /* Byte received */
    for (;;)
    {
        err = csp_uart1_Byte_Get();
  
        if (err == -1)
            break;

        *(RXbuff1.pWR) = err;
	    RXbuff1.stat.EMPTY = FALSE;

        if (++RXbuff1.cnt >= RXbuff1.size)
        {
            RXbuff1.stat.FULL = TRUE;
            return;
        }

        if (++RXbuff1.pWR >= (RXbuff1.buff + RXbuff1.size))
            RXbuff1.pWR = RXbuff1.buff;
    }
}

void bsp_sim_ERR_Service (void)
{
    S16 err;
    err = csp_uart1_Err_Get();

    if (err & UART_ERR_OERR)
    {
        RXbuff1.stat.ERR = TRUE;
        csp_uart1_Err_Clr(UART_ERR_OERR);
    }
    
    if (err & UART_ERR_FERR)
    {
        RXbuff1.stat.ERR = TRUE;
        csp_uart1_Byte_Get();
    }         
    
}

U08 Get_sim300_error(void)
{
    return sim.sim300_error;
}

U08 Get_sim900_SSI(void)
{
    return sim.sim900_Level;
}


/*
void FloatString (char* buf, float data)
{
    float del = 10000.0F;
    char ch;
    BOOL point = FALSE;

    if (data < 0.000000F)// Если меньше нуля
    {
        data = -data;
        *buf = '-';
        buf++;
    }

    do{

        ch = '0' + (U32)((float)data/(float)del)%10;

        if ( ch != '0' || point)
        {
            point = TRUE;
            *buf = ch;
            buf++;
        }

        if (del == 1.0)
        {
            if ( !point )
            {
                point = TRUE;
                *buf = '0'; // Если число меньше 1.0 - нужно записать нуль
                buf++;
            }
            *buf = '.';
            buf++;
        }

        del /= 10.0;
    }while (del >= 0.000001);

    *buf = 0;
}
*/


void MakeSMS(char* buf)
{
}


//Ищем в строке buf правильный номер телефона на протяжении N символов. Вытаскиваем этот номер в dest
BOOL TNumber_Check (U08 *buf, U08 N, U08 *dest)
{
   U08 n=0, i=0;

	do
	{
    	if (buf[n] == '+' && buf[n+1] == '7') 
    	{
        	i = 1;
        	while (buf[n+i] >= '0' && buf[n+i] <= '9' && i < 12)
        	{
            	dest[i] = buf[n+i];
              	i++;
         	}
    	}else if (buf[n] == '8')
        {
           	i = 2;
           	dest[1] = '7';
           	while (buf[n+i-1] >= '0' && buf[n+i-1] <= '9' && i < 12)
           	{
               	dest[i] = buf[n+i-1];
               	i++;
           	}
        }
      
		if (i == 12 )
      	{
        	dest[0] = '+';
        	dest[12] = 0;
          	return TRUE;
      	}
      	n++;
	} while (--N) ;
   
   return FALSE;
}



//Отправка SMS на мастер-телефон
BOOL Send_SMS_to_master(U08 *buf)
{
    U16 i = 1;
    U08 m = 0;
    
    if (sms_send.Send_SMS && (AD_Device_Config.tel1[0] == '+' || AD_Device_Config.tel2[0] == '+') )
    {   

		buf[0] = 0;
        // Есть повод для сообщения и есть номер телефона
        while (i < 0x2000)
        {
            if (sms_send.Send_SMS & i)
			{
				if (strlen((char*)buf))
                	sprintf((char*)buf, "%s %s", buf, Message[m]);
				else
                	sprintf((char*)buf, Message[m]);
			}

            i<<=1;
            m++;
        }
        if (AD_Device_Config.tel1[0] == '+')
            memcpy (&sim.buf[200], AD_Device_Config.tel1, 12);
        else
            memcpy (&sim.buf[200], AD_Device_Config.tel2, 12);
            
        sms_send.Send_SMS = 0;
        return TRUE;
    }

    sms_send.Send_SMS = 0;
    return FALSE;          
}
