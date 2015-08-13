/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __TN_USER_H__
#define __TN_USER_H__

#include "tnkernel/tn.h"

#define TN_SYS_TICK_FQ_HZ           1000    // 1 kHz for task switching

// common ROM task's parameters

// main.c
#define TASK_LEDS_PRI               4
#define TASK_LEDS_STK_SZ            128

// VPU.c
#define TASK_VPU_PRI                5
#define TASK_VPU_STK_SZ             128

// sim900/sim900.c
#define TASK_SIM900_PRI             6
#define TASK_SIM900_STK_SZ          128*4

// lwip/lwiplib.c
#define TASK_ETH_PRI                12
#define TASK_ETH_STK_SZ             256

// ipmulticast/cmd_ch.c
#define TASK_CMD_PRI                16
#define TASK_CMD_STK_SZ             256
//
// ipmulticast/cmd_ch.c
#define TASK_CMD_LED_PRI            25
#define TASK_CMD_STK_LED_SZ         256

// adcc.c
#define TASK_ADC_PRI                29
#define TASK_ADC_STK_SZ             256

// gps/gps.c
#define TASK_GPS_PRI                20
#define TASK_GPS_STK_SZ             256

// pins.c
#define TASK_PINS_PRI               25
#define TASK_PINS_STK_SZ            256


// light/light.c
#define TASK_LIGHT_PRI              10
#define TASK_LIGHT_STK_SZ           256

// digi/rf_task.c
#define TASK_RF_PRI                 14
#define TASK_RF_STK_SZ              256

// sys util's

int     tn_cpu_int_disable();
int     tn_cpu_int_enable();
int     tn_inside_int();
void    tn_halt();
void    tn_halt_boot();
void    tn_reset();

// common interrupts

void tn_reset_int_handler();
void tn_nmi_int_handler();
void tn_fault_int_handler();
void tn_default_int_handler();
void tn_sys_tick_int_handler();
void PendSV_Handler();
// hardware initialization

void init_hardware();

// hw time util

void            hw_delay(unsigned long uS);         // hard loop delay in microseconds with tn_disable_interrupt()
unsigned long   hw_time();                          // current time in milliseconds
unsigned long   hw_time_span(unsigned long time);   // time span in milliseconds
unsigned long   hw_sys_clock();                     // system clock in MHz

// hw timer0a util, periodic

void hw_timer0a_init(TN_EVENT* evt, unsigned evt_pattern);
void hw_timer0a_set_timeout(unsigned long uS);  // max timeout == 1000000 uS (1 s)
void hw_timer0a_start();
void hw_timer0a_stop();

// hw timer1a util, one shot

void hw_timer1a_init(TN_EVENT* evt, unsigned evt_pattern);
void hw_timer1a_start_once(unsigned long uS);  // max timeout == 1000000 uS (1 s)

// watchdog

void hw_watchdog_init();    // can not be disabled
void hw_watchdog_clear();

// interrupts

void hw_ethernet_int_handler();
void hw_timer0a_int_handler();
void hw_timer1a_int_handler();
void hw_watchdog_int_handler();
void hw_adc_seq1_int_handler();
void hw_adc_seq0_int_handler();
//void hw_adc0_seq2_int_handler();
void hw_uart0_int_handler();
void hw_uart1_int_handler();
void hw_uart2_int_handler();
void hw_portA_int_handler();
void hw_portG_int_handler();
void hw_portC_int_handler();
void hw_portJ_int_handler();
void hw_portH_int_handler();
void hw_USB0DeviceIntHandler();

#endif // __TN_USER_H__
