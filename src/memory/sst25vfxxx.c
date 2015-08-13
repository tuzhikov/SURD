/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "../tnkernel/tn.h"
#include "../spi.h"
#include "../pins.h"
#include "sst25vfxxx.h"

#include "../debug/debug.h"

#define SST25VFXXX_PAGE_SIZE         1024*4
#define SST25VFXXX_ADDR_SIZE         3
#define SST25VFXXX_PAGE_MAX_SIZE    (1 << M25PEXX_ADDR_SIZE * 8)

#define SST25VFXXX_WREN        0x06    // Write-Enable
#define SST25VFXXX_WRDI        0x04    // Write-Disable
#define SST25VFXXX_RDSR        0x05    // Read-Status-Register
#define SST25VFXXX_EWSR        0x50    // Enable-Write-Status-Register
#define SST25VFXXX_WRSR        0x01    // Write-Status-Register

#define SST25VFXXX_READ        0x03    // Read Memory

#define SST25VFXXX_BTPR        0x02    // To Program One Data Byte
#define SST25VFXXX_AAIP        0xAD    // Auto Address Increment Programming

#define SST25VFXXX_ERASE_4K    0x20    // Erase 4 KByte of memory array
#define SST25VFXXX_ERASE_32K   0x52    // Erase 32 KByte of memory array
#define SST25VFXXX_ERASE_64K   0xD8    // Erase 64 KByte of memory array
#define SST25VFXXX_ERASE_FULL  0xC7    // Erase Full Memory Array

#define SST25VFXXX_RDID        0x9F    // JEDEC Read-ID

#define SST25VFXXX_SR_BUSY     0x01    // WIP, write in progress
#define SST25VFXXX_SR_WREN     0x02    // WEL, write enable latch
#define SST25VFXXX_SR_BP       0x3C    // BP0 - BP3

#define spi_lock()             memory_spi_lock_br(g_sst25vfxxx_spi_bitrate)
#define spi_unlock()           memory_spi_unlock()
#define sst25vfxxx_select()    pin_off(OPIN_SST25VFXXX_CS_FLASH)
#define sst25vfxxx_unselect()  pin_on(OPIN_SST25VFXXX_CS_FLASH)

static unsigned long        g_sst25vfxxx_size;
static unsigned long        g_sst25vfxxx_spi_bitrate;
static unsigned char        g_sst25vfxxx_page_buf[SST25VFXXX_PAGE_SIZE];

// local functions

static void                 sst25vfxxx_wr_pg(unsigned long addr, unsigned char* buf, unsigned len);
static unsigned char        sst25vfxxx_get_status();

BOOL sst25vfxxx_init(unsigned size, unsigned long spi_bitrate)
{
    unsigned char count = 10;
    
    if (size > FLASH_SZ)
    {
        dbg_puts("size > 0x00FFFFFF");
        dbg_trace();
        tn_halt();
    }

    g_sst25vfxxx_size          = size;
    g_sst25vfxxx_spi_bitrate   = spi_bitrate;

    spi_lock();
    while (sst25vfxxx_get_status() & SST25VFXXX_SR_BUSY && --count);
    spi_unlock();
    
    return (BOOL)count;
}

void sst25vfxxx_rdid (unsigned char *id)
{

    spi_lock();
    while (sst25vfxxx_get_status() & SST25VFXXX_SR_BUSY);

    sst25vfxxx_select();
    memory_spi_rw(SST25VFXXX_RDID);
    *id++ = memory_spi_rw(0xFF);
    *id++ = memory_spi_rw(0xFF);
    *id++ = memory_spi_rw(0xFF);
    sst25vfxxx_unselect();
    memory_spi_rw(0xFF); // pause after chip select disable    
    spi_unlock();    
}
//------------------------------------------------------------------------------
void sst25vfxxx_ERASE_FULL()
{
    spi_lock();
    while (sst25vfxxx_get_status() & SST25VFXXX_SR_BUSY);

    sst25vfxxx_select();
    memory_spi_rw(SST25VFXXX_ERASE_FULL);
    ///
    sst25vfxxx_unselect();
    memory_spi_rw(0xFF); // pause after chip select disable    
    spi_unlock();    
}


//------------------------------------------------------------------------------

void sst25vfxxx_rd(unsigned long addr, unsigned char* buf, unsigned len)
{
    if (addr + len > g_sst25vfxxx_size)
    {
        dbg_puts("addr + len > g_sst25vfxxx_size");
        dbg_trace();
        tn_halt();
    }

    spi_lock();
    while (sst25vfxxx_get_status() & SST25VFXXX_SR_BUSY);

    sst25vfxxx_select();
    memory_spi_rw(SST25VFXXX_READ);
    memory_spi_rw((unsigned char)(addr >> 16));
    memory_spi_rw((unsigned char)(addr >> 8));
    memory_spi_rw((unsigned char)(addr));

    while (len--)
        *buf++ = memory_spi_rw(0xFF);

    sst25vfxxx_unselect();
    spi_unlock();
}

void sst25vfxxx_wr(unsigned long addr, unsigned char* buf, unsigned len)
{
    if (addr + len > g_sst25vfxxx_size)
    {
        dbg_puts("addr + len > g_sst25vfxxx_size");
        dbg_trace();
        tn_halt();
    }

    unsigned long const fp_len = SST25VFXXX_PAGE_SIZE - (addr & SST25VFXXX_PAGE_SIZE - 1);
    if (fp_len != SST25VFXXX_PAGE_SIZE)
    {
        if (len >= fp_len)
        {
            sst25vfxxx_wr_pg(addr, buf, fp_len);
            addr    += fp_len;
            buf     += fp_len;
            len     -= fp_len;
        }
        else
        {
            sst25vfxxx_wr_pg(addr, buf, len);
            addr    += len;
            buf     += len;
            len     -= len;
        }
    }

    while (len >= SST25VFXXX_PAGE_SIZE)
    {
        sst25vfxxx_wr_pg(addr, buf, SST25VFXXX_PAGE_SIZE);
        addr    += SST25VFXXX_PAGE_SIZE;
        buf     += SST25VFXXX_PAGE_SIZE;
        len     -= SST25VFXXX_PAGE_SIZE;
    }

    if (len != 0)
        sst25vfxxx_wr_pg(addr, buf, len);
}

void sst25vfxxx_erase(void)
{
    unsigned char status;

    spi_lock();
    while ((status = sst25vfxxx_get_status()) & SST25VFXXX_SR_BUSY);

    if ((status & SST25VFXXX_SR_WREN) == 0)
    {
        sst25vfxxx_select();
        memory_spi_rw(SST25VFXXX_WREN);
        sst25vfxxx_unselect();
        memory_spi_rw(0xFF); 
    }
    
    sst25vfxxx_select();
    memory_spi_rw(SST25VFXXX_ERASE_FULL);
    
    sst25vfxxx_unselect();
    spi_unlock();
}

// local functions

static void sst25vfxxx_wr_pg(unsigned long addr, unsigned char* buf, unsigned len)
{

    unsigned char status;
    unsigned long i = addr & (SST25VFXXX_PAGE_SIZE-1);
    unsigned long addr_page = addr & ~(SST25VFXXX_PAGE_SIZE-1);
    unsigned long p=0;
    
    spi_lock();
    
 //   sst25vfxxx_rd( addr_page, g_sst25vfxxx_page_buf, SST25VFXXX_PAGE_SIZE);

    while (sst25vfxxx_get_status() & SST25VFXXX_SR_BUSY);

    sst25vfxxx_select();
    memory_spi_rw(SST25VFXXX_READ);
    memory_spi_rw((unsigned char)(addr_page >> 16));
    memory_spi_rw((unsigned char)(addr_page >> 8));
    memory_spi_rw((unsigned char)(addr_page));

    while (p < SST25VFXXX_PAGE_SIZE)
        g_sst25vfxxx_page_buf[p++] = memory_spi_rw(0xFF);

    sst25vfxxx_unselect();    
    
    
    
    while (len--)
        g_sst25vfxxx_page_buf[i++] = *buf++;
    
    
    while ((status = sst25vfxxx_get_status()) & SST25VFXXX_SR_BUSY);
    if ((status & SST25VFXXX_SR_WREN) == 0)
    {
        sst25vfxxx_select();
        memory_spi_rw(SST25VFXXX_WREN);
        sst25vfxxx_unselect();
        memory_spi_rw(0xFF);
    }
    if ( status & SST25VFXXX_SR_BP )
    {
        sst25vfxxx_select();
        memory_spi_rw(SST25VFXXX_WRSR);
        memory_spi_rw(status & ~SST25VFXXX_SR_BP);
        sst25vfxxx_select();
        memory_spi_rw(0xFF); 
    }
    
    sst25vfxxx_select();
    memory_spi_rw(SST25VFXXX_ERASE_4K);
    memory_spi_rw((unsigned char)(addr_page >> 16));
    memory_spi_rw((unsigned char)(addr_page >> 8));
    memory_spi_rw((unsigned char)(addr_page)); 
    sst25vfxxx_unselect();
    memory_spi_rw(0xFF);

    while ((status = sst25vfxxx_get_status()) & SST25VFXXX_SR_BUSY);
    if ((status & SST25VFXXX_SR_WREN) == 0)
    {
        sst25vfxxx_select();
        memory_spi_rw(SST25VFXXX_WREN);
        sst25vfxxx_unselect();
        memory_spi_rw(0xFF);
    }   
    
    i = 0;

    sst25vfxxx_select();
    memory_spi_rw(SST25VFXXX_AAIP);
    memory_spi_rw((unsigned char)(addr_page >> 16));
    memory_spi_rw((unsigned char)(addr_page >> 8));
    memory_spi_rw((unsigned char)(addr_page));
    memory_spi_rw(g_sst25vfxxx_page_buf[i++]);
    memory_spi_rw(g_sst25vfxxx_page_buf[i++]); 
    sst25vfxxx_unselect();
    memory_spi_rw(0xFF);  
    while (i < SST25VFXXX_PAGE_SIZE)
    {
        while ((status = sst25vfxxx_get_status()) & SST25VFXXX_SR_BUSY);
        
        sst25vfxxx_select();
        memory_spi_rw(SST25VFXXX_AAIP);
        memory_spi_rw(g_sst25vfxxx_page_buf[i++]);
        memory_spi_rw(g_sst25vfxxx_page_buf[i++]);
        sst25vfxxx_unselect();
        memory_spi_rw(0xFF); 
    }
    
    while ((status = sst25vfxxx_get_status()) & SST25VFXXX_SR_BUSY);   
   
    sst25vfxxx_select();
    memory_spi_rw(SST25VFXXX_WRDI);
    sst25vfxxx_unselect();
    memory_spi_rw(0xFF); 
    
    spi_unlock();
}

static unsigned char sst25vfxxx_get_status()
{
    unsigned char status;
    sst25vfxxx_select();
    memory_spi_rw(SST25VFXXX_RDSR);
    status = memory_spi_rw(0xFF);
    sst25vfxxx_unselect();
    memory_spi_rw(0xFF); // pause after chip select disable
    return status;
}