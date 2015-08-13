/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/
#include <string.h>
#include "../tnkernel/tn.h"
#include "../spi.h"
#include "../pins.h"
#include "xbeeXpro.h"
#include "digi_utils.h"

#include "../debug/debug.h"


#define spi_lock()           rf_spi_lock()
#define spi_unlock()         rf_spi_unlock()
#define xbeeXpro_select()    pin_off(OPIN_DIGI_CS_ZB)
#define xbeeXpro_unselect()  pin_on(OPIN_DIGI_CS_ZB)
#define xbeeXpro_alert()     !pin_rd(IPIN_DIGI_ZB_INT)


RF_BUFER buf_rf_Xpro;



static unsigned short WriteRead(RF_BUFER *buf_rf);



void Init_RF_ZB(RF_BUFER *buf_rf)
{
    buf_rf->tx_b = 0;
}


void rf_zb_service(void)
{
    static char step = 0;
    unsigned short len, old;
    unsigned char resbuf[256];
    unsigned char ni[8] = {'d','e','v',' ','Z','B',0};
    
    switch (step)
    {
        case 0:
          pin_on(OPIN_DIGI_RESET_ZB);          
          tn_task_sleep(500);
          step++;
        break;
        case 1:

          Init_RF_ZB(&buf_rf_Xpro);
          
          buf_rf_Xpro.tx_b=Xbee_AT_Command_read((unsigned char*)buf_rf_Xpro.tx_buf, 1, 'N'+('I'<<8));
          WriteRead(&buf_rf_Xpro); 
          tn_task_sleep(1000);
          buf_rf_Xpro.tx_b=Xbee_AT_Command_write_string((unsigned char*)buf_rf_Xpro.tx_buf, 1, 'N'+('I'<<8), ni);
          WriteRead(&buf_rf_Xpro);
          tn_task_sleep(500);
          buf_rf_Xpro.tx_b=Xbee_AT_Command_write_short((unsigned char*)buf_rf_Xpro.tx_buf, 1, 'I'+('D'<<8), 0x1230);
          WriteRead(&buf_rf_Xpro);
          tn_task_sleep(500);          
          buf_rf_Xpro.tx_b=Xbee_AT_Command_write((unsigned char*)buf_rf_Xpro.tx_buf, 1, 'Z'+('S'<<8), 2);
          WriteRead(&buf_rf_Xpro);
          tn_task_sleep(500);
          buf_rf_Xpro.tx_b=Xbee_AT_Command_write((unsigned char*)buf_rf_Xpro.tx_buf, 1, 'C'+('E'<<8), 0);
          WriteRead(&buf_rf_Xpro);          
          step++;
        break;
        
      
        case 2:

          len = WriteRead(&buf_rf_Xpro);
          
          old = 0;
          while (len)
          {
              old = find_pack (&buf_rf_Xpro.rx_buf[old], resbuf, len);

              if (old)
              {
                  Parse_Pack(resbuf);                
                  len-=old;
              }
              else
                break;
          }
         // step++;
        break;
        
        case 3:
          buf_rf_Xpro.tx_b = Xbee_AT_Command_read(buf_rf_Xpro.tx_buf, 1, 'S'+('L'<<8));
     //     WriteRead(&buf_rf_Xpro);
          step++;          
        break;
        
        case 4:
    //      WriteRead(&buf_rf_Xpro);
          step=1;
        break;
    }
  
}



unsigned short WriteRead(RF_BUFER *buf_rf)
{
    unsigned char b;
    unsigned short tx=0;
    unsigned short rx=0;
    
    spi_lock();
    xbeeXpro_select();

    while(buf_rf->tx_b || xbeeXpro_alert())
    {
        if (buf_rf->tx_b)
        {
            b = buf_rf->tx_buf[tx];
            tx++;
            buf_rf->tx_b--;
        }else
        {
            b = 0xFF;
        }
        
        
        if (xbeeXpro_alert())
        {
            buf_rf->rx_buf[rx++] = rf_spi_rw(b);
            if (rx >= SIZE_RF_RXBUF)
                rx--;
        }else
        {
            rf_spi_rw(b);   
        }
    }
    
    xbeeXpro_unselect();
    spi_unlock();
    
    return rx;
}

void Parse_Pack(unsigned char *data)
{
    unsigned char addr[8];
    unsigned short size_data;
    unsigned char res[256];
    
    switch (data[3])
    {
        case 0x90: // Получены данные
            memcpy(addr,&data[4],8);
            size_data = ((unsigned char)data[1]*256)+(unsigned char)data[2]-12;            
            if (size_data >=10 && !memcmp(&data[15], "tttttttttt", 10))
            {
                buf_rf_Xpro.tx_b=Xbee_TX_Request((unsigned char*)buf_rf_Xpro.tx_buf,1, addr, 0,0,(unsigned char*)"eeeeeeeeee",size_data);              
                WriteRead(&buf_rf_Xpro);              
            }else
            {
		
		for (int i = 0; i<size_data; i++)
                    res[i] = data[i+15]+1;
                buf_rf_Xpro.tx_b=Xbee_TX_Request((unsigned char*)buf_rf_Xpro.tx_buf,1, addr, 0,0,res,size_data);
		tn_task_sleep(2);
                WriteRead(&buf_rf_Xpro);              
            }
        break;
    }
  
}
