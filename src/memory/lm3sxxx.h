/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __LM3SXXX_H__
#define __LM3SXXX_H__

void lm3sxxx_init(unsigned long start_addr, unsigned size);
void lm3sxxx_rd(unsigned long addr, unsigned char* buf, unsigned len);
void lm3sxxx_wr(unsigned long addr, unsigned char* buf, unsigned len);

#endif // __LM3SXXX_H__
