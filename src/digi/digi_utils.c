/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include <string.h>
#include "../tnkernel/tn.h"

unsigned char calc_CRC(unsigned char *data, unsigned short n)
{
	unsigned char crc=data[0];
	for (unsigned long i = 1; i< n; i++)
		crc += data[i];
	return 0xFF-crc;
}

unsigned short escape_on(unsigned char *data)
{
	unsigned char buf[600];
	unsigned short p = 0;
	unsigned short i;
	unsigned short size = data[1]*256+data[2]+4;
	for ( i = 1; i<size; i++)
	{
            if (data[i] == 0x7E || data[i] == 0x7D || data[i] == 0x11 || data[i] == 0x13)
            {
		buf[p++] = 0x7D;
		buf[p++] = data[i] ^ 0x20;
            }
            else
            {
		buf[p++] = data[i];
            }
	}
	for ( i = 0; i<p ; i++)
		data[1+i] = buf[i];
	return p+1; //Вернём итоговую длинну пакета
}

void escape_off(unsigned char *data)
{
	unsigned short p = 1;
	unsigned long i;
        unsigned short size = 0xFFFF;

	for (i = 1; p<size; i++)
	{
            if (data[i] == 0x7D)
            {
		data[p++] = data[++i]^0x20;
            }else
            {
		data[p++] = data[i];
            }

            if (p == 3)
                size = (data[2]+256)+data[1];
	}
	data[p] = data[i]; //CRC
}

unsigned short make_pack(unsigned char *data)
{
	data[3+(unsigned long)data[1]*256+data[2]] = calc_CRC(&data[3], data[1]*256+data[2]);
	return data[1]*256+data[2]+4;
          //escape_on(data);
}

unsigned short Xbee_AT_Command_read(unsigned char *buf, unsigned char Frame_ID, unsigned short AT_Comand)
{
	buf[0] = 0x7E;
	buf[1] = 0x00;
	buf[2] = 0x04;
	buf[3] = 0x08;  // Frame_type
	buf[4] = Frame_ID;
	buf[5] = AT_Comand & 0xFF;
	buf[6] = AT_Comand >> 8;
	return make_pack(buf);
}

unsigned short Xbee_TX_Request(unsigned char *buf, unsigned char Frame_ID, unsigned char *addres, unsigned char Broadcast, unsigned char options, unsigned char *data, unsigned char n)
{
	buf[0] = 0x7E;
	buf[1] = 0x00;
	buf[2] = 0x0E + n;
	buf[3] = 0x10;  // Frame_type
	buf[4] = Frame_ID;
	buf[5] = *addres++;
	buf[6] = *addres++;
	buf[7] = *addres++;
	buf[8] = *addres++;
	buf[9] = *addres++;
	buf[10] = *addres++;
	buf[11] = *addres++;
	buf[12] = *addres++;
	buf[13] = 0xFF;
	buf[14] = 0xFE;
	buf[15] = Broadcast;
	buf[16] = options;
	for (unsigned char i = 0; i<n; i++)
		buf[17+i] = data[i];

	return make_pack(buf);
}

unsigned short Xbee_AT_Command_write(unsigned char *buf, unsigned char Frame_ID, unsigned short AT_Comand, unsigned char data)
{
	buf[0] = 0x7E;
	buf[1] = 0x00;
	buf[2] = 0x05;
	buf[3] = 0x08;  // Frame_type
	buf[4] = Frame_ID;
	buf[5] = AT_Comand & 0xFF;
	buf[6] = AT_Comand >> 8;
	buf[7] = data;
	return make_pack(buf);
}

unsigned short Xbee_AT_Command_write_short(unsigned char *buf, unsigned char Frame_ID, unsigned short AT_Comand, unsigned short data)
{
	buf[0] = 0x7E;
	buf[1] = 0x00;
	buf[2] = 0x06;
	buf[3] = 0x08;  // Frame_type
	buf[4] = Frame_ID;
	buf[5] = AT_Comand & 0xFF;
	buf[6] = AT_Comand >> 8;
	buf[7] = data >> 8 ;
	buf[8] = data & 0xFF ;
	return make_pack(buf);
}


unsigned short Xbee_AT_Command_write_string(unsigned char *buf, unsigned char Frame_ID, unsigned short AT_Comand, unsigned char * data)
{
	unsigned char size = strlen((const char*)data);
	buf[0] = 0x7E;
	buf[1] = 0x00;
	buf[2] = 0x04 + size;
	buf[3] = 0x08;  // Frame_type
	buf[4] = Frame_ID;
	buf[5] = AT_Comand & 0xFF;
	buf[6] = AT_Comand >> 8;
	for (unsigned char i = 7; size; size--)
		buf[i++] = *data++;
	return make_pack(buf);
}



unsigned short find_pack (unsigned char *sbuf, unsigned char *rbuf, unsigned short lenght)
{
    unsigned short read = 0;
    unsigned short write;

    unsigned char step = 0;
    unsigned short size;


    while (read < lenght)
    {

        switch (step)
        {
            case 0:   //Ищем начало пакета
                if (sbuf[read++] == 0x7E)
                {
                    write = 0;
                    rbuf[write++] = 0x7E;
                    step = 1;
                }
            break;

            case 1: // Собираем тело пакета

                rbuf[write++] = sbuf[read++];


                if (write == 3)
                    size = rbuf[2] + rbuf[1]*256;

                if (write >= size+3 && calc_CRC(&rbuf[3],size) == rbuf[write-1])
                    return read;

            break;

        }
    }

    return 0;
}


