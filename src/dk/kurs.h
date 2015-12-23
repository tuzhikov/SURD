#ifndef kursH
#define kursH

#include "dk.h"

/*----------------------------------------------------------------------------*/
/*external variable*/
/*----------------------------------------------------------------------------*/
extern     BYTE     GREEN_PORT;
extern     BYTE     RED_PORT;
extern     BYTE     YEL_PORT;

extern     BYTE     GR_PORT_NET;
extern     BYTE     RD_PORT_NET;
extern     BYTE     YL_PORT_NET;

extern     BYTE     GREEN_PORT_CONF;
extern     BYTE     RED_PORT_CONF;
extern     BYTE     YEL_PORT_CONF;

extern     BYTE     GREEN_PORT_ERR;
extern     BYTE     RED_PORT_ERR;

/*----------------------------------------------------------------------------*/
/*External functios*/
/*----------------------------------------------------------------------------*/
void  Update_STATES_KURS(const bool flash,const bool fUdzON);
void  Set_LED(int group, int gr_col,BOOL stat);
void  SET_OUTPUTS();
void Clear_LED();
void Prepare_KURS_Structures();
/*----------------------------------------------------------------------------*/
/*End*/
/*----------------------------------------------------------------------------*/
#endif