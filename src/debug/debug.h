/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __DEBUG_CON_H__
#define __DEBUG_CON_H__



void    dbg_init();
void    dbg_free();
void    dbg_puts(char const* s);
void    dbg_printf(char const* fmt, ... );
#define dbg_trace()  dbg_printf("%s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__)
void dbg_get_str(int page, char *buf);


#endif // __DEBUG_CON_H__
