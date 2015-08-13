/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __FIRMWARE_H__
#define __FIRMWARE_H__

#include "../tnkernel/tn.h"

#define FIRMWARE_MAX_PKT_LEN        512
#define FIRMWARE_HDR_SIZE           8

#define FIRMWARE_ERR_NO_ERR         0
#define FIRMWARE_ERR_WRONG_PARAM    1
#define FIRMWARE_ERR_UNKNOWN        2

// all functions in current module working after call:
// dbg_init(), see debug/debug.h
// crc32_init(), see crc32.h
// opins_init(), see pins.h

// this functions working after call memory_init() function, see memory/memory.h

int firmware_write(unsigned long addr, unsigned char* buf, unsigned len);
int firmware_read(unsigned long addr, unsigned char* buf, unsigned len);
int firmware_write_nfo(unsigned long crc32, unsigned long fw_len);

// this functions working after call iflash_init() function, see memory/memory.h
// use this functions in boot loader only

BOOL firmware_find();
void firmware_update();
BOOL firmware_ver_chek();

#endif // __FIRMWARE_H__
