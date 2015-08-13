/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __PINS_H__
#define __PINS_H__

#include "tnkernel/tn.h"

enum o_pin
{
    OPIN_TEST2,
    OPIN_TEST1,
    OPIN_CPU_LED,
    OPIN_POWER,
    OPIN_ERR_LED,
    OPIN_SIM900_ON,
    OPIN_TR_EN,
    OPIN_485_PV,
    OPIN_485_RST,
    OPIN_DS1390_CS_CLOCK,

    OPIN_SST25VFXXX_CS_FLASH,          // M25PE8(?M25PE16?) - 1 MiB (?2MiB?) bytes SPI flash memory, chip select (0 - selected, 1 - unselected)
    OPIN_GPS_RESET,

    OPIN_Y_LOAD,
    OPIN_G_LOAD,
    OPIN_R_LOAD,

    OPIN_D3,
    OPIN_D0,
    OPIN_D1,
    OPIN_D2,
    OPIN_D7,
    OPIN_D6,
    OPIN_D5,
    OPIN_D4,

    OPIN_CNT                        // count of used output pin's
};

enum i_pin
{
    IPIN_TVP0,
    IPIN_TVP1,
    IPIN_GPS_PPS_INT,
    IPIN_DS1390_INT,
    IPIN_Y_BLINK,
    IPIN_OFF,
    IPIN_UC1,
    IPIN_UC2,
    IPIN_UC3,
    IPIN_UC4,
    IPIN_UC5,
    IPIN_UC6,
    IPIN_UC7,
    IPIN_UC8,
    IPIN_PW_CONTR,

    IPIN_CNT                        // count of used input pin's
};
/*----------------------------------------------------------------------------*/
void pins_init();
void opins_init();                  // silent initialize output pins
void opins_free();
void pin_on(enum o_pin pin);
void pin_off(enum o_pin pin);
BOOL pin_rd(enum i_pin pin);

#endif // __PINS_H__
