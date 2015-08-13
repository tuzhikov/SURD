/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/


#ifndef __XBEE868LP_H__
#define __XBEE868L_H__

#include "digi_utils.h"



void Init_RF_868LP(RF_BUFER *buf_rf );
void rf_868LP_service(void);


void Parse_Pack_868LP(unsigned char *data);


#endif