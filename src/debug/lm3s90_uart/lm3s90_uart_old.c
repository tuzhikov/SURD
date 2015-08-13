/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "../../tnkernel/tn.h"
#include "../../stellaris.h"
#include "lm3s90_uart.h"

#define SYSCTL_PERIPH_UART  SYSCTL_PERIPH_UART1
#define INT_UART            INT_UART1
#define UART_BASE           UART1_BASE
#define UART_SPEED          115200

unsigned char buf_uart[256];
unsigned char p_tx;
unsigned char p_rx;


void lm3s90_uart_init()
{
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART);

    MAP_GPIOPinConfigure(GPIO_PB4_U1RX);
    MAP_GPIOPinConfigure(GPIO_PB5_U1TX);

    MAP_GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_4 | GPIO_PIN_5);
    MAP_UARTConfigSetExpClk(
        UART_BASE,
        MAP_SysCtlClockGet(),
        UART_SPEED,
        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE
        );

    MAP_UARTDisable(UART_BASE);
    MAP_UARTTxIntModeSet(UART_BASE, UART_TXINT_MODE_EOT);
//    MAP_UARTIntEnable(UART_BASE, UART_INT_TX);
    MAP_IntEnable(INT_UART);
    MAP_UARTEnable(UART_BASE);
    MAP_UARTFIFODisable(UART_BASE);
    
    p_tx = 0;
    p_rx = 0;
}

void lm3s90_uart_free()
{
    MAP_UARTDisable(UART_BASE);
    MAP_SysCtlPeripheralDisable(SYSCTL_PERIPH_GPIOB);   // DEBUG ?
    MAP_SysCtlPeripheralDisable(SYSCTL_PERIPH_UART);    // DEBUG ?
}

void lm3s90_uart_out(char const* s)
{
    MAP_UARTIntDisable(UART_BASE, UART_INT_TX);
    
    char p_temp = p_rx;

    while (*s)
    {
        buf_uart[p_rx++] = *s++;
        
        if (p_tx == p_rx)
            break;
    }
        
    
    if (p_temp == p_tx)
    {
          send_tx();
    }
     
    MAP_UARTIntEnable(UART_BASE, UART_INT_TX);

    
    
    
    /*
    
    while (*s)
    {
        MAP_UARTCharPut(UART_BASE, *s++);
    }
*/
}


void uart1_int_handler()
{
    unsigned long const ist = MAP_UARTIntStatus(UART_BASE, TRUE);

    if (ist & UART_INT_RX)
    {
        MAP_UARTIntClear(UART_BASE, UART_INT_RX);
        long const b = MAP_UARTCharGetNonBlocking(UART_BASE);
        MAP_UARTIntDisable(UART_BASE, UART_INT_RX);
    }

    if (ist & UART_INT_TX)
    {
        MAP_UARTIntClear(UART_BASE, UART_INT_TX);
        send_tx();
    }
}

BOOL send_tx(void)
{
    if (p_tx == p_rx)
    {
        MAP_UARTIntDisable(UART_BASE, UART_INT_TX);
        return FALSE;
    }

    if (MAP_UARTCharPutNonBlocking(UART_BASE,  buf_uart[p_tx++]) == FALSE)
    {
        MAP_UARTIntDisable(UART_BASE, UART_INT_TX);
        return FALSE;
    }
    return TRUE;
}