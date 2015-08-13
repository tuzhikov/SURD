/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __STELLARIS_H__
#define __STELLARIS_H__

#include <inc/hw_adc.h>
#include <inc/hw_can.h>
#include <inc/hw_comp.h>
#include <inc/hw_epi.h>
#include <inc/hw_ethernet.h>
#include <inc/hw_flash.h>
#include <inc/hw_gpio.h>
#include <inc/hw_hibernate.h>
#include <inc/hw_i2c.h>
#include <inc/hw_i2s.h>
#include <inc/hw_ints.h>
#include <inc/hw_memmap.h>
#include <inc/hw_nvic.h>
#include <inc/hw_pwm.h>
#include <inc/hw_qei.h>
#include <inc/hw_ssi.h>
#include <inc/hw_sysctl.h>
#include <inc/hw_timer.h>
#include <inc/hw_types.h>
#include <inc/hw_uart.h>
#include <inc/hw_udma.h>
#include <inc/hw_usb.h>
#include <inc/hw_watchdog.h>

#include <driverlib/adc.h>
#include <driverlib/can.h>
#include <driverlib/comp.h>
#include <driverlib/cpu.h>
#include <driverlib/debug.h>
#include <driverlib/epi.h>
#include <driverlib/ethernet.h>
#include <driverlib/flash.h>
#include <driverlib/gpio.h>
#include <driverlib/hibernate.h>
#include <driverlib/i2c.h>
#include <driverlib/i2s.h>
#include <driverlib/interrupt.h>
#include <driverlib/mpu.h>
#include <driverlib/pin_map.h>
#include <driverlib/pwm.h>
#include <driverlib/qei.h>
#include <driverlib/ssi.h>
#include <driverlib/sysctl.h>
#include <driverlib/systick.h>
#include <driverlib/timer.h>
#include <driverlib/uart.h>
#include <driverlib/udma.h>
#include <driverlib/usb.h>
#include <driverlib/watchdog.h>

// for use driverlib functions in the ROM, add this predefine to the project settings:
// TARGET_IS_TEMPEST_RXX, where XX - chip revision (for example: TARGET_IS_TEMPEST_RC3)
#include <driverlib/rom.h> // lm3s9b90 is have Stellaris ROM
#include <driverlib/rom_map.h>

#endif // __STELLARIS_H__
