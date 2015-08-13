/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "../stellaris.h"   // for HWREG()
#include "../crc32.h"
#include "../mem_map.h"
#include "memory.h"
#include "firmware.h"
#include "../tnkernel/tn.h"
#include "../version.h"


#define FIRMWARE_INVALID_VALUE  0xFFFFFFFF

int firmware_write(unsigned long addr, unsigned char* buf, unsigned len)
{
    if (len > FIRMWARE_MAX_PKT_LEN || FIRMWARE_HDR_SIZE + addr + len > iflash_sz(IFLASH_FIRMWARE_BUFFER))
        return FIRMWARE_ERR_WRONG_PARAM;

    iflash_wr(IFLASH_FIRMWARE_BUFFER, FIRMWARE_HDR_SIZE + addr, buf, len);
    return FIRMWARE_ERR_NO_ERR;
}

int firmware_read(unsigned long addr, unsigned char* buf, unsigned len)
{
    iflash_rd(IFLASH_FIRMWARE_BUFFER, FIRMWARE_HDR_SIZE + addr, buf, len);
    return FIRMWARE_ERR_NO_ERR;
}


int firmware_write_nfo(unsigned long crc32, unsigned long fw_len)
{
    if (FIRMWARE_HDR_SIZE + fw_len > iflash_sz(IFLASH_FIRMWARE_BUFFER))
        return FIRMWARE_ERR_WRONG_PARAM;

    unsigned long buf[2] = { crc32, fw_len };
    iflash_wr(IFLASH_FIRMWARE_BUFFER, 0, (unsigned char*)buf, FIRMWARE_HDR_SIZE);
    return FIRMWARE_ERR_NO_ERR;
}

BOOL firmware_find()
{
    unsigned long const crc32   = HWREG(MEM_FW_BUFFER_PTR);
    unsigned long const fw_len  = HWREG(MEM_FW_BUFFER_PTR + sizeof (crc32));

    // check firmware nfo
    if (crc32 == FIRMWARE_INVALID_VALUE && (fw_len == FIRMWARE_INVALID_VALUE || fw_len <= VTABLE_SZ || fw_len > MEM_FW_BUFFER_SZ - FIRMWARE_HDR_SIZE))
        return FALSE;

    // check crc32
    return crc32_calc((unsigned char*)(MEM_FW_BUFFER_PTR + FIRMWARE_HDR_SIZE), fw_len) == crc32;
}

void firmware_update()
{
    iflash_wr(IFLASH_FIRMWARE, 0, (unsigned char*)(MEM_FW_BUFFER_PTR + FIRMWARE_HDR_SIZE),
        HWREG(MEM_FW_BUFFER_PTR + FIRMWARE_HDR_SIZE / 2));

    unsigned long const buf[2] = { FIRMWARE_INVALID_VALUE, FIRMWARE_INVALID_VALUE };
    iflash_wr(IFLASH_FIRMWARE_BUFFER, 0, (unsigned char*)buf, FIRMWARE_HDR_SIZE);
}


BOOL firmware_ver_chek()
{

#ifdef FIRMWARE_CONTROL
    unsigned char ver;
    
    iflash_rd(IFLASH_FIRMWARE_BUFFER, FIRMWARE_HDR_SIZE + VTABLE_SZ, (unsigned char*)&ver, sizeof(ver));
    if (ver == (unsigned char)FW_VER->major)
        return TRUE;
    else
        return FALSE;
#else    
    return TRUE;
#endif

}