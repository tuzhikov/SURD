/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "../tnkernel/tn.h"
#include "lm3s90_uart/lm3s90_uart.h"
#include "debug.h"
#include "../multicast/cmd_ch.h"
//#include "../multicast/ipmulticast.h"
#include "../gps/gps.h"
#include "../multicast/cmd_fn.h"
#include "../multicast/cmd_ch.h"

#define DBG_MEM_SZ      3000
#define DBG_MEM_PAGE    500
//static unsigned char DBG_MEM[DBG_MEM_SZ];

void dbg_init()
{
    
    TN_INTSAVE_DATA
    tn_disable_interrupt();
    lm3s90_uart_init();
    //memset(DBG_MEM, 0, DBG_MEM_SZ);
    tn_enable_interrupt();
    
}
//--------------------------------------
void dbg_get_str(int page, char *buf)
{
  /*
  if (page<(DBG_MEM_SZ/DBG_MEM_PAGE))
   memcpy(buf,&DBG_MEM[DBG_MEM_PAGE*page],DBG_MEM_PAGE);
  else
   strcpy(buf,"OUT of range in page");   
  */
}
//--------------------------------------
void dbg_free()
{
    /*
    TN_INTSAVE_DATA
    tn_disable_interrupt();
    lm3s90_uart_free();
    tn_enable_interrupt();
    */
}

void dbg_puts(char const* s)
{
  /*
   int d_size = strlen((const char*)DBG_MEM);
   int s_size = strlen((const char*)s);
   ////
    if (d_size+s_size<DBG_MEM_SZ)
      strcat((char*)DBG_MEM,s);
 */ 
  
  
/*
  return;
lm3s90_uart_out(s);
lm3s90_uart_out("\n");
debugee(s);
 */ 
//    Send_Data_uart(s, strlen(s));    
//    Send_Data_uart("\n", 1);    
    
//return;
#ifndef BOOT_LOADER
static BOOL flags = FALSE;
while(flags)
    tn_task_sleep(5);
flags = TRUE;
#endif
    
    TN_INTSAVE_DATA
    tn_disable_interrupt();
    lm3s90_uart_out(s);
    lm3s90_uart_out("\n");
    tn_enable_interrupt();
#ifndef BOOT_LOADER
flags = FALSE;
#endif

}


void dbg_printf(char const* fmt, ... )
{
   //int d_size = strlen((const char*)DBG_MEM);
   ////
    static char buf[256];
    memset(buf, 0, sizeof(buf));
    va_list arg;
    va_start(arg, fmt);
        
    vsnprintf(buf, sizeof(buf) - 1, fmt, arg); // vsnprintf not write end zero
    va_end(arg);
    ///
    int s_size = strlen(buf);
   
    //if (d_size+s_size<DBG_MEM_SZ)
    //  strcat((char*)DBG_MEM,buf);
  
//  return;
////////////////////////////
  /*
#ifndef BOOT_LOADER
static BOOL flags = FALSE;
while(flags)
   tn_task_sleep(5);
flags = TRUE;
#endif
    TN_INTSAVE_DATA
    tn_disable_interrupt(); // !!!! Переделать

    static char buf[256];
    memset(buf, 0, sizeof(buf));
    va_list arg;
    va_start(arg, fmt);
        
    vsnprintf(buf, sizeof(buf) - 1, fmt, arg); // vsnprintf not write end zero
    va_end(arg);

    //Send_Data_uart(buf, strlen(buf));
    
    lm3s90_uart_out(buf);
        
    tn_enable_interrupt();
#ifndef BOOT_LOADER
flags = FALSE;
#endif
  */
lm3s90_uart_out(buf);
}
