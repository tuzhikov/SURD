/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "tnkernel/tn.h"
#include "stellaris.h"
#include "spi.h"

#include "debug/debug.h"

#define DEFAULT_BITRATE 3000000

static TN_MUTEX g_spi0_mutex;
static TN_MUTEX g_spi1_mutex;

void spi0_init()
{
    dbg_printf("Initializing SPI0...");

    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);

    MAP_SSIDisable(SSI0_BASE);

    MAP_GPIOPinConfigure(GPIO_PA2_SSI0CLK);
    MAP_GPIOPinConfigure(GPIO_PA4_SSI0RX);
    MAP_GPIOPinConfigure(GPIO_PA5_SSI0TX);
    
    MAP_GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5);
//    MAP_GPIOPadConfigSet(_regs._GPIO_PORT_BASE, _regs._GPIO_PORT_BASE_MASK, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD_WPU);
   
    MAP_SSIConfigSetExpClk(SSI0_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, DEFAULT_BITRATE, 8);
    MAP_SSIEnable(SSI0_BASE);

    if (tn_mutex_create(&g_spi0_mutex, TN_MUTEX_ATTR_INHERIT, 0) != TERR_NO_ERR)
    {
        dbg_puts("tn_mutex_create(&g_spi0_mutex) != TERR_NO_ERR");
        dbg_trace();
        tn_halt();
    }    
    
    dbg_puts("[done]");
}

void spi0_set_bitrate(unsigned long bitrate)
{
    MAP_SSIDisable(SSI0_BASE);
    MAP_SSIConfigSetExpClk(SSI0_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, bitrate, 8);
    MAP_SSIEnable(SSI0_BASE);
}

unsigned char spi0_rw(unsigned char b)
{
    unsigned long rb;
    MAP_SSIDataPut(SSI0_BASE, (unsigned long)b);
    MAP_SSIDataGet(SSI0_BASE, &rb);
    return (unsigned char)rb;
}

void spi0_lock()
{
    if (tn_mutex_lock(&g_spi0_mutex, TN_WAIT_INFINITE) != TERR_NO_ERR)
    {
        dbg_puts("tn_mutex_lock(&g_spi0_mutex) != TERR_NO_ERR");
        dbg_trace();
        tn_halt();
    }
}

void spi0_lock_br(unsigned long bitrate)
{
    spi0_lock();
    spi0_set_bitrate(bitrate);
}

void spi0_unlock()
{
    if (tn_mutex_unlock(&g_spi0_mutex) != TERR_NO_ERR)
    {
        dbg_puts("tn_mutex_unlock(&g_spi0_mutex) != TERR_NO_ERR");
        dbg_trace();
        tn_halt();
    }
}

//PC6
void spi1_init()
{
    dbg_printf("Initializing SPI1...");

    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOH);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI1);

    MAP_SSIDisable(SSI1_BASE);

    MAP_GPIOPinConfigure(GPIO_PE2_SSI1RX);
    MAP_GPIOPinConfigure(GPIO_PE3_SSI1TX);
    MAP_GPIOPinConfigure(GPIO_PH4_SSI1CLK);

    MAP_GPIOPinTypeSSI(GPIO_PORTE_BASE, GPIO_PIN_2 | GPIO_PIN_3);
    MAP_GPIOPinTypeSSI(GPIO_PORTH_BASE, GPIO_PIN_4);
    
    MAP_SSIConfigSetExpClk(SSI1_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_3, SSI_MODE_MASTER, DEFAULT_BITRATE, 8);
    MAP_SSIEnable(SSI1_BASE);
    
    if (tn_mutex_create(&g_spi1_mutex, TN_MUTEX_ATTR_INHERIT, 0) != TERR_NO_ERR)
    {
        dbg_puts("tn_mutex_create(&g_spi1_mutex) != TERR_NO_ERR");
        dbg_trace();
        tn_halt();
    }    
    
    dbg_puts("[done]");
}

void spi1_set_bitrate(unsigned long bitrate)
{
    MAP_SSIDisable(SSI1_BASE);
    MAP_SSIConfigSetExpClk(SSI1_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_3, SSI_MODE_MASTER, bitrate, 8);
    MAP_SSIEnable(SSI1_BASE);
}

unsigned char spi1_rw(unsigned char b)
{
    unsigned long rb;
    MAP_SSIDataPut(SSI1_BASE, (unsigned long)b);
    MAP_SSIDataGet(SSI1_BASE, &rb);
    return (unsigned char)rb;
}

void spi1_lock()
{
    if (tn_mutex_lock(&g_spi1_mutex, TN_WAIT_INFINITE) != TERR_NO_ERR)
    {
        dbg_puts("tn_mutex_lock(&g_spi1_mutex) != TERR_NO_ERR");
        dbg_trace();
        tn_halt();
    }
}

void spi1_lock_br(unsigned long bitrate)
{
    spi1_lock();
    spi1_set_bitrate(bitrate);
}

void spi1_unlock()
{
    if (tn_mutex_unlock(&g_spi1_mutex) != TERR_NO_ERR)
    {
        dbg_puts("tn_mutex_unlock(&g_spi1_mutex) != TERR_NO_ERR");
        dbg_trace();
        tn_halt();
    }
}