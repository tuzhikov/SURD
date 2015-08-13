/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/


#ifndef __XBEEXPRO_H__
#define __XBEEXPRO_H__

#include "digi_utils.h"

void Init_RF_ZB(RF_BUFER *buf_rf);
void rf_zb_service(void);
void Parse_Pack(unsigned char *data);




#endif