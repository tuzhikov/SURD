/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __SST25VFXXX_H__
#define __SST25VFXXX_H__


#define FLASH_SZ            (4 * 1024 * 1024)


BOOL sst25vfxxx_init(unsigned size, unsigned long spi_bitrate);
void sst25vfxxx_rdid (unsigned char *id);
void sst25vfxxx_rd(unsigned long addr, unsigned char* buf, unsigned len);
void sst25vfxxx_wr(unsigned long addr, unsigned char* buf, unsigned len);
void sst25vfxxx_erase(void);

#endif // __SST25VFXXX_H__