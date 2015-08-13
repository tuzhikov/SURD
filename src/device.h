/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
******************************************************************************/

#ifndef __DEVICE_H__
#define __DEVICE_H__


enum type_dev
{
    DEVICE_MASTER,
    DEVICE_SLAVE
};

typedef struct _DEVICE
{
   enum type_dev device_type;
   unsigned char addr_digi_868[8];
   unsigned char addr_digi_zb[8];
   unsigned char GPS;
   unsigned char SIM900;
   unsigned char DS1390;

} DEVICE;


#endif