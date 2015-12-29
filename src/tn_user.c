/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "stellaris.h"
#include "tnkernel/tn.h"
#include "lwip/lwiplib.h"
#include "pins.h"
#include "adcc.h"
#include "utime.h"
#include "gps/gps.h"
#include "memory/ds1390.h"
#include "sim900/sim900.h"
#include "vpu.h"
#include "event/evt_fifo.h"

//#include "digi/rf_task.h"

#include "debug/debug.h"

#include "inc/hw_types.h"
#include "driverlib/usb.h"
#include "usblib/usblib.h"
#include "usblib/usb-ids.h"
#include "usblib/device/usbdevice.h"
#include "usblib/device/usbdbulk.h"


//#include "usb_bulk_structs.h"

#define BOOT_DELAY  200000  // 200 mS

extern void __iar_program_start(); // The entry point for IAR CRT library.

// local variables

static unsigned long volatile   g_sys_ticks;
static TN_EVENT                 *g_timer0a_evt, *g_timer1a_evt;
static unsigned                 g_timer0a_evt_pattern, g_timer1a_evt_pattern;
static BOOL volatile            g_watchdog_clear = TRUE;
static unsigned long volatile   adc_trigg=0;

// Tозвраает состояние регистра PRIMASK перед изменением.
// 1 - если прерvвания уже бvли вvкліченv до вvзова функции.
int tn_cpu_int_disable()
{
    return MAP_IntMasterDisable();
}
/*----------------------------------------------------------------------------*/
// Tозвраает состояние регистра PRIMASK перед изменением.
// 1 - если прерvвания бvли вvкліченv до вvзова функции.
int tn_cpu_int_enable()
{
    return MAP_IntMasterEnable();
}
/*----------------------------------------------------------------------------*/
int tn_inside_int()
{
    return HWREG(NVIC_INT_CTRL) & 0x1FF; // NVIC_INT_CTRL[8:0] - active interrupts bits
}
/*----------------------------------------------------------------------------*/
void tn_halt()
{
    tn_cpu_int_disable();
    dbg_trace();
    tn_halt_boot();
}
/*----------------------------------------------------------------------------*/
void tn_halt_boot()
{
    tn_cpu_int_disable();

    for (unsigned int i=0; i<50; i++)
    {
        hw_delay(50000);
    }
    Event_Push_Str("Reset Halt...\n");
    resrtart();
    //tn_reset();
}
/*----------------------------------------------------------------------------*/
void tn_reset()
{
    tn_cpu_int_disable();
    MAP_SysCtlReset();
}
/*----------------------------------------------------------------------------*/
void resrtart(void)
{
setFlagRestart();
tn_reset(); // сбросс CPU
}
/*----------------------------------------------------------------------------*/
// interrupts
void tn_reset_int_handler()
{

    hw_watchdog_init();

    tn_cpu_int_disable(); // switched back in tn_start_system();

#ifdef BOOT_LOADER
    hw_delay(BOOT_DELAY);
#endif // BOOT_LOADER

#ifdef DEBUG
    hw_delay(1000000); // DEBUG delay 1000 mS
#endif // DEBUG

#ifndef BOOT_LOADER
        // Set system clock to 80 MHz
        MAP_SysCtlClockSet(SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ | SYSCTL_SYSDIV_2_5 | SYSCTL_INT_OSC_DIS | SYSCTL_INT_PIOSC_DIS);

        // Set system clock to 50 MHz
        //MAP_SysCtlClockSet(SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ | SYSCTL_SYSDIV_4 | SYSCTL_INT_OSC_DIS | SYSCTL_INT_PIOSC_DIS);
#endif // BOOT_LOADER


    __iar_program_start();
}

void tn_nmi_int_handler()
{
    dbg_puts("NMI interrupt");
    tn_halt();
}

void tn_fault_int_handler()
{
    dbg_puts("Fault interrupt");

    /*
    unsigned long flag = HWREG(NVIC_HFAULT_STAT);
    unsigned long fault = HWREG(NVIC_FAULT_STAT);
    unsigned long addr = HWREG(NVIC_MM_ADDR);
    unsigned long faddr = HWREG(NVIC_FAULT_ADDR);

    HWREG(NVIC_HFAULT_STAT) = 0x40000000;
    HWREG(NVIC_FAULT_STAT) = 0x00020000;
    if (flag == 0x040000 || fault || addr || faddr)
    {
        fault = 0;

    }
    */
    asm("TST LR, #4");
    /*asm("ITE EQ");
    asm("MRSEQ R0, MSP");
    asm("MRSNE R0, PSP");*/
     asm("ITE EQ \n"
    "MRSEQ R0, MSP \n"
    "MRSNE R0, PSP");
    asm("B fault_print");
}

void fault_print(unsigned int * hardfault_args)
{
    unsigned int stacked_r0;
    unsigned int stacked_r1;
    unsigned int stacked_r2;
    unsigned int stacked_r3;
    unsigned int stacked_r12;
    unsigned int stacked_lr;
    unsigned int stacked_pc;
    unsigned int stacked_psr;

    stacked_r0 = ((unsigned long) hardfault_args[0]);
    stacked_r1 = ((unsigned long) hardfault_args[1]);
    stacked_r2 = ((unsigned long) hardfault_args[2]);
    stacked_r3 = ((unsigned long) hardfault_args[3]);

    stacked_r12 = ((unsigned long) hardfault_args[4]);
    stacked_lr = ((unsigned long) hardfault_args[5]);
    stacked_pc = ((unsigned long) hardfault_args[6]);
    stacked_psr = ((unsigned long) hardfault_args[7]);

    dbg_puts ("\n\n[Hard fault handler]\n");
    dbg_printf ("R0 = %x\n", stacked_r0);
    dbg_printf ("R1 = %x\n", stacked_r1);
    dbg_printf ("R2 = %x\n", stacked_r2);
    dbg_printf ("R3 = %x\n", stacked_r3);
    dbg_printf ("R12 = %x\n", stacked_r12);
    dbg_printf ("LR [R14] = %x  subroutine call return address\n", stacked_lr);
    dbg_printf ("PC [R15] = %x  program counter\n", stacked_pc);
    dbg_printf ("PSR = %x\n", stacked_psr);

    tn_halt();
}

void tn_default_int_handler()
{
    dbg_puts("default interrupt");
    tn_halt();
}

void tn_sys_tick_int_handler()
{
    tn_tick_int_processing();

    ++g_sys_ticks;

    if (g_sys_ticks % TN_SYS_TICK_FQ_HZ == 0)
        hw_unix_time_int_handler();

    if (g_sys_ticks % ETH_TIMER_EVENT_TICKS == 0)
        lwip_timer_event_iset();

    tn_int_exit();
}

// hardware initialization SysCtlClockSet

void init_hardware()
{
    // Set SysTick timer period (TN_SYS_TICK_FQ_HZ)
    MAP_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                       SYSCTL_XTAL_16MHZ);
    ///////////
    MAP_SysTickDisable();
    MAP_SysTickPeriodSet(MAP_SysCtlClockGet() / TN_SYS_TICK_FQ_HZ);
    MAP_SysTickIntEnable();
    MAP_SysTickEnable();
}

// hw time util

void hw_delay(unsigned long uS)
{
    TN_INTSAVE_DATA
    tn_disable_interrupt();
    MAP_SysCtlDelay(MAP_SysCtlClockGet() / 1000000 * uS / 3); // The loop takes 3 cycles/loop
    tn_enable_interrupt();
}

unsigned long hw_time()
{
    TN_INTSAVE_DATA
    tn_disable_interrupt();
    unsigned long const ticks = g_sys_ticks;
    tn_enable_interrupt();
    return ticks;
}

unsigned long hw_time_span(unsigned long time)
{
    TN_INTSAVE_DATA
    tn_disable_interrupt();
    unsigned long const span = g_sys_ticks - time; // unsigned half owerflov
    tn_enable_interrupt();
    return span;
}

unsigned long hw_sys_clock()
{
    return MAP_SysCtlClockGet() / 1000000;
}

// hw timer0a util, periodic

void hw_timer0a_init(TN_EVENT* evt, unsigned evt_pattern)
{

    if (evt == NULL || evt_pattern == 0)
    {
        dbg_puts("evt == NULL || evt_pattern == 0");
        dbg_trace();
        tn_halt();
    }

    g_timer0a_evt           = evt;
    g_timer0a_evt_pattern   = evt_pattern;

    // TIMER_0_A32
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    MAP_SysCtlPeripheralReset(SYSCTL_PERIPH_TIMER0);
    MAP_TimerDisable(TIMER0_BASE, TIMER_A);
    MAP_TimerConfigure(TIMER0_BASE, TIMER_CFG_32_BIT_PER); // periodic mode
    MAP_TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    //MAP_IntPrioritySet(INT_TIMER0A, 0); // 0 - max pri 7 - min pri
    MAP_IntEnable(INT_TIMER0A);

/*
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
    MAP_TimerDisable(TIMER3_BASE, TIMER_A);
    MAP_TimerConfigure(TIMER3_BASE, TIMER_CFG_32_BIT_OS);
    MAP_TimerEnable(TIMER3_BASE, TIMER_A);
*/
}

void hw_timer0a_set_timeout(unsigned long uS)
{
#ifdef DEBUG
    if (uS > 1000000)
    {
        dbg_puts("uS > 1000000");
        dbg_trace();
        tn_halt();
    }
#endif // DEBUG

    MAP_TimerLoadSet(TIMER0_BASE, TIMER_A, MAP_SysCtlClockGet() / 1000000 * uS);
}

void hw_timer0a_start()
{
    MAP_TimerEnable(TIMER0_BASE, TIMER_A);
}

void hw_timer0a_stop()
{
    MAP_TimerDisable(TIMER0_BASE, TIMER_A);
}

// hw timer1a util, one shot

void hw_timer1a_init(TN_EVENT* evt, unsigned evt_pattern)
{
    if (evt == NULL || evt_pattern == 0)
    {
        dbg_puts("evt == NULL || evt_pattern == 0");
        dbg_trace();
        tn_halt();
    }

    g_timer1a_evt           = evt;
    g_timer1a_evt_pattern   = evt_pattern;

    // TIMER_1_A32
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    MAP_SysCtlPeripheralReset(SYSCTL_PERIPH_TIMER1);
    MAP_TimerDisable(TIMER1_BASE, TIMER_A);
    MAP_TimerConfigure(TIMER1_BASE, TIMER_CFG_32_BIT_OS); // one shot mode
    MAP_TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    //MAP_IntPrioritySet(INT_TIMER1A, 0); // 0 - max pri 7 - min pri
    MAP_IntEnable(INT_TIMER1A);
}

void hw_timer1a_start_once(unsigned long uS)
{
#ifdef DEBUG
    if (uS > 1000000)
    {
        dbg_puts("uS > 1000000");
        dbg_trace();
        tn_halt();
    }
#endif // DEBUG

    MAP_TimerLoadSet(TIMER1_BASE, TIMER_A, MAP_SysCtlClockGet() / 1000000 * uS);
    MAP_TimerEnable(TIMER1_BASE, TIMER_A);
}

// watchdog

void hw_watchdog_init()
{
    // Startup watchdog timer
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_WDOG0);
    MAP_WatchdogReloadSet(WATCHDOG0_BASE, MAP_SysCtlClockGet() * 8); // 8s
    MAP_WatchdogResetEnable(WATCHDOG0_BASE);

#ifdef DEBUG
    MAP_WatchdogStallEnable(WATCHDOG0_BASE);
#endif // DEBUG

    MAP_IntEnable(INT_WATCHDOG);
    MAP_WatchdogEnable(WATCHDOG0_BASE); // can not be disabled

    hw_watchdog_clear();
}

void hw_watchdog_clear()
{
    g_watchdog_clear = TRUE;
}

/* interrupts ----------------------------------------------------------------*/
void hw_ethernet_int_handler()
{
    lwip_eth_int_processing();
    tn_int_exit();
}
/*----------------------------------------------------------------------------*/
void hw_USB0DeviceIntHandler()
{
    //lwip_eth_int_processing();
//    USB0DeviceIntHandler();
    tn_int_exit();
}
/*----------------------------------------------------------------------------*/
void hw_timer0a_int_handler()
{
    MAP_TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    tn_event_iset(g_timer0a_evt, g_timer0a_evt_pattern);
    tn_int_exit();
}
/*----------------------------------------------------------------------------*/
void hw_timer1a_int_handler()
{
    MAP_TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    tn_event_iset(g_timer1a_evt, g_timer1a_evt_pattern);
    tn_int_exit();
}
/*----------------------------------------------------------------------------*/
void hw_watchdog_int_handler()
{
    //MAP_WatchdogIntClear(WATCHDOG0_BASE);
    if (g_watchdog_clear)
    {
        MAP_WatchdogIntClear(WATCHDOG0_BASE);
        g_watchdog_clear = FALSE;
    }else
    {
#ifdef DEBUG
        dbg_trace();
#endif // DEBUG
        Event_Push_Str("Reset Watchdog...\n");
        resrtart();
        //tn_reset();
    }
}
/*----------------------------------------------------------------------------*/
void hw_adc_seq0_int_handler()
{
    adc_seq0_int_handler();
    tn_int_exit();
}
/*----------------------------------------------------------------------------*/
void hw_adc_seq1_int_handler()
{
    adc_trigg++;
    adc0_seq1_int_handler();
    tn_int_exit();
}
/*----------------------------------------------------------------------------*/
void hw_uart0_int_handler()
{
    uart0_int_handler();
    tn_int_exit();
}
/*----------------------------------------------------------------------------*/
void hw_uart1_int_handler()
{
    uart1_int_handler();
    tn_int_exit();
}
/*----------------------------------------------------------------------------*/
void hw_uart2_int_handler()
{
//    uart2_int_handler();
    tn_int_exit();
}
/*----------------------------------------------------------------------------*/
void hw_portA_int_handler()
{
    unsigned char mask = GPIOPinIntStatus(GPIO_PORTA_BASE, NULL);
    if (mask & GPIO_PIN_7)
    {
        MAP_GPIOPinIntClear(GPIO_PORTA_BASE, GPIO_PIN_7 );
        DS1390_int_handler();
    }
    tn_int_exit();
}
/*----------------------------------------------------------------------------*/
void hw_portJ_int_handler()
{
    /*
     unsigned char mask = GPIOPinIntStatus(GPIO_PORTJ_BASE, NULL);
    if (mask & GPIO_PIN_7)
    {
        MAP_GPIOPinIntClear(GPIO_PORTA_BASE, GPIO_PIN_7 );
        //DS1390_int_handler();
    }
    */
    IntGPIO_U();
    tn_int_exit();
}
/*----------------------------------------------------------------------------*/
void hw_portH_int_handler()
{
    IntGPIO_PW_CONTR();
    tn_int_exit();
}
/*----------------------------------------------------------------------------*/
void hw_portG_int_handler()
{
    unsigned char mask = GPIOPinIntStatus(GPIO_PORTG_BASE, NULL);
    if (mask & GPIO_PIN_7)
    {
        MAP_GPIOPinIntClear(GPIO_PORTG_BASE, GPIO_PIN_7 );
        GPS_PPS_int_handler();
    }

    tn_int_exit();
}
/*----------------------------------------------------------------------------*/
void hw_portC_int_handler()
{
    unsigned char mask = GPIOPinIntStatus(GPIO_PORTC_BASE, NULL);

    if (mask & GPIO_PIN_4)
    {
    MAP_GPIOPinIntClear(GPIO_PORTC_BASE, GPIO_PIN_4 );
    }
    if (mask & GPIO_PIN_5)
    {
    MAP_GPIOPinIntClear(GPIO_PORTC_BASE, GPIO_PIN_5 );
    }
    tn_int_exit();
}