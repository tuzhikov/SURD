/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __DIGI_UTILS_H__
#define __DIGI_UTILS_H__

#define SIZE_RF_RXBUF 512

typedef struct _RF_BUFER
{
    unsigned short tx_b;
    unsigned char tx_buf[SIZE_RF_RXBUF];
    unsigned char rx_buf[SIZE_RF_RXBUF];
}RF_BUFER;



unsigned char calc_CRC(unsigned char *data, unsigned short n);

unsigned short escape_on(unsigned char *data);
unsigned short make_pack(unsigned char *data);
void escape_off(unsigned char *data);
unsigned short Xbee_AT_Command_read(unsigned char *buf, unsigned char Frame_ID, unsigned short AT_Comand);
unsigned short Xbee_TX_Request(unsigned char *buf, unsigned char Frame_ID, unsigned char *addres, unsigned char Broadcast, unsigned char options, unsigned char *data, unsigned char n);
unsigned short Xbee_AT_Command_write(unsigned char *buf, unsigned char Frame_ID, unsigned short AT_Comand, unsigned char data);
unsigned short Xbee_AT_Command_write_string(unsigned char *buf, unsigned char Frame_ID, unsigned short AT_Comand, unsigned char * data);
unsigned short Xbee_AT_Command_write_short(unsigned char *buf, unsigned char Frame_ID, unsigned short AT_Comand, unsigned short data);
unsigned short find_pack (unsigned char *sbuf, unsigned char *rbuf, unsigned short lenght);

#endif