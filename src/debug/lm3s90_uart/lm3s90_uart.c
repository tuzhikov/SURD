/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "../../stellaris.h"
#include "lm3s90_uart.h"


#define SYSCTL_PERIPH_UART  SYSCTL_PERIPH_UART0
#define UART_BASE           UART0_BASE
#define UART_SPEED          115200

void lm3s90_uart_init()
{
    
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART);

    MAP_GPIOPinConfigure(GPIO_PA0_U0RX);
    MAP_GPIOPinConfigure(GPIO_PA1_U0TX);
    MAP_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    
    MAP_UARTConfigSetExpClk(
        UART_BASE,
        MAP_SysCtlClockGet(),
        UART_SPEED,
        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE
        );

    MAP_UARTEnable(UART_BASE);
    
}

void lm3s90_uart_free()
{
    
    MAP_UARTDisable(UART_BASE);
    MAP_SysCtlPeripheralDisable(SYSCTL_PERIPH_GPIOB);   // DEBUG ?
    MAP_SysCtlPeripheralDisable(SYSCTL_PERIPH_UART);    // DEBUG ?
    
}

void lm3s90_uart_out(char const* s)
{
    
    while (*s)
    {
        MAP_UARTCharPut(UART_BASE, *s++);
    }
    
}
