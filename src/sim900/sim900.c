/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include <string.h>
#include <stdio.h>
#include "../tnkernel/tn.h"
#include "../stellaris.h"
#include "../pins.h"
#include "../pref.h"
#include "../utime.h"
#include "../version.h"
#include "../multicast/cmd_ch.h"
#include "../adcc.h"
#include "../memory/memory.h"
#include "../dk/dk.h"
#include "../gps/gps.h"
#include "../crc32.h"
#include "sim900.h"


#include "../debug/debug.h"

#define SYSCTL_PERIPH_UART          SYSCTL_PERIPH_UART2
#define INT_UART                    INT_UART2
#define UART_BASE                   UART2_BASE
#define UART_SPEED                  115200
#define UART_BUF_SZ                 256

#define SIM900_REFRESH_INTERVAL     200

#define PWRKEY_OFF()     pin_off(OPIN_SIM900_ON)
#define PWRKEY_ON()      pin_on(OPIN_SIM900_ON)

static TN_TCB task_SIM900_tcb;
#pragma location = ".noinit"
#pragma data_alignment=8
static unsigned int task_SIM900_stk[TASK_SIM900_STK_SZ];
static void task_SIM900_func(void* param);

// local variables

//static unsigned char    g_rx_buf[UART_BUF_SZ], g_tx_buf[UART_BUF_SZ];
//static unsigned char*   g_tx_buf_ptr;
//static unsigned         g_rx_buf_len, g_tx_buf_len;

UART_SBUF   TXbuff, RXbuff;
U08         TXbuff_mem[SIM_TX_BUFF_SIZE];
U08         RXbuff_mem[SIM_RX_BUFF_SIZE];

SIM900 sim;
void bsp_sim_buff_flush (UART_SBUF *buf);
static BOOL sim_tx_pkt_snd_one();


unsigned char regim;
unsigned char regim_new;

PROJ_SMS_STRUCT  PROJ_SMS;

void SIM900_init()
{

    dbg_printf("Initializing SIM900 UART...");
    // UART 2
    // PINS 18 (PG1) and 19 (PG0)
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART);

    MAP_GPIOPinConfigure(GPIO_PG0_U2RX);
    MAP_GPIOPinConfigure(GPIO_PG1_U2TX);
    MAP_GPIOPinTypeUART(GPIO_PORTG_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    MAP_UARTConfigSetExpClk(UART_BASE, MAP_SysCtlClockGet(), UART_SPEED, UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);
    MAP_UARTDisable(UART_BASE);
    MAP_UARTTxIntModeSet(UART_BASE, UART_TXINT_MODE_EOT);
    MAP_UARTIntEnable(UART_BASE, UART_INT_RX /*| UART_INT_TX*/);
    MAP_IntEnable(INT_UART);
    MAP_UARTEnable(UART_BASE);
    MAP_UARTFIFODisable(UART_BASE);


    if (tn_task_create(&task_SIM900_tcb, &task_SIM900_func, TASK_SIM900_PRI,
        &task_SIM900_stk[TASK_SIM900_STK_SZ - 1], TASK_SIM900_STK_SZ, 0,
        TN_TASK_START_ON_CREATION) != TERR_NO_ERR)
    {
        dbg_puts("tn_task_create(&task_SIM900_tcb) error");
        goto err;
    }

    dbg_puts("[done]");

    return;

err:
    dbg_trace();
    tn_halt();
}
//--------------------------------------------------------------------------
void uart2_int_handler()
{
    unsigned long const ist = MAP_UARTIntStatus(UART_BASE, TRUE);

    if (ist & UART_INT_RX)
    {
        MAP_UARTIntClear(UART_BASE, UART_INT_RX);

        for (;;)
        {
            long const err = MAP_UARTCharGetNonBlocking(UART_BASE);

            if (err == -1)
                break;

            *(RXbuff.pWR) = err;
	    RXbuff.stat.EMPTY = FALSE;

            if (++RXbuff.cnt >= RXbuff.size)
            {
                RXbuff.stat.FULL = TRUE;
                break;
            }

            if (++RXbuff.pWR >= (RXbuff.buff + RXbuff.size))
                RXbuff.pWR = RXbuff.buff;
        }
    }

    if (ist & UART_INT_TX)
    {
        MAP_UARTIntClear(UART_BASE, UART_INT_TX);
        sim_tx_pkt_snd_one();

    }
}

void bsp_sim_Handle (void);
//-----------------------------------------------------------------------------
static void task_SIM900_func(void* param)
{
    TXbuff.buff = TXbuff_mem;
    TXbuff.size = sizeof(TXbuff_mem);
    bsp_sim_buff_flush(&TXbuff);

    RXbuff.buff = RXbuff_mem;
    RXbuff.size = sizeof(RXbuff_mem);
    bsp_sim_buff_flush(&RXbuff);

    sim.reboot = 0;
    sim.sinhro = 0;
    sim.CSD_try_count = 0;
    sim.timer_CSD = 300;
    sim.Send_SMS = 0;
    sim.led_flash = 0;
    sim.call_period = 0;
    sim.connect = 0;

    for (;;)
    {
        tn_task_sleep(SIM900_REFRESH_INTERVAL);
        bsp_sim_Handle ();


        if(sim.timer_CSD++ > 301)
            sim.timer_CSD = 301;

        //if (sim.connect && PROJ_SMS.sms_config.mod == 3 && PROJ_SMS.sms_config.per && sim.call_period++ > PROJ_SMS.sms_config.per*15)
        if (sim.connect && sim.call_period++ > 90)
        {
           sim.connect = FALSE;
           regim_new = 0;
           sim.sinhro = 1;
        }


        if (sim.reboot)
        {
            if(sim.reboot++ > 25)
                tn_reset();
        }
    }
}
//-----------------------------------------------------------------------------

static BOOL sim_tx_pkt_snd_one()
{

     if (!TXbuff.cnt)
     {
         TXbuff.stat.EMPTY = TRUE;
         MAP_UARTIntDisable(UART_BASE, UART_INT_TX);
         return FALSE;
     }

     MAP_UARTCharPutNonBlocking(UART_BASE, *(TXbuff.pRD));
     TXbuff.cnt--;

     if (++TXbuff.pRD >= (TXbuff.buff + TXbuff.size))
         TXbuff.pRD = TXbuff.buff;

     MAP_UARTIntEnable(UART_BASE, UART_INT_TX);
     return TRUE;
}



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
BOOL Sim_AT_CSD_Call(void);
BOOL Sim_Busy(void);

S16 sim_str_get(UART_SBUF *buf, U08* str, U16 size);
U16 bsp_sim_char_put (UART_SBUF *buf, U08 ch);
U16 sim_str_put2(U08* str);
U16 sim_str_put(UART_SBUF *buf, U08* str, U16 size);
S16 bsp_sim_char_get (UART_SBUF *buf);
BOOL TNumber_Check (U08 *buf, U08 N, U08 *dest);
BOOL Sim_Get_Pack(void);




const SIM_COMMAND sim_st[]=
{

    //  command               ans_OK             go_OK     ans_ERR               go_ERR   time  go_time  fnc_PROC         fnc_OK             fnc_ERR       fnc_TIME
/*00*/    {"AT+CPOWD=1\r\n",      "NORMAL",             1,     "",               0xFF,     10,      1,   0,               Sim_Power_Off,    0,            Sim_Power_Off     }, // 00 - Программное выключение SIM
/*01*/    {"",                    "",                   0,     "",               0xFF,      5,      2,   0,               0,                0,            Sim_Power_On      }, // 01 - Подаём питание
/*02*/    {"",                    "",                0xFF,     "",               0xFF,      5,      3,   0,               0,                0,            Sim_PKeyOff       }, // 02 - Выключаем Power_Key
/*03*/    {"",                    "RDY",                4,     "",               0xFF,     15,      4,   0,               Sim_PKeyOn,       0,            Sim_PKeyOn        }, // 03 - Выключаем Power_Key

/*04*/    {"ATE0\r\n",            "OK",                 7,     "RDY",               4,     10,      5,   0,               0,                0,            Sim_BitRateBACK   }, // 04 - Выключить эхо

/*05*/    {TEST(AT_,DEF_BR,AT_end),"OK",                6,     "",               0xFF,     10,      1,   0,               Sim_BitRateDEF,   0,            0                 }, // 05 - Установить скорость

/*06*/    {"AT&W\r\n",            "OK",                 4,     "",               0xFF,     10,      0,   0,               0,                0,            0,                }, // 06 - Сохранить настройки
/*07*/    {"AT+CMGF=1\r\n",       "OK",                 8,     "",               0xFF,     10,      0,   0,               0,                0,            0,                }, // 07 - настроить формат сообщений
/*08*/    {"AT+CPIN?\r\n",        "+CPIN: READY",      10,     "+CPIN: SIM PIN",    9,     10,      0,   0,               0,                0,            0,                }, // 08 - запрос готовности SIM-карты

/*09*/    {"AT+CPIN=0000\r\n",    "OK",                 8,     "",               0xFF,     20,      0,   0,               0,                0,            0,                }, // 09 - ввести PIN-код

/*10*/    {"AT+GSN\r\n",          "",                  11,     "",	         0xFF,     20,      0,   Sim_AT_GSN,      0,                0,            0,                }, // 10 - Запрос IMEI
/*11*/    {"",                    "OK",                12,     "",               0xFF,     20,      0,   0,               0,                0,            0,                }, // 11 - Результат IMEI

// Проверка регистрации в сети
/*12*/    {"AT+CREG?\r\n",        "+CREG:",            13,     "",               0xFF,   0xF0,      0,   Sim_AT_CREG,     0,                0,            0,                }, // 12 - Запрос готовности сети
/*13*/    {"",                    "OK",                14,     "",	         0xFF,     10,      0,   0,               0,                0,            0,                }, // 13 - Подтвердить, что сеть готова

//Проверка SMS
/*14*/    {"AT+CMGR=1\r\n",       "+CMGR:",            15,     "OK",               16,     20,     15,   0,               0,                Sim_Led_Off,  0,                }, // 14 - Если есть SMS номер 1
/*15*/    {"",                    "OK",                40,     "",	           17,     20,     17,   Sim_SMS_Read,    0,                0,            0,                }, // 15 - Ждём ОК
/*16*/    {"",                    "RING",              23,     "",	            0,     50,     18,   0,               0,                0,            Sim_Led_on        }, // 16 - Ждём звонка
/*17*/    {"AT+CMGD=1,0\r\n",     "OK",                18,     "",	         0xFF,     20,      0,   0,               0,                0,            0,                }, // 17 - Удаляем СМС и идём дальше

/*18*/    {"AT+CMGR=2\r\n",       "+CMGR:",            19,     "OK",  	           20,     20,     19,   0,               0,                Sim_Led_Off,  0,                }, // 18 - Если есть SMS номер 2
/*19*/    {"",                    "OK",                41,     "",	           21,     20,     21,   Sim_SMS_Read,    0,                0,            0,                }, // 19 - Ждём ОК
/*20*/    {"",                    "RING",              23,     "",	            0,     50,     22,   0,               0,                0,            Sim_Led_on        }, // 20 - Ждём звонка
/*21*/    {"AT+CMGD=2,0\r\n",     "OK",                23,     "",	         0xFF,     20,      0,   0,               0,                0,            0,                }, // 21 - Удаляем СМС и идём дальше

//Запрос уровня сигнала
/*22*/    {"AT+CSQ\r\n",          "+CSQ:",             11,     "+CME ERROR",       11,     20,     11,   0,               Sim_AT_CSQ,       0,            0,                }, // 22 - запросить уровень сигнала

// Запрос типа звонка
/*23*/    {"AT+CLCC\r\n",         "+CLCC:",            24,     "",  	         0xFF,     20,      0,   0,               0,                0,            0,                }, // 23 - Определить тип звонка
/*24*/    {"",                    "OK",                25,     "RING",             26,     50,     26,   0,               Sim_Ring,         0,            0,                }, // 24 - Подтвердить, что сеть готова

/*25*/    {"ATA\r\n",             "",                0xFF,     "ERROR",            14,     20,     27,   0,               0,                0,            0,                }, // 25 - поднять трубку
/*26*/    {"ATH\r\n",             "OK",                14,     "ERROR",            14,     20,     31,   0,               Sim_GPRS,         0,            0,                }, // 26 - положить трубку

/*27*/    {"",                    "CONNECT",           28,     "NO CARRIER",       26,    250,     14,   0,               0,                0,            0,                }, // 27 - Дождаться установки соединения
/*28*/    {"",                    "",                0xFF,     "NO CARRIER",       26,    250,     29,   Sim_Connect,     0,                0,            0,                }, // 28 - Общение по CSD

/*29*/    {"+++",                 "OK",                26,     "",               0xFF,     10,     25,   0,               0,                0,            0,                }, // 29 - Перейти в коммандный режим

/*30*/    {"",                    "OK",                66,     "",                  0,     20,     66,   0,               0,                0,            0,                }, // 30 - Подтвердить, что сеть готова

// Подключаем GPRS
/*31*/    {"AT+CGATT=1\r\n",      "OK",                32,     "ERROR",            14,    100,      0,   0,               0,                0,            0,                }, // 31
/*32*/    {"",                    "OK",                33,     "ERROR",            38,    100,      0,   Sim_AT_CIPCSGP,  0,                0,            0,                }, // 32
/*33*/    {"AT+CIPHEAD=0\r\n",    "OK",                34,     "ERROR",            14,     20,      0,   0,               0,                0,            0,                }, // 33
/*34*/    {"AT+CIPMUX=0\r\n",     "OK",                35,     "ERROR",            14,     20,      0,   0,               0,                0,            0,                }, // 34
/*35*/    {"AT+CIPMODE=0\r\n",    "OK",                36,     "ERROR",            14,     20,      0,   0,               0,                0,            0,                }, // 35
/*36*/    {"",                    "OK",                42,     "ERROR",            14,    100,      0,   Sim_AT_CIPSTART, 0,                0,            0,                }, // 36
/*37*/    {"",                    "",                  43,     "ERROR",            37,     50,     38,   Sim_GPRS_Conn,   0,                0,            0,                }, // 37

// Отключаем GPRS
/*38*/    {"AT+CIPCLOSE\r\n",     "CLOSED",            39,     "ERROR",            39,     50,     39,   0,               0,                0,            0,                }, // 38
/*39*/    {"AT+CIPSHUT\r\n",      "SHUT OK",           14,     "ERROR",            14,     50,     14,   0,               0,                0,            0,                }, // 39

// Отправка SMS
/*40*/    {"",                    "+CMGS",             17,     "+CMS ERROR:",      12,     50,     12,   Send_SMS,        0,                0,            0,                }, // 40 - Отправка ответа на SMS1
/*41*/    {"",                    "+CMGS",             21,     "+CMS ERROR:",      12,     50,     12,   Send_SMS,        0,                0,            0,                }, // 41 - Отправка ответа на SMS2
/*42*/    {"",                    "CONNECT OK",        37,     "ERROR",            37,     50,     38,   0,               0,                0,            0,                }, // 42
/*43*/    {"",                    "SEND OK",           37,     "ERROR",            37,     50,     38,   0,               0,                0,            0,                }, // 43

/*44*/    {"",                    "+CMGS",             22,     "+CMS ERROR:",      12,     50,     12,   Send_SMS,        0,                0,            0,                }, // 44 - Отправка Экстеной SMS

/*45*/    {"",                    "",                  27,     "ERROR",            45,     50,     27,   Sim_AT_CSD_Call, 0,                0,            0,                },  // Подключаемся по CSD
/*46*/    {"",                    "BUSY",              26,     "NO CARRIER",       26,     50,     26,   0,               Sim_Busy,         0,            0,                },
};


char arr[20];
char ts[20];
char ps = 0;
////////////////////////////////////////
void bsp_sim_Handle (void)
{

    while (1)
    {

      if (arr[ps] != sim.state)
      {
          ps++;
          arr[ps] = sim.state;
          if (ps==19)
            ps = 0;
      }else
        ts[ps] = sim.time;

 /*
   if (sim.state == 5)
     ps++;
 */

	if (!sim.time)
	{
             if (strlen(sim_st[sim.state].command))
                 sim_str_put(&TXbuff, (U08*)sim_st[sim.state].command, strlen((char*)sim_st[sim.state].command) );
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
	if (!sim_str_get(&RXbuff, sim.buf, 250 ))
  	{
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


BOOL Sim_Power_Off(void)
{
    PWRKEY_ON();          //Включим POWER_ON
    bsp_sim_buff_flush(&TXbuff);
    bsp_sim_buff_flush(&RXbuff);
    return TRUE;
}


BOOL Sim_Power_On(void)
{
    //если нет питания
    /*if (sim.flags_power_off)
    {
	if (sim.power_off_count < 86400)
	{
          return FALSE;
	}
    }
    */
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
    bsp_sim_buff_flush(&RXbuff);
    bsp_sim_buff_flush(&TXbuff);

    PWRKEY_ON();
    return TRUE;
}


BOOL Sim_BitRateBACK (void)
{
    static const U32 bitrate[] = {9600UL, 19200UL, 38400UL, 57600UL, 115200UL};
    static U08 pbr = 4;
/*
    MAP_UARTDisable(UART_BASE);
    MAP_UARTConfigSetExpClk(UART_BASE, MAP_SysCtlClockGet(), bitrate[pbr], UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);
    MAP_UARTEnable(UART_BASE);
*/
    if (++pbr >= sizeof(bitrate)/sizeof(U32))
	pbr = 0;
    return TRUE;
}


BOOL Sim_BitRateDEF (void)
{
    // Установить скорость поумолчанию
  /*
    MAP_UARTDisable(UART_BASE);
    MAP_UARTConfigSetExpClk(UART_BASE, MAP_SysCtlClockGet(), DEF_BR, UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);
    MAP_UARTEnable(UART_BASE);
*/
  return TRUE;
}


BOOL Sim_AT_GSN(void)
{
    U08 i;

    if (!sim_str_get(&RXbuff, sim.buf, sizeof(sim.id)))
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
    if (!sim_str_get(&RXbuff, sim.buf, 250))
    {
        if (sim.buf[9] == '1' || sim.buf[9] == '5')
        {
            // Регистрация в сети прошла
            sim.state = sim_st[sim.state].go_OK;
            return TRUE;
        }
        // Ждём регистрацию в сети
    }

    if ((sim.time & 0x1F) == 0x1F)
        sim_str_put(&TXbuff, (U08*)sim_st[sim.state].command, strlen((char*)sim_st[sim.state].command) );

    return FALSE;
}



BOOL Sim_SMS_Read(void)
{
    SMS_CONFIG config;
    BOOL result;

    memset(&sim.buf[250],0,250);
    // читаем текст CMC и принимаем решения
    if (!sim_str_get(&RXbuff, &sim.buf[250], 250))
    {
        if (TNumber_Check(&sim.buf[2], 100, &sim.buf[200]))
        {
            str_from_unicod((char*)&sim.buf[250]);
            str_to_hight((char*)&sim.buf[250]);

            Init_sms_command(&config);
            result = parse_sms(&sim.buf[250], &config);

            // Проверка пароля
            if (!memcmp(config.pass, PROJ_SMS.sms_config.pass, 4))
            {
                if(sim.led_flash<30)
                    sim.led_flash += 30;

                if (result)
                    snprintf((char*)sim.buf, 10, "OK ");
                else
                    snprintf((char*)sim.buf, 10, "ERROR ");


                if (config.read)
                {
                    config.read = 0;
                    Get_config_list((char*)&sim.buf[strlen((char const*)sim.buf)]);
                }

                if (config.res)
                {
                    config.res = 0;
                    sim.reboot = 1;
                }


                // Записать полученные настройки во FLASH
                if (config.per<30)
                    config.per = 0;
                //flash_wr(FLASH_PROGRAM, (unsigned long)&(((TPROJECT*)0)->sms_config), (unsigned char*)&config, sizeof(config));

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
         sim_str_get(&RXbuff, &sim.buf[500], 10);
         sim_str_put(&TXbuff,(U08*)"AT+CMGS=\"",9);
         sim_str_put(&TXbuff,&sim.buf[200],12);
         sim_str_put2((U08*)"\"\r");
         return FALSE;
    }else
    {
        if (!sim_str_get(&RXbuff, &sim.buf[250], 25))
        {

            if (!memcmp(&sim.buf[250], sim_st[sim.state].ans_OK, strlen(sim_st[sim.state].ans_OK)) )
            {
                sim.state = sim_st[sim.state].go_OK;
                return TRUE;
            }
        }else
            if (!memcmp(&sim.buf[250], "> ", 2))
            {
                sim_str_put(&TXbuff, sim.buf, strlen((char*)sim.buf));
                bsp_sim_char_put(&TXbuff, 0x1A);       //?????
                sim.buf[250] = 0;
            }
        return FALSE;
    }
}

//------------------------------------------------------------------------------
BOOL Sim_Led_on(void)
{

    if (PROJ_SMS.sms_config.mod == 3 && PROJ_SMS.sms_config.master == 1 && sim.sinhro == 0
    //    && ((Get_GPS_valid() && regim_new==2)|| regim_new!=2) && regim != regim_new && sim.timer_CSD > 300)
        && (( regim_new==2)|| regim_new!=2) && regim != regim_new && sim.timer_CSD > 300)
    {
        sim.timer_CSD = 0;
        if (sim.CSD_try_count++ >= 4)
        {
            regim_new=regim;
            return TRUE;
        }else
        {
            sim.state = 45; // Звоним по CSD
            sim.pack_rx = 0;
            return FALSE;
        }
    }


    if (sim.connect && PROJ_SMS.sms_config.mod == 3 && PROJ_SMS.sms_config.master == 0 && PROJ_SMS.sms_config.per > 0 && sim.call_period > PROJ_SMS.sms_config.per*5)
    {
        sim.state = 45; // Звоним по CSD
        return FALSE;
    }

    /*
    if (sim.power_off_count > 86400)
    {
        sim.GPRS_connect_count = 0;
        sim.try_GPRS_connect = 0;
        sim.count_try_GPRS_connect = 0;
        sim.GPRS_need_connect = TRUE;
    }
    */
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
//------------------------------------------------------------------------------

BOOL Sim_Led_Off(void)
{

    static U08 flag=1, flag_now=1;
    U08 Unow;

    //Unow = Get_real_chanel(6)/10;

    if (Unow < PROJ_SMS.sms_config.umin)
        flag_now = 1;
    if (Unow >= PROJ_SMS.sms_config.umax)
    {
        flag_now = 0;
        flag = 0;
    }

    if (flag != flag_now && flag_now==1)
    {
        flag = flag_now;
        sim.Send_SMS = 1;
    }


    if (sim.Send_SMS && Send_SMS_to_master(sim.buf, &sim.buf[200]) )
    {
        // Нужно отправить Экстренное SMS
        sim.state = 44;
        sim.time = 0;
        sim.Send_SMS = 0;
        return FALSE;
    }

    return TRUE;
}


BOOL Sim_Ring(void)
{
    // +CLCC: 1,1,4,0,0,"+79155064858",145,""

    if (sim.buf[11] == '4' && sim.buf[13] == '1' // Если звонок входящий и вызов data
        && PROJ_SMS.sms_config.mod == 3 && PROJ_SMS.sms_config.master == 0)
    {
        sim.autoriz = FALSE;    // Сбросить авторизацию
        sim.pack_rx = 0;
        return TRUE;
    }

    sim.call_period = 0;
    sim.time = sim_st[sim.state].time_out+1;
/*
    sim.GPRS_need_connect = TRUE; // Был голосовой вызов - нужно подключаться
    sim.try_GPRS_connect = 0;
    sim.count_try_GPRS_connect = 0;
*/
    return FALSE;
}


BOOL Sim_AT_CSQ(void)
{
    // Преобразовываем и забираем значение уровня сигнала
    if (sim.buf[7] != ',' )
        sim.sim900_Level = (sim.buf[6]-'0')*10 + sim.buf[7]-'0';
    else
        sim.sim900_Level = sim.buf[6]-'0';

    /*
    if ( sim.power_off_count < 86400)
    {
        // Нужно выключить питание и спать
        sim.state = 1; //Будем ждать питание
    }
     */
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
  		 sim.try_GPRS_connect++;
  		 sprintf((char*)sim.buf,"AT+CIPCSGP=1,\"%s\",\"%s\",\"%s\"\r\n","APN", "login", "password");
  		 sim_str_put(&TXbuff, sim.buf, strlen((char*)sim.buf) );
    }else
      if(!sim_str_get(&RXbuff, sim.buf, 50))
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
        sprintf((char*)sim.buf,"AT+CIPSTART=\"TCP\",\"%s\",\"%s\"\r\n", "192.168.2.1", "9999");
        sim_str_put(&TXbuff, (U08*)sim.buf, strlen((char*)sim.buf) );
        sim.bSend = FALSE;
    }else
      if(!sim_str_get(&RXbuff, sim.buf, 50))
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


BOOL Sim_AT_CSD_Call(void)
{
    if (sim.time == 1)
    {
        sprintf((char*)sim.buf,"ATD%s\r", PROJ_SMS.sms_config.tel);
        sim_str_put(&TXbuff, (U08*)sim.buf, strlen((char*)sim.buf) );

        if (PROJ_SMS.sms_config.mod == 3 && PROJ_SMS.sms_config.master == 1)
            sim.state = sim_st[sim.state].go_OK;
        else
            sim.state = 47;
    }
    return FALSE;
}

BOOL Sim_Busy(void)
{
    sim.call_period = 0;
    return TRUE;
}


BOOL Sim_GPRS_Conn(void) // Если коннект по GPRS
{
    sim.sim300_error = SIM_NOERR;
    if (sim.bSend)
    {
        if (bsp_sim_char_get(&RXbuff) == '>')
        {
    		    sim_str_put(&TXbuff, sim.buf, sim.buf[1]+sim.buf[2]*256);
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
            sprintf((char*)&sim.buf[784],"AT+CIPSEND=%d\r",(U16)sim.buf[1]+sim.buf[2]*256);
            sim_str_put(&TXbuff, &sim.buf[784], strlen((char*)&sim.buf[784]));
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

    if (sim.time == 1 && PROJ_SMS.sms_config.mod ==3 && PROJ_SMS.sms_config.master == 1)
    {
        //начинаем обмен
        sim.CSD_try_count = 0;
        if(sim.led_flash<30)
          sim.led_flash += 30;
        sim.buf[0] = 0xFF;
        Packet_RS(sim.buf);
        sim_str_put(&TXbuff, sim.buf, sim.buf[1]+sim.buf[2]*256);
        return FALSE;
    }

    if (Sim_Get_Pack())  // Получен пакет - необходимо обработать
    {

        if(sim.led_flash<30)
          sim.led_flash += 30;
        if (Packet_RS(sim.buf))
        {
            sim_str_put(&TXbuff, sim.buf, sim.buf[1]+sim.buf[2]*256);
            sim.time = 2;
        }else
        {
            //Закрыть соединение
            sim.state = sim_st[sim.state].go_time_out;
            sim.time = 0;
            return TRUE;

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
  /*
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
*/
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

    MAP_UARTIntDisable(UART_BASE, UART_INT_RX);

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
                str[++read_pos]=0x0A;
                read_pos = 0;        //Позицию в начало
                MAP_UARTIntEnable(UART_BASE, UART_INT_RX);
                return 0;
            }else
                continue;
        }

        if (str[read_pos] == 0x0D)
            continue;

        read_pos++;
        size--;
    };
    MAP_UARTIntEnable(UART_BASE, UART_INT_RX);

    if (size)
        return -1;

    read_pos = 0;//Позицию в начало
    return 0;
}


U16 sim_str_put2(U08* str)
{
    U16 size=0;
    while (str[size++] != '\r');

    return sim_str_put(&TXbuff, str, size);
}


//Функция кладёт строку в буфер передачи UART. В случае успеха возвращает число положенных байт.
//В случае провала возвращает ноль.
U16 sim_str_put(UART_SBUF *buf, U08* str, U16 size)
{
    U16 i = 0;

    //Если буфер полон - вернём количество переданных байт
    if (buf->stat.FULL == TRUE)
        return 0;

    MAP_UARTIntDisable(UART_BASE, UART_INT_TX);
    do
    {
      *(buf->pWR) = str[i++];

      if (++buf->cnt >= buf->size)
      {
          buf->stat.FULL = TRUE;
          MAP_UARTIntEnable(UART_BASE, UART_INT_TX);
          return i;
      }

      if (++buf->pWR >= (buf->buff + buf->size))
          buf->pWR = buf->buff;
    }while ( i < size);

    if (buf->stat.EMPTY == TRUE)
    {
        /* Начало передачи, если буфер пуст */
        buf->stat.EMPTY = FALSE;
        MAP_UARTCharPutNonBlocking(UART_BASE, *(buf->pRD));
        buf->cnt--;

       if (++buf->pRD >= (buf->buff + buf->size))
           buf->pRD = buf->buff;
    }

    MAP_UARTIntEnable(UART_BASE, UART_INT_TX);
    return i;
}


U16 bsp_sim_char_put (UART_SBUF *buf, U08 ch)
{

    if (buf->stat.FULL == TRUE)
       return 0;

    MAP_UARTIntDisable(UART_BASE, UART_INT_TX);
    *(buf->pWR) = ch;

    if (++buf->cnt >= buf->size)
        buf->stat.FULL = TRUE;

    if (++buf->pWR >= (buf->buff + buf->size))
        buf->pWR = buf->buff;

    if (buf->stat.EMPTY == TRUE)
    {
        /* Начало передачи, если буфер пуст */
        buf->stat.EMPTY = FALSE;
        MAP_UARTCharPutNonBlocking(UART_BASE, *(buf->pRD));
        buf->cnt--;

        if (++buf->pRD >= (buf->buff + buf->size))
           buf->pRD = buf->buff;
    }

    MAP_UARTIntEnable(UART_BASE, UART_INT_TX);

    return 1;
}

S16 bsp_sim_char_get (UART_SBUF *buf)
{
    S16 ret = 0;

    if (buf->stat.EMPTY == TRUE)
       return -1;

    MAP_UARTIntDisable(UART_BASE, UART_INT_RX);
    buf->stat.FULL = FALSE;
    ret = (S16)(*(buf->pRD));

    if (--buf->cnt == 0)
       buf->stat.EMPTY = TRUE;
    if (++buf->pRD >= (buf->buff + buf->size))
       buf->pRD = buf->buff;
    MAP_UARTIntEnable(UART_BASE, UART_INT_RX);

    return ret;
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


BOOL Sim_Get_Pack (void)
{
    unsigned long data;
    U16 size;
    static U08 count = 0;

    while (1)
    {
        data = bsp_sim_char_get(&RXbuff);
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

        if (sim.pack_rx == 3)
	size=sim.buf[1]+sim.buf[2]*256;

        if (sim.pack_rx>=5 && size==sim.pack_rx)
        {
            sim.pack_rx = 0;
            data = crc32_calc(sim.buf, size-2);
	if ((unsigned short)data == sim.buf[size-2]+256*sim.buf[size-1])
                return TRUE;
        }
    }

    if (count++ > 15)
        sim.pack_rx = 0;

    return FALSE;
}

