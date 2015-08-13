/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "../tnkernel/tn.h"
#include "../mem_map.h"
#include "sst25vfxxx.h"
#include "lm3sxxx.h"
#include "memory.h"

#include "../debug/debug.h"



#define struct_region_nfo_t(n)      \
struct n ## _region_nfo_t           \
{                                   \
    enum n ## _region_t rg_id;      \
    unsigned long       rg_start;   \
    unsigned long       rg_size;    \
}

struct_region_nfo_t(flash);
struct_region_nfo_t(iflash);

static struct flash_region_nfo_t const g_flash[FLASH_CNT] =
{
    { FLASH_PREF,              0,                 FLASH_PREF_SIZE },
    { FLASH_PROGRAM,           FLASH_PREF_SIZE,   FLASH_PROGRAM_SIZE },
    { FLASH_PROGS,             FLASH_PREF_SIZE+FLASH_PROGRAM_SIZE,   FLASH_PROGS_SIZE},    
    { FLASH_EVENT,             FLASH_PREF_SIZE+FLASH_PROGRAM_SIZE+FLASH_PROGS_SIZE,   FLASH_EVENT_SIZE},
    
    
    { FLASH_FREE,              FLASH_PREF_SIZE+FLASH_PROGRAM_SIZE+FLASH_EVENT_SIZE+FLASH_PROGS_SIZE,
          FLASH_SZ - FLASH_PREF_SIZE - FLASH_PROGRAM_SIZE - FLASH_EVENT_SIZE -FLASH_PROGS_SIZE}
};

static struct iflash_region_nfo_t const g_iflash[IFLASH_CNT] =
{
    { IFLASH_FIRMWARE,          0,                  MEM_FW_SZ },
    { IFLASH_FIRMWARE_BUFFER,   MEM_FW_SZ,          MEM_FW_BUFFER_SZ }
};

unsigned char flash_init();
// âåðíóòü àäðåñ 
unsigned long get_region_start(enum flash_region_t rg)
{
  return (g_flash[rg].rg_start);
}
//
unsigned char memory_init()
{
    unsigned char err=0;
    err |= flash_init() << 1;
    iflash_init();
    return err;
}

// FLASH

static unsigned char flash_init()
{
    unsigned char id[3];
    dbg_printf("Initializing %u KiB FLASH...", FLASH_SZ / 1024);
    if (sst25vfxxx_init(FLASH_SZ, 5000000))    // 4096 KiB FLASH, 5000000 bps, spi1 and pins used
    {
        dbg_puts("[done]");
        sst25vfxxx_rdid (id);
        dbg_printf("Manufacturer’s ID %X, Memory Type %X, Memory Capacity %X\n", id[0],id[1],id[2]);
        return 0;
    }

    dbg_puts("[false]");
    return 1;
}

unsigned flash_sz(enum flash_region_t rg)
{
    return g_flash[rg].rg_size;
}

void flash_rd(enum flash_region_t rg, unsigned long addr, unsigned char* buf, unsigned len)
{
    if (addr + len > g_flash[rg].rg_size)
    {
        dbg_printf("addr + len > g_flash[%u].rg_size\n", rg);
        dbg_trace();
        tn_halt();
    }

    sst25vfxxx_rd(g_flash[rg].rg_start + addr, buf, len);
}

void flash_rd_direct(unsigned long addr, unsigned char* buf, unsigned len)
{
    sst25vfxxx_rd(addr, buf, len);
}

void flash_wr(enum flash_region_t rg, unsigned long addr, unsigned char* buf, unsigned len)
{
    if (addr + len > g_flash[rg].rg_size)
    {
        dbg_printf("addr + len > g_flash[%u].rg_size\n", rg);
        dbg_trace();
        tn_halt();
    }

    sst25vfxxx_wr(g_flash[rg].rg_start + addr, buf, len);
}

void flash_wr_direct(unsigned long addr, unsigned char* buf, unsigned len)
{
    sst25vfxxx_wr(addr, buf, len);
}

void flash_erase(void)
{
    sst25vfxxx_erase();
}

// IFLASH

void iflash_init()
{
    dbg_printf("Initializing %u KiB IFLASH...", MEM_FW_BUFFER_SZ / 1024);
    lm3sxxx_init(MEM_FW_PTR, MEM_FW_SZ + MEM_FW_BUFFER_SZ);
    dbg_puts("[done]");
}

unsigned iflash_sz(enum iflash_region_t rg)
{
    return g_iflash[rg].rg_size;
}

void iflash_rd(enum iflash_region_t rg, unsigned long addr, unsigned char* buf, unsigned len)
{
    if (addr + len > g_iflash[rg].rg_size)
    {
        dbg_printf("addr + len > g_iflash[%u].rg_size\n", rg);
        dbg_trace();
        tn_halt();
    }

    lm3sxxx_rd(g_iflash[rg].rg_start + addr, buf, len);
}

void iflash_wr(enum iflash_region_t rg, unsigned long addr, unsigned char* buf, unsigned len)
{
    if (addr + len > g_iflash[rg].rg_size)
    {
        dbg_printf("addr + len > g_iflash[%u].rg_size\n", rg);
        dbg_trace();
        tn_halt();
    }

    lm3sxxx_wr(g_iflash[rg].rg_start + addr, buf, len);
}
