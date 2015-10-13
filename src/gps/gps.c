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
#include "gps.h"
#include "../memory/ds1390.h"
#include "../dk/dk.h"
#include "../event/evt_fifo.h"


#include "../debug/debug.h"

#define SYSCTL_PERIPH_UART          SYSCTL_PERIPH_UART0
#define INT_UART                    INT_UART0
#define UART_BASE                   UART0_BASE
#define UART_SPEED                  115200
#define UART_BUF_SZ                 256

#define GPS_REFRESH_INTERVAL        1000

#define PPS_INT_ENABLE()    MAP_GPIOPinIntEnable(GPIO_PORTG_BASE, GPIO_PIN_0)
#define PPS_INT_DISABLE()   MAP_GPIOPinIntDisable(GPIO_PORTG_BASE, GPIO_PIN_0)
#define GPS_RESET_ON()      pin_on(OPIN_GPS_RESET);
#define GPS_RESET_OFF()     pin_off(OPIN_GPS_RESET);


static TN_TCB task_GPS_tcb;
#pragma location = ".noinit"
#pragma data_alignment=8
static unsigned int task_GPS_stk[TASK_GPS_STK_SZ];
static void task_GPS_func(void* param);
//
volatile unsigned long st[10];
char p = 0;

// local variables

static unsigned char    g_rx_buf[UART_BUF_SZ], g_tx_buf[UART_BUF_SZ];
static unsigned char*   g_tx_buf_ptr;
static unsigned         g_rx_buf_len, g_tx_buf_len;
static unsigned char    health_count = 1;


static GPS_INFO gps_info;
static void ProcessGPGGA(unsigned char *pData);
static void ProcessGPRMC(unsigned char *pData);
static float string_to_float( unsigned char * buf);
static BOOL tx_pkt_snd_one();
////
// состояние светика 0 - нет жпс, 1 - что-то шлется, 2 - верное время
static unsigned char GPS_LED=0;
  SYSTEMTIME   cool_pack_recv, bad_pack_recv; // control time

#define DEFAULT_GMT 4
static BOOL validGMT = false;
static unsigned char GPS_synk_flag;//=false;
volatile unsigned char gps_pps_counter=0;

void GPS_init()
{

    dbg_printf("Initializing GPS module...");

    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART);

    MAP_GPIOPinConfigure(GPIO_PA0_U0RX);
    MAP_GPIOPinConfigure(GPIO_PA1_U0TX);
    MAP_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    MAP_UARTConfigSetExpClk(UART_BASE, MAP_SysCtlClockGet(), UART_SPEED, UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);
    MAP_UARTDisable(UART_BASE);
    MAP_UARTTxIntModeSet(UART_BASE, UART_TXINT_MODE_EOT);
    MAP_UARTIntEnable(UART_BASE, UART_INT_RX | UART_INT_TX);
    MAP_IntEnable(INT_UART);
    MAP_UARTEnable(UART_BASE);
    MAP_UARTFIFODisable(UART_BASE);

    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    //
    MAP_IntEnable(INT_GPIOG);
    // Настроить прерывания на PPS
    MAP_GPIOIntTypeSet(GPIO_PORTG_BASE, GPIO_PIN_7, GPIO_FALLING_EDGE);
    MAP_GPIOPinIntEnable(GPIO_PORTG_BASE, GPIO_PIN_7);
    //

    if (tn_task_create(&task_GPS_tcb, &task_GPS_func, TASK_GPS_PRI,
        &task_GPS_stk[TASK_GPS_STK_SZ - 1], TASK_GPS_STK_SZ, 0,
        TN_TASK_START_ON_CREATION) != TERR_NO_ERR)
    {
        dbg_puts("tn_task_create(&task_GPS_tcb) error");
        goto err;
    }

    // Настроить прерывания на PPS
    //MAP_IntEnable(INT_GPIOG);
    //MAP_GPIOIntTypeSet(GPIO_PORTG_BASE, GPIO_PIN_7, GPIO_FALLING_EDGE);
    //MAP_GPIOPinIntEnable(GPIO_PORTG_BASE, GPIO_PIN_7);

    dbg_puts("[done]");

    return;
err:
    dbg_trace();
    tn_halt();
}
//------------------------------------------------------------------------------
//new
void uart0_int_handler()
{
    unsigned long const ist = MAP_UARTIntStatus(UART_BASE, TRUE);

    if (ist & (UART_INT_RX | UART_INT_RT))
    {
        MAP_UARTIntClear(UART_BASE, UART_INT_RX|UART_INT_RT);
        long  b;
      while(MAP_UARTCharsAvail(UART_BASE))
      {
        b = MAP_UARTCharGetNonBlocking(UART_BASE);
        if (b == -1 || (char)b == '$' || g_rx_buf_len >= UART_BUF_SZ)
        {
            g_rx_buf_len = 0;
            g_rx_buf[g_rx_buf_len++] = b;
        }else
        {
            if (b == 0x0D && g_rx_buf[0] == '$' && g_rx_buf[g_rx_buf_len-3] == '*')  // +ёыш ъюэхЎ яръхЄр
            {
                if ( g_rx_buf_len > 10)
                {

                    unsigned char crc;
                    if (g_rx_buf[--g_rx_buf_len] > '9')
                        crc = 0x0A + g_rx_buf[g_rx_buf_len]-'A';
                    else
                        crc = (g_rx_buf[g_rx_buf_len]-'0');

                    if (g_rx_buf[--g_rx_buf_len] > '9')
                        crc |= (0x0A + g_rx_buf[g_rx_buf_len]-'A')<< 4;
                    else
                        crc |= (g_rx_buf[g_rx_buf_len]-'0')<< 4;

                    g_rx_buf_len-=2;

                    do{
                        crc ^= g_rx_buf[g_rx_buf_len];
                    }while( --g_rx_buf_len );

                    if (!crc) // CRC яръхЄр ёю°¬ыё  - юсЁрсюЄрЄ№!
                    {
                        health_count = 0;
                        gps_info.pack_found++;

                       /* Parse packet */
                        if( !strncmp((char*)g_rx_buf, "$GPGGA", 6) )
	                {
		            ProcessGPGGA(&g_rx_buf[7]);
                            st[1] =hw_time();
                        }
	                else if( !strncmp((char*)g_rx_buf, "$GPRMC", 6))
	                {
	                    ProcessGPRMC(&g_rx_buf[7]);
                            //
                            gps_pps_counter=0;
                            //
                            //
                            st[0] =hw_time();

                        }
                    }else
                    {
                        gps_info.crc_error++;
                    }
                }

                g_rx_buf_len = 0;
            }else
            if (b != 0x0A)
                g_rx_buf[g_rx_buf_len++] = b;
          }

       }//while
    }
    /////
    if (ist & UART_INT_TX)
    {
        MAP_UARTIntClear(UART_BASE, UART_INT_TX);
        tx_pkt_snd_one();
    }

}


//-------------------------------------------------------------------------------
/*
void uart0_int_handler()
{
    unsigned long const ist = MAP_UARTIntStatus(UART_BASE, TRUE);

    if (ist & UART_INT_RX)
    {
        MAP_UARTIntClear(UART_BASE, UART_INT_RX);
        long const b = MAP_UARTCharGetNonBlocking(UART_BASE);
        if (b == -1 || (char)b == '$' || g_rx_buf_len >= UART_BUF_SZ)
        {
            g_rx_buf_len = 0;
        }else
        {
            if (b == 0x0A )  // Если конец пакета
            {
                if ( g_rx_buf_len > 10)
                {
                    unsigned char crc;
                    if (g_rx_buf[--g_rx_buf_len] > '9')
                        crc = 0x0A + g_rx_buf[g_rx_buf_len]-'A';
                    else
                        crc = (g_rx_buf[g_rx_buf_len]-'0');

                    if (g_rx_buf[--g_rx_buf_len] > '9')
                        crc |= (0x0A + g_rx_buf[g_rx_buf_len]-'A')<< 4;
                    else
                        crc |= (g_rx_buf[g_rx_buf_len]-'0')<< 4;

                    g_rx_buf_len--;

                    do{
                        crc ^= g_rx_buf[--g_rx_buf_len];
                    }while( g_rx_buf_len );


                    if (!crc) // CRC пакета сошёлся - обработать!
                    {
                        health_count = 0;


                        if( !strncmp((char*)g_rx_buf, "GPGGA", 5) )
	                {
		            ProcessGPGGA(&g_rx_buf[6]);
                        }
	                else if( !strncmp((char*)g_rx_buf, "GPRMC", 5))
	                {
	                    ProcessGPRMC(&g_rx_buf[6]);
                        }
                    }
                }

                g_rx_buf_len = 0;
            }

            if (b == 0x0D)
            {
            }
            else
            {
                g_rx_buf[g_rx_buf_len++] = b;
            }
        }
    }

    if (ist & UART_INT_TX)
    {
        MAP_UARTIntClear(UART_BASE, UART_INT_TX);
        tx_pkt_snd_one();
    }
}
*/
//------------------------------------------------------------------------------
void GPS_PPS_int_handler(void)
{

    MAP_UARTIntDisable(UART_BASE, UART_INT_RX);

    memcpy(&gps_info.time_now, &gps_info.time_pack, sizeof(gps_info.time_pack));
    gps_pps_counter++;

    MAP_UARTIntEnable(UART_BASE, UART_INT_RX);

}
//------------------------------------------------------------------------------
BOOL Synk_TIME(void)
{
    DS1390_TIME t;
    SYSTEMTIME st, n_st;
    //
    if (!GPS_synk_flag)
       return false;
    //
    if (gps_pps_counter>1)
       return false;
    //
    GPS_synk_flag=false;
    //проверка валидности
    if (gps_info.time_valid)
     {
       st.tm_hour = gps_info.time_pack.hour;
       st.tm_mday = gps_info.time_pack.date;
       st.tm_min  = gps_info.time_pack.min;
       //
       st.tm_mon  = gps_info.time_pack.month;
       st.tm_sec  = gps_info.time_pack.sec;
       st.tm_year = gps_info.time_pack.year;
       //
       if (st.tm_year<100)
         st.tm_year+=2000;
       // Корректировка UTC+
       /*
       int gmt=4;
       if (DK[CUR_DK].proj_valid) // проверка проекта
          gmt = PROJ[CUR_DK].jornal.gmt;*/
       int gmt = getValueGMT();
       TIME_PLUS(&st, &n_st, gmt*3600);
       t.msec = 0;
       t.sec = n_st.tm_sec;
       t.min = n_st.tm_min;
       t.hour = n_st.tm_hour;
       t.year = n_st.tm_year;
       t.month = n_st.tm_mon;
       t.date = n_st.tm_mday;
       //
       if (SetTime_DS1390(&t)){GPS_LED = 2;}
                          else{GPS_LED = 1;}
     }//end gps_info.time_valid
return true;
}
//------------------------------------------------------------------------------
void Check_State_LED()
{
    unsigned long i_c,i_b;
    //
    i_c = Seconds_Between(&cool_pack_recv, &CT);
    i_b = Seconds_Between(&bad_pack_recv, &CT);
    //
    if ((i_c==0) && (i_b==0))
      return;
    //
    if (i_c > i_b)
    {
      if (i_b>5)
       GPS_LED=0;
      else
        GPS_LED=1;
    }
    else
    {
      if (i_c>5)
       GPS_LED=0;
      else
        GPS_LED=2;
    }
}
//----------------------------------------------------------------------------//
enum E_MESSAGE{ST_M_OK,ST_M_NO,ST_F_OK,ST_F_NO,ST_T_OK,ST_T_NO,ST_END};
// запись в журнал событий по одному разу
static void WrateEventStr(const BYTE inEvent)
{
static bool fWr[ST_END]={0};

switch(inEvent)
  {
  case ST_M_OK:
    if(!fWr[ST_M_OK]){fWr[ST_M_OK] = true;Event_Push_Str("GPS/GLONASS модуль найден");}
    fWr[ST_M_NO] = false;
    break;
  case ST_M_NO:
    if(!fWr[ST_M_NO]){fWr[ST_M_NO] = true;Event_Push_Str("GPS/GLONASS модуль потерян");}
    fWr[ST_M_OK] = false;
    break;
  case ST_F_OK:
    //if(!fWr[ST_F_OK]){fWr[ST_F_OK] = true;Event_Push_Str("GPS/GLONASS время определил");}
    fWr[ST_F_NO] =false;
    break;
  case ST_F_NO:
    //if(!fWr[ST_F_NO]){fWr[ST_F_NO] = true;Event_Push_Str("GPS/GLONASS время потерял");}
    fWr[ST_F_OK] = false;
    break;
  case ST_T_OK:
    if(!fWr[ST_T_OK]){fWr[ST_T_OK] = true;Event_Push_Str("GPS/GLONASS время ОК");}
    fWr[ST_T_NO] = false;
    break;
  case ST_T_NO:
    if(!fWr[ST_T_NO]){fWr[ST_T_NO] = true;Event_Push_Str("GPS/GLONASS время NO");}
    fWr[ST_T_OK] = false;
    break;
  }
}
//------------------------------------------------------------------------------
static void task_GPS_func(void* param)
{
    unsigned char time_valid = gps_info.time_valid = 0;
    unsigned char fix_valid = gps_info.fix_valid = 0;
    unsigned char health = gps_info.health = 0;
    //PPS_INT_ENABLE();
    static unsigned char count_time=0;
    GPS_synk_flag=false;
    //
    pin_on(OPIN_TEST1);
    ///////
    for (;;)
    {
        tn_task_sleep(GPS_REFRESH_INTERVAL);
        //
        Check_State_LED();
        //
        if (GPS_LED==1)
        {
          pin_off(OPIN_TEST1);
          tn_task_sleep(100);
          pin_on(OPIN_TEST1);
        }
        ///
        if (GPS_LED==2)
        {
          if (count_time % 2)
          {
            pin_off(OPIN_TEST1);
          }
          else
          {
            pin_on(OPIN_TEST1);
          }
        }
        ///
        count_time++;
        if ((count_time % 100)==0)
          GPS_synk_flag=true;
        //Synk_TIME();

        //
        if (health_count > 4)
        {
            gps_info.health = FALSE;
            //PPS_INT_ENABLE(); //PPS_INT_DISABLE();
            pin_on(OPIN_GPS_RESET);  // Пробуем сбросить модуль GPS
            health_count = 1;
        }
        else
        {
            pin_off(OPIN_GPS_RESET);
            if (!health_count++)
                gps_info.health = TRUE;
        }

        //if (health != gps_info.health)
        {
            health = gps_info.health;
            if (health){
              dbg_puts("GPS/GLONASS module found");
              WrateEventStr(ST_M_OK);
              }
              else{
              dbg_puts("GPS/GLONASS module lost");
              WrateEventStr(ST_M_NO);
              }
        }

        if (fix_valid != gps_info.fix_valid)
        {
            fix_valid = gps_info.fix_valid;
            if (fix_valid){
                dbg_puts("GPS/GLONASS FIX valid");
                WrateEventStr(ST_F_OK);
                }
                else{
                dbg_puts("GPS/GLONASS FIX invalid");
                WrateEventStr(ST_F_NO);
                }
        }

        if (time_valid != gps_info.time_valid)
        {
            time_valid = gps_info.time_valid;
            if (time_valid)
            {
                dbg_puts("GPS/GLONASS time valid");
                PPS_INT_ENABLE();
                WrateEventStr(ST_T_OK);
            }
            else{
            dbg_puts("GPS/GLONASS time invalid");
            WrateEventStr(ST_T_NO);
            }
        }
    }
}
//------------------------------------------------------------------------------
static BOOL tx_pkt_snd_one()
{
    if (g_tx_buf_len == 0)
    {
        g_tx_buf_ptr = g_tx_buf;
        return FALSE;
    }

    if (MAP_UARTCharPutNonBlocking(UART_BASE, *g_tx_buf_ptr++) == FALSE)
    {
        g_tx_buf_ptr = g_tx_buf;
        g_tx_buf_len = 0;
        return FALSE;
    }

    --g_tx_buf_len;
    return TRUE;
}
//------------------------------------------------------------------------------

void Get_gps_info(GPS_INFO *gps)
{
    MAP_UARTIntDisable(UART_BASE, UART_INT_RX);

    memcpy(gps, &gps_info, sizeof(gps_info));

    MAP_UARTIntEnable(UART_BASE, UART_INT_RX);
}
//------------------------------------------------------------------------------
BOOL Send_Data_uart(const char *buf, unsigned char len)
{
    unsigned char send_first_byte = g_tx_buf_len;

    MAP_UARTIntDisable(UART_BASE, UART_INT_TX);

    while (len-- && g_tx_buf_len<UART_BUF_SZ)
    {
        g_tx_buf[g_tx_buf_len++] = *buf++;
    }

    MAP_UARTIntEnable(UART_BASE, UART_INT_TX);

    if (!send_first_byte)  // Если ничего до этого не отправлялось
        tx_pkt_snd_one();

    return !len;
}

///////////////////////////////////////////////////////////////////////////////
// Name:		GetField
//
// Description:	This function will get the specified field in a NMEA string.
//
// Entry:		BYTE *pData -		Pointer to NMEA string
//		BYTE *pField -		pointer to returned field
//		int nfieldNum -		Field offset to get
//		int nMaxFieldLen -	Maximum of bytes pFiled can handle
///////////////////////////////////////////////////////////////////////////////
static BOOL GetField(unsigned char *pData, unsigned char *pField, unsigned char nFieldNum, unsigned char nMaxFieldLen)
{
    //
    // Validate params
    //
    if(pData == NULL || pField == NULL || nMaxFieldLen <= 0)
    {
        return FALSE;
    }

    //
    // Go to the beginning of the selected field
    //
    unsigned char i = 0;
    unsigned char nField = 0;
    while (nField != nFieldNum && pData[i])
    {
        if (pData[i] == ',')
        {
    	nField++;
        }

        i++;

        if (pData[i] == NULL)
        {
    	pField[0] = '\0';
  	return FALSE;
        }
    }

    if (pData[i] == ',' || pData[i] == '*')
    {
        pField[0] = '\0';
        return FALSE;
    }

    //
    // copy field from pData to Field
    //
    unsigned char i2 = 0;
    while(pData[i] != ',' && pData[i] != '*' && pData[i])
    {
        pField[i2] = pData[i];
        i2++;
        i++;

    //
    // check if field is too big to fit on passed parameter. If it is,
    // crop returned field to its max length.
    //
        if (i2 >= nMaxFieldLen)
        {
	i2 = nMaxFieldLen-1;
	break;
        }
    }

    pField[i2] = '\0';
    return TRUE;
}
//------------------------------------------------------------------------------
static void ProcessGPGGA(unsigned char *pData)
{
    #define MAXFIELD 10
    unsigned char pField[MAXFIELD];

    //
    // Altitude
    //
    if (GetField(pData, pField, 8, MAXFIELD))
    {
        gps_info.Position.Altitude = string_to_float((unsigned char *)pField);
    }
}
//------------------------------------------------------------------------------
static void ProcessGPRMC(unsigned char *pData)
{
    #define MAXFIELD 10
    unsigned char pField[MAXFIELD];

    // Time
    if (GPS_LED==0)GPS_LED = 1;
    //
    if (GetField(pData, pField, 0, MAXFIELD))
    {
    // Hour
        gps_info.time_pack.hour = (pField[0]-'0')*10 + pField[1]-'0';

    // minute
        gps_info.time_pack.min = (pField[2]-'0')*10 + pField[3]-'0';

    // Second
        gps_info.time_pack.sec = (pField[4]-'0')*10 + pField[5]-'0';
    }

    //
    // Data valid
    //
    if (GetField(pData, pField, 1, MAXFIELD) && pField[0] == 'A')
    {
        gps_info.fix_valid = TRUE;
        gps_info.time_valid = TRUE;
        //GPS_LED = 2;
        memcpy(&cool_pack_recv, &CT, sizeof(SYSTEMTIME));
    }
    else
    {
        gps_info.fix_valid = FALSE;
        gps_info.time_valid = FALSE;
        //
        memcpy(&bad_pack_recv, &CT, sizeof(SYSTEMTIME));

    }
    //
    // Latitude
    //
    if (GetField(pData, pField, 2, MAXFIELD))
    {
        gps_info.Position.Latitude = string_to_float((unsigned char *)pField+2)/60.0;
        gps_info.Position.Latitude += (float)((pField[0]-'0')*10+(pField[1]-'0'));
    }
    if (GetField(pData, pField, 3, MAXFIELD))
    {
        if (pField[0] == 'S')
	gps_info.Position.Latitude = -gps_info.Position.Latitude;
    }

    //
    // Longitude
    //
    if (GetField(pData, pField, 4, MAXFIELD))
    {
        gps_info.Position.Longitude =  string_to_float((unsigned char *)pField+3)/60.0;
        gps_info.Position.Longitude += (float)((pField[0]-'0')*100 + (pField[1]-'0')*10 + (pField[2]-'0'));
    }

    if (GetField(pData, pField, 5, MAXFIELD))
    {
        if (pField[0] == 'W')
	gps_info.Position.Longitude = -gps_info.Position.Longitude;
    }

    //
    // Ground speed
    //
    if (GetField(pData, pField, 6, MAXFIELD))
    {
        gps_info.Velocity.Velocity = string_to_float((unsigned char *)pField)*0.5144444;
    }
    else
    {
        gps_info.Velocity.Velocity = 0.0;
    }

    //
    // course over ground, degrees true
    //
    if (GetField(pData, pField, 7, MAXFIELD))
    {
        gps_info.Velocity.Course = string_to_float((unsigned char *)pField);
    }
    else
    {
        gps_info.Velocity.Course = 0.0;
    }

    //
    // Date
    //
    if (GetField(pData, pField, 8, MAXFIELD))
    {
        // Day
        gps_info.time_pack.date = (pField[0]-'0')*10 + pField[1]-'0';

        // Month
        gps_info.time_pack.month = (pField[2]-'0')*10 + pField[3]-'0';

        // Year (Only two digits. I wonder why?)
        gps_info.time_pack.year = (pField[4]-'0')*10 + pField[5]-'0';

    }

}
///////////////////////////////
//------------------------------------------------------------------------------
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
// установить состояние проетка
void setValidGMT(const BOOL val)
{
validGMT = val;
}
// проверить состояние проетка
unsigned char getValueGMT(void)
{
if(validGMT)return PROJ[CUR_DK].jornal.gmt;
return DEFAULT_GMT;
}
