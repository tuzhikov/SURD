/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __LM3S9B90_UART_H__
#define __LM3S9B90_UART_H__

void lm3s90_uart_init();
void lm3s90_uart_free();
void lm3s90_uart_out(char const* s);

#endif // __LM3S9B90_UART_H__
