/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/
#include <stdlib.h>
#include "./tnkernel/tn.h"
#include "crc32.h"
#include "pins.h"
#include "spi.h"
#include "adcc.h"
#include "pref.h"
#include "lwip/lwiplib.h"
#include "memory/memory.h"
#include "memory/ds1390.h"
#include "multicast/cmd_ch.h"
#include "gps/gps.h"
#include "event/evt_fifo.h"
#include "sim900/sim900.h"
#include "version.h"
#include "stellaris.h"
#include "light/light.h"
#include "dk/dk.h"
#include "usb/usb_dev_serial.h"
#include "debug/debug.h"
#include "vpu.h"
//#include "digi/rf_task.h"

#define DS_COUNT        10

static TN_TCB task_leds_tcb;
#pragma location = ".noinit"
#pragma data_alignment=8
static unsigned int task_leds_stk[TASK_LEDS_STK_SZ];
static void task_leds_func(void* param);
/*-----------------local functions--------------------------------------------*/
static void dbg_print_app_ver();
static void startup();
static void Err_led(unsigned char err);
//static BOOL time_init=false;
/*----------------------------------------------------------------------------*/
/**/
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
void __error__(char *pcFilename, unsigned long ulLine)
{
    dbg_printf("__error__(\"%s\", ln=%u);\n", pcFilename, ulLine);
    tn_halt();
}
#endif
/*----------------- main function --------------------------------------------*/
int main()
{
  // Для запуска BootLoader порт A пин 4
  if (HWREG(FLASH_BOOTCFG) & FLASH_BOOTCFG_NW ){
       HWREG(FLASH_FMD) = FLASH_BOOTCFG_PORT_A | FLASH_BOOTCFG_PIN_4 | FLASH_BOOTCFG_DBG1;
       HWREG(FLASH_FMA) = 0x75100000;
       HWREG(FLASH_FMC) = FLASH_FMC_WRKEY | FLASH_FMC_COMT;
       while(HWREG(FLASH_FMC) & FLASH_FMC_COMT)
        {
        };
    }
  tn_start_system();
  return 0;
}
/*-----------------------Start system-----------------------------------------*/
void tn_app_init()
{
    tn_cpu_int_disable(); // прерывания включаются обратно после выхода из tn_app_init();
    // start
    startup();
    // for static IP
    struct ip_addr ipaddr   = { pref_get_long(PREF_L_NET_IP) };
    struct ip_addr netmask  = { pref_get_long(PREF_L_NET_MSK) };
    struct ip_addr gw       = { pref_get_long(PREF_L_NET_GW) };
    //ip_addr_debug_print(buf, ipaddr);
    ethernet_init((enum net_mode)pref_get_long(PREF_L_NET_MODE), &ipaddr, &netmask, &gw);
    cmd_ch_init(); // init cmd for ethernet
    // creat task_leds_func
    if (tn_task_create(&task_leds_tcb, &task_leds_func, TASK_LEDS_PRI, &task_leds_stk[TASK_LEDS_STK_SZ - 1],
        TASK_LEDS_STK_SZ, 0, TN_TASK_START_ON_CREATION) != TERR_NO_ERR)
    {
        dbg_puts("tn_task_create(&task_leds_tcb) error");
        goto err;
    }
    //time_init=true;
    return;

err:
    dbg_trace();
    tn_halt();
}
//------------------------------------------------------------------------------
void Clear_DS_COUNTS()
{
    WAIT_1S_COUNT=0;
    DS_INTS_COUNT=0;
}
//------------------------------------------------------------------------------
void Check_DS()
{
if (abs(WAIT_1S_COUNT-DS_INTS_COUNT)>5){
  //ales
  dbg_puts("Clock error!!!!!");
  Event_Push_Str("Часы (DS1390) не работают!!!");
  //
  #ifndef DEBUG
    DK_HALT();
  #endif
  }
Clear_DS_COUNTS();
}
//------------------------------------------------------------------------------
static void task_leds_func(void* param)
{
    bool  b_err;
    //
    Clear_DS_COUNTS();
    //
    for (;;)
    {
      if (DK[CUR_DK].CUR.source==ALARM)
       {
         //tn_task_sleep(50);
         if (b_err)
           pin_on(OPIN_ERR_LED);
         else
           pin_off(OPIN_ERR_LED);
         ///
         b_err=!b_err;

       }
       else
       {
         pin_on(OPIN_ERR_LED);
       }
       ///////////////////////////
        hw_watchdog_clear();
        tn_task_sleep(900);
        pin_off(OPIN_CPU_LED);
        tn_task_sleep(100);
        pin_on(OPIN_CPU_LED);
        //////////////////////
        WAIT_1S_COUNT++;
        if (WAIT_1S_COUNT > DS_COUNT)
            Check_DS();
    }
}
//------------------------------------------------------------------------------
// local functions
static void dbg_print_app_ver()
{

    // CPU: LM3S9B90
    // Clk: 50 MHz

    static char buf[128];
    get_version(buf, sizeof(buf));
    dbg_printf(
        "\n%s\n"
        APP_COPYRIGHT"\n\n"
        "CPU: LM3S9B96\n"
        "Clk: %u MHz\n\n",
        buf, hw_sys_clock()
        );
}
/* One start -----------------------------------------------------------------*/
static void startup()
{
    unsigned char err = 0;

    init_hardware();
    dbg_init();
    dbg_print_app_ver();
    crc32_init();
    pins_init();
    //spi0_init();    // used in m25pexx.c (FLASH)
    spi1_init();
    adc_init();
    err = memory_init();  // memory initialization, external EEPROM (used in pref.c, orion.c), external FLASH, internal FLASH
    if (err)
       goto err;
    pref_init();
    ds1390_init();
    evt_fifo_init();
    GPS_init();
    //SIM900_init();
    light_init();
    saveDatePorojectIP();//новые параметры связи
    vpu_init();  /*VPU start*/
    //Init_USB();
    //if (PROJ.jornal.power_on)
   //Event_Push_Str("СТАРТ");
    return;
err:
  Err_led(err);
}
/* Error LED blink------------------------------------------------------------*/
static void Err_led(unsigned char err)
{
    bool b_err;

    while (1)
    {
        MAP_WatchdogIntClear(WATCHDOG0_BASE);
        if (b_err)
           pin_on(OPIN_ERR_LED);
         else
           pin_off(OPIN_ERR_LED);
         b_err=!b_err;
        hw_delay(700000);
    }
}