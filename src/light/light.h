/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __LIGHT_H__
#define __LIGHT_H__

#define DIRECT1 0x0007
#define WALKER1 0x0030
#define ARROW1  0x00C0
#define DIRECT2 0x0700
#define WALKER2 0x3000
#define ARROW2  0xC000

/*----------------------------------------------------------------------------*/
typedef struct _LIGHT_MACHINE
{
    unsigned char   work;
    unsigned char   state;
    unsigned short  light;
}_LIGHT_MACHINE;
/*----------------------------------------------------------------------------*/
extern unsigned char DS_INTS_COUNT;
extern unsigned char WAIT_1S_COUNT;
/*----------------------------------------------------------------------------*/
void light_init(void); //calling from main
BOOL GetLightMachineWork(void);
void SetLightMachineWork(BOOL work);
void SetPWM (unsigned long pwm_rgy);
void GetLightText(char *buf);
void Light_set_event(void);
void SIGNAL_OFF();
BOOL ligh_load_init();
void DK_HALT();
void DK_RESTART();
void POWER_SET(const BOOL stat);
unsigned long retCRC32();
//void External_Buttons(unsigned char button);
//void SetLight (unsigned short light, BOOL flash);
//unsigned short GetLight (void);
//void GetNaprText (char *buf);

#endif // __LIGHT_H__
