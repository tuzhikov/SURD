/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __M25PEXX_H__
#define __M25PEXX_H__

BOOL m25pexx_init(unsigned size, unsigned long spi_bitrate);
void m25pexx_rdid (unsigned char *id);
void m25pexx_rd(unsigned long addr, unsigned char* buf, unsigned len);
void m25pexx_wr(unsigned long addr, unsigned char* buf, unsigned len);
void m25pexx_erase(void);

#endif // __M25PEXX_H__
