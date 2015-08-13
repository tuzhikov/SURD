/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
******************************************************************************/

#ifndef __CRC32_H__
#define __CRC32_H__

#ifdef __cplusplus
extern "C" {
#endif
void            crc32_init();
unsigned long   crc32_calc(unsigned char* buf, unsigned len);
#ifdef __cplusplus
}1
#endif

#endif // __CRC32_H__
