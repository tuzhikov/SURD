/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "../tnkernel/tn.h"
#include "../spi.h"
#include "../pins.h"
#include "m25pexx.h"

#include "../debug/debug.h"

#define M25PEXX_PAGE_SIZE   256
#define M25PEXX_ADDR_SIZE   3
#define M25PEXX_MAX_SIZE    (1 << M25PEXX_ADDR_SIZE * 8)

#define M25PEXX_WREN        0x06    // Write-Enable
#define M25PEXX_WRDI        0x04    // Write-Disable
#define M25PEXX_RDSR        0x05    // Read-Status-Register
#define M25PEXX_WRSR        0x01    // Write-Status-Register
#define M25PEXX_READ        0x03    // Read Memory
#define M25PEXX_FAST_RD     0x0B    // fast read
#define M25PEXX_PGPR        0x02    // page program, without page auto erase
#define M25PEXX_PGWR        0xAD    // page write, with page auto erase
#define M25PEXX_ERASE_BULK  0xC7
#define M25PEXX_RDID        0x9F    // JEDEC Read-ID

#define M25PEXX_SR_BUSY     0x01    // WIP, write in progress
#define M25PEXX_SR_WREN     0x02    // WEL, write enable latch
#define M25PEXX_SR_BP       0x3C    // BP0 - BP3

#define spi_lock()          memory_spi_lock_br(g_m25pexx_spi_bitrate)
#define spi_unlock()        memory_spi_unlock()
#define m25pexx_select()    pin_off(OPIN_M25PEXX_CS_FLASH)
#define m25pexx_unselect()  pin_on(OPIN_M25PEXX_CS_FLASH)

static unsigned long        g_m25pexx_size;
static unsigned long        g_m25pexx_spi_bitrate;

// local functions

static void                 m25pexx_wr_pg(unsigned long addr, unsigned char* buf, unsigned len);
static unsigned char        m25pexx_get_status();

BOOL m25pexx_init(unsigned size, unsigned long spi_bitrate)
{
    unsigned char count = 10;
    
    if (size > M25PEXX_MAX_SIZE)
    {
        dbg_puts("size > 0x00FFFFFF");
        dbg_trace();
        tn_halt();
    }

    g_m25pexx_size          = size;
    g_m25pexx_spi_bitrate   = spi_bitrate;

    spi_lock();
    while (m25pexx_get_status() & M25PEXX_SR_BUSY && --count);
    spi_unlock();
    
    return (BOOL)count;
}

void m25pexx_rdid (unsigned char *id)
{

    spi_lock();
    while (m25pexx_get_status() & M25PEXX_SR_BUSY);

    m25pexx_select();
    memory_spi_rw(M25PEXX_RDID);
    *id++ = memory_spi_rw(0xFF);
    *id++ = memory_spi_rw(0xFF);
    *id++ = memory_spi_rw(0xFF);
    m25pexx_unselect();
    memory_spi_rw(0xFF); // pause after chip select disable    
    spi_unlock();    
}

void m25pexx_rd(unsigned long addr, unsigned char* buf, unsigned len)
{
    if (addr + len > g_m25pexx_size)
    {
        dbg_puts("addr + len > g_at25xxx_size");
        dbg_trace();
        tn_halt();
    }

    spi_lock();
    while (m25pexx_get_status() & M25PEXX_SR_BUSY);

    m25pexx_select();
    memory_spi_rw(M25PEXX_READ);
    memory_spi_rw((unsigned char)(addr >> 16));
    memory_spi_rw((unsigned char)(addr >> 8));
    memory_spi_rw((unsigned char)(addr));

    while (len--)
        *buf++ = memory_spi_rw(0xFF);

    m25pexx_unselect();
    spi_unlock();
}

void m25pexx_wr(unsigned long addr, unsigned char* buf, unsigned len)
{
    if (addr + len > g_m25pexx_size)
    {
        dbg_puts("addr + len > g_at25xxx_size");
        dbg_trace();
        tn_halt();
    }

    unsigned long const fp_len = M25PEXX_PAGE_SIZE - (addr & M25PEXX_PAGE_SIZE - 1);
    if (fp_len != M25PEXX_PAGE_SIZE)
    {
        if (len >= fp_len)
        {
            m25pexx_wr_pg(addr, buf, fp_len);
            addr    += fp_len;
            buf     += fp_len;
            len     -= fp_len;
        }
        else
        {
            m25pexx_wr_pg(addr, buf, len);
            addr    += len;
            buf     += len;
            len     -= len;
        }
    }

    while (len >= M25PEXX_PAGE_SIZE)
    {
        m25pexx_wr_pg(addr, buf, M25PEXX_PAGE_SIZE);
        addr    += M25PEXX_PAGE_SIZE;
        buf     += M25PEXX_PAGE_SIZE;
        len     -= M25PEXX_PAGE_SIZE;
    }

    if (len != 0)
        m25pexx_wr_pg(addr, buf, len);
}

void m25pexx_erase(void)
{
    unsigned char status;

    spi_lock();
    while ((status = m25pexx_get_status()) & M25PEXX_SR_BUSY);

    if ((status & M25PEXX_SR_WREN) == 0)
    {
        m25pexx_select();
        memory_spi_rw(M25PEXX_WREN);
        m25pexx_unselect();
        memory_spi_rw(0xFF); // m25pexx_unselect(); pause; m25pexx_select();
    }
    
    m25pexx_select();
    memory_spi_rw(M25PEXX_ERASE_BULK);
    
    m25pexx_unselect();
    spi_unlock();
}

// local functions

static void m25pexx_wr_pg(unsigned long addr, unsigned char* buf, unsigned len)
{
    spi_lock();
    unsigned char status;
    while ((status = m25pexx_get_status()) & M25PEXX_SR_BUSY);

    if ((status & M25PEXX_SR_WREN) == 0)
    {
        m25pexx_select();
        memory_spi_rw(M25PEXX_WREN);
        m25pexx_unselect();
        memory_spi_rw(0xFF); // m25pexx_unselect(); pause; m25pexx_select();
    }
    
    if ( status & M25PEXX_SR_BP )
    {
        m25pexx_select();
        memory_spi_rw(M25PEXX_WRSR);
        memory_spi_rw(status & ~M25PEXX_SR_BP);
        m25pexx_unselect();
        memory_spi_rw(0xFF); // m25pexx_unselect(); pause; m25pexx_select();      
    }


    m25pexx_select();
    memory_spi_rw(M25PEXX_PGWR);
    memory_spi_rw((unsigned char)(addr >> 16));
    memory_spi_rw((unsigned char)(addr >> 8));
    memory_spi_rw((unsigned char)(addr));

    while (len--)
        memory_spi_rw(*buf++);

    
    
    m25pexx_unselect();
memory_spi_rw(0xFF);
        m25pexx_select();
        memory_spi_rw(M25PEXX_WRDI);
        m25pexx_unselect();
        memory_spi_rw(0xFF); // m25pexx_unselect(); pause; m25pexx_select();
    spi_unlock();
}

static unsigned char m25pexx_get_status()
{
    unsigned char status;
    m25pexx_select();
    memory_spi_rw(M25PEXX_RDSR);
    status = memory_spi_rw(0xFF);
    m25pexx_unselect();
    memory_spi_rw(0xFF); // pause after chip select disable
    return status;
}