/*****************************************************************************
*
* © 2015 Cyber-SB. Written by Alex Tuzhikov.
*
******************************************************************************/

#include "crc16.h"
#include "types_define.h"

unsigned short CRC16(void* buf, unsigned short len)
{
unsigned short i;
INT_TO_TWO_CHAR cKs;
INT_TO_TWO_CHAR tmp;
unsigned char *pMess = (unsigned char*)buf;
cKs.Int = 0xFFFF;

while(len--)
  {
  cKs.Int^=*pMess;
  for(i=0;i<8;i++)
    {
    if(cKs.Int&0x0001){ // МЛ бит
      cKs.Int>>=1;
      cKs.Int^=0xA001;
      }else{
        cKs.Int>>=1;
        }
    }
    pMess++;

  }
// мл.вперед
tmp.Byte[0] = cKs.Byte[1];
tmp.Byte[1] = cKs.Byte[0];
return tmp.Int;
}