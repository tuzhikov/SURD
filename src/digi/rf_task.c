/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "../tnkernel/tn.h"
#include "../debug/debug.h"
#include "../stellaris.h"
#include "rf_task.h"
#include "xbee868LP.h"
#include "xbeeXpro.h"

#define EVENT_ZB_DATA         1
#define EVENT_868LP_DATA      2

static TN_TCB task_rf_tcb;
#pragma location = ".noinit"
#pragma data_alignment=8

static unsigned int task_rf_stk[TASK_RF_STK_SZ];
static void task_rf_func(void* param);

#define RF_REFRESH_INTERVAL 200
static TN_EVENT         g_rf_evt;



void RF_init()
{
    dbg_printf("Initializing RF...");


    if (tn_event_create(&g_rf_evt, TN_EVENT_ATTR_SINGLE | TN_EVENT_ATTR_CLR, 0) != TERR_NO_ERR)
    {
        dbg_puts("tn_event_create(&g_rf_evt) error");
        dbg_trace();
        tn_halt();
    }    
    
    
    if (tn_task_create(&task_rf_tcb, &task_rf_func, TASK_RF_PRI,
        &task_rf_stk[TASK_RF_STK_SZ - 1], TASK_RF_STK_SZ, 0,
        TN_TASK_START_ON_CREATION) != TERR_NO_ERR)
    {
        dbg_puts("tn_task_create(&task_rf_tcb) error");
        dbg_trace();
        tn_halt();
    }
    

    
    MAP_IntEnable(INT_GPIOC);
    
    MAP_GPIOIntTypeSet(GPIO_PORTC_BASE, GPIO_PIN_4 | GPIO_PIN_5, GPIO_FALLING_EDGE);
    MAP_GPIOPinIntEnable(GPIO_PORTC_BASE, GPIO_PIN_4 | GPIO_PIN_5);     
    
    
    dbg_puts("[done]");
}



static void task_rf_func(void* param)
{
    int err;
    for (;;)
    {
        unsigned int fpattern;
        err = tn_event_wait(&g_rf_evt, EVENT_ZB_DATA | EVENT_868LP_DATA, TN_EVENT_WCOND_OR, &fpattern, RF_REFRESH_INTERVAL);
        
    MAP_GPIOPinIntDisable(GPIO_PORTC_BASE, GPIO_PIN_4 | GPIO_PIN_5);        
        if (err == TERR_TIMEOUT)
        {
            rf_zb_service();
            rf_868LP_service();
        }else
        {
            if (fpattern & EVENT_ZB_DATA)
            {
                rf_zb_service();
            }
        
            if (fpattern & EVENT_868LP_DATA)
            {
                rf_868LP_service();
            }
        }
     MAP_GPIOPinIntEnable(GPIO_PORTC_BASE, GPIO_PIN_4 | GPIO_PIN_5);       
/*
        pin_on(OPIN_DISO_0);
*/
    }
}


void RF_ZB_int_handler(void)
{
    tn_event_iset(&g_rf_evt, EVENT_ZB_DATA);  
}
void RF_868LP_int_handler(void)
{
    tn_event_iset(&g_rf_evt, EVENT_868LP_DATA);    
}