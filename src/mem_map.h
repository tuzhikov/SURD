/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __MEM_MAP_H__
#define __MEM_MAP_H__

#include "stellaris.h"

#pragma pack(push, 1)
struct version_t
{
    unsigned char   major;
    unsigned short  minor;
    unsigned long   svn_rev;
    unsigned char   fix_rev;
};
#pragma pack(pop)

// Memory map for lm3s9b90

// Common part

// ROM memory description

#define MEM_ROM_PTR         0
#define MEM_ROM_SZ          0x40000                     // 256 KiB

// RAM memory description

#define MEM_RAM_PTR         0x20000000
#define MEM_RAM_SZ          0x18000                     // 96 KiB

// Vector table description

#define VTABLE_SZ           0x120   // size of lm3s9b90 vector table = 0x120 (see __vector_table[] in startup.c)

// Project part

#define FW_VERSION(ptr)     ((struct version_t*)(ptr))

// Boot loader memory description

#define MEM_BOOT_PTR        MEM_ROM_PTR
#define MEM_BOOT_SZ         0x4000                      // 16 KiB
#define BOOT_VER            FW_VERSION(MEM_BOOT_PTR + VTABLE_SZ)

// Firmware memory description

#define MEM_FW_PTR          MEM_BOOT_SZ
#define MEM_FW_SZ           0x1E000                     // 120 KiB

#ifdef DEBUG
    #define FW_VER              FW_VERSION(MEM_ROM_PTR + VTABLE_SZ)
#else
    #define FW_VER              FW_VERSION(MEM_FW_PTR + VTABLE_SZ)
#endif // DEBUG

// Firmware update buffer description

#define MEM_FW_BUFFER_PTR   (MEM_BOOT_PTR + MEM_BOOT_SZ + MEM_FW_SZ)
#define MEM_FW_BUFFER_SZ    MEM_FW_SZ                   // 120 KiB

#if MEM_BOOT_SZ + MEM_FW_SZ + MEM_FW_BUFFER_SZ != MEM_ROM_SZ
#error mem_map.h error: incorrect memory configuration
#endif

#endif // __MEM_MAP_H__
