/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "../tnkernel/tn.h"
#include "../spi.h"
#include "../pins.h"
#include "xbee868LP.h"
#include "digi_utils.h"

#include "../dk/dk.h"
#include "../debug/debug.h"


#define spi_lock()            rf_spi_lock()
#define spi_unlock()          rf_spi_unlock()
#define xbee868LP_select()    pin_off(OPIN_DIGI_CS_868)
#define xbee868LP_unselect()  pin_on(OPIN_DIGI_CS_868)
#define xbee868LP_alert()     !pin_rd(IPIN_DIGI_868_INT)


RF_BUFER buf_rf;


static unsigned short WriteRead(RF_BUFER *buf_rf);

void Init_RF_868LP(RF_BUFER *buf_rf)
{
    buf_rf->tx_b = 0;
}


void rf_868LP_service(void)
{
    static char step = 0;
    unsigned short len, old;
    unsigned char resbuf[256];
    unsigned char addr[8];
    unsigned char ni[8] = {'d','e','v',' ','8','6','8',0};
    
    switch (step)
    {
        case 0:
          pin_on(OPIN_DIGI_RESET_868);
          Init_RF_868LP(&buf_rf);
          step++;
        break;
        case 1:
          buf_rf.tx_b=Xbee_AT_Command_write_string((unsigned char*)buf_rf.tx_buf, 1, 'N'+('I'<<8), ni);
          WriteRead(&buf_rf);          
        
          
          step++;
        break;
        
      
        case 2:

          len = WriteRead(&buf_rf);
          
          old = 0;
          while (len)
          {
              old = find_pack (&buf_rf.rx_buf[old], resbuf, len);

              if (old)
              {
                  Parse_Pack_868LP(resbuf);                
                  len-=old;
              }
              else
                break;
          }
         // step++;
        break;
        
        case 3:
         // buf_rf.tx_b = Xbee_AT_Command_read(buf_rf.tx_buf, 1, 'S'+('L'<<8));
          addr[0] = 0x00;
          addr[1] = 0x13;
          addr[2] = 0xA2;
          addr[3] = 0x00;          
          addr[4] = 0x40;
          addr[5] = 0xB8;
          addr[6] = 0x01;
          addr[7] = 0x13;  
          buf_rf.tx_b=Xbee_TX_Request((unsigned char*)buf_rf.tx_buf,1, addr, 0,0,(unsigned char*)"eeeeeeeeee",10);         
          WriteRead(&buf_rf);
          step++;          
        break;
        
        case 4:
          WriteRead(&buf_rf);
          step=1;
        break;
    }
  
}



static unsigned short WriteRead(RF_BUFER *buf_rf)
{
    unsigned char b;
    unsigned short tx=0;
    unsigned short rx=0;
    
    spi_lock();
    xbee868LP_select();

    while(buf_rf->tx_b || xbee868LP_alert())
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
        
        
        if (xbee868LP_alert())
        {
            buf_rf->rx_buf[rx++] = rf_spi_rw(b);
            if (rx >= SIZE_RF_RXBUF)
                rx--;
        }else
        {
            rf_spi_rw(b);   
        }
    }
    
    xbee868LP_unselect();
    spi_unlock();
    
    return rx;
}


void Parse_Pack_868LP(unsigned char *data)
{
    unsigned char addr[8];
    unsigned short size_data;
    unsigned char res[1024];
    unsigned short Digital_Samples;

    
    switch (data[3])
    {
        case 0x90: // Получены данные
            memcpy(addr,&data[4],8);
            size_data = ((unsigned char)data[1]*256)+(unsigned char)data[2]-12;            
            if (!memcmp(&data[15], "tttttttttt", 10))
            {
                buf_rf.tx_b=Xbee_TX_Request((unsigned char*)buf_rf.tx_buf,1, addr, 0,0,(unsigned char*)"eeeeeeeeee",size_data);              
                WriteRead(&buf_rf);              
            }else
            {
		
		for(int i = 0; i<size_data; i++)
                   res[i] = data[i+15]+1;
                buf_rf.tx_b=Xbee_TX_Request((unsigned char*)buf_rf.tx_buf,1, addr, 0,0,res,size_data);
		WriteRead(&buf_rf);              
            }
        break;
        
        
        case 0x92:
            memcpy(addr,&data[4],8);
            Digital_Samples = (data[19]<<8) + data[20];
            
            if ((Digital_Samples & 0x0001) == 0x0000)
              External_Buttons(0);
              
            if ((Digital_Samples & 0x0002) == 0x0000)
              External_Buttons(1);

        break;
    }
  
}
