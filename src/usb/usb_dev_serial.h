/*****************************************************************************
*
* © 2014  Written by Serikov Igor.
*
*****************************************************************************/

#ifndef __USM_M_H__
#define __USM_M_H__

int Init_USB();
unsigned long
ControlHandler(void *pvCBData, unsigned long ulEvent,
               unsigned long ulMsgValue, void *pvMsgData);
unsigned long
TxHandler(void *pvCBData, unsigned long ulEvent, unsigned long ulMsgValue,
          void *pvMsgData);
unsigned long
RxHandler(void *pvCBData, unsigned long ulEvent, unsigned long ulMsgValue,
          void *pvMsgData);

#endif // USM_M
