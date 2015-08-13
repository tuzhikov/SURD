/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "string.h"
#include "../tnkernel/tn.h"
#include "../stellaris.h"
#include "lm3sxxx.h"

#include "../debug/debug.h"

#define LM3SXXX_PAGE_SIZE   1024

static unsigned long    g_lm3sxxx_start_addr;
static unsigned         g_lm3sxxx_size;

static void lm3sxxx_wr_pg(unsigned long addr, unsigned char* buf, unsigned len);

void lm3sxxx_init(unsigned long start_addr, unsigned size)
{
    g_lm3sxxx_start_addr    = start_addr;
    g_lm3sxxx_size          = size;
    MAP_FlashUsecSet(MAP_SysCtlClockGet() / 1000000);
}

void lm3sxxx_rd(unsigned long addr, unsigned char* buf, unsigned len)
{
    if (addr + len > g_lm3sxxx_size)
    {
        dbg_puts("addr + len > g_lm3sxxx_size");
        dbg_trace();
        tn_halt();
    }

    memcpy(buf, (void*)(g_lm3sxxx_start_addr + addr), len);
}

void lm3sxxx_wr(unsigned long addr, unsigned char* buf, unsigned len)
{
    if (addr + len > g_lm3sxxx_size)
    {
        dbg_puts("addr + len > g_lm3sxxx_size");
        dbg_trace();
        tn_halt();
    }

    unsigned long aaddr = g_lm3sxxx_start_addr + addr;  // absolute adderss

    unsigned long const fp_len = LM3SXXX_PAGE_SIZE - (aaddr & LM3SXXX_PAGE_SIZE - 1);
    if (fp_len != LM3SXXX_PAGE_SIZE)
    {
        if (len >= fp_len)
        {
            lm3sxxx_wr_pg(aaddr, buf, fp_len);
            aaddr   += fp_len;
            buf     += fp_len;
            len     -= fp_len;
        }
        else
        {
            lm3sxxx_wr_pg(aaddr, buf, len);
            aaddr   += len;
            buf     += len;
            len     -= len;
        }
    }

    while (len >= LM3SXXX_PAGE_SIZE)
    {
        lm3sxxx_wr_pg(aaddr, buf, LM3SXXX_PAGE_SIZE);
        aaddr   += LM3SXXX_PAGE_SIZE;
        buf     += LM3SXXX_PAGE_SIZE;
        len     -= LM3SXXX_PAGE_SIZE;
    }

    if (len != 0)
        lm3sxxx_wr_pg(aaddr, buf, len);
}

// local functions

static void lm3sxxx_wr_pg(unsigned long aaddr, unsigned char* buf, unsigned len)
{
    static unsigned char    page_buf[LM3SXXX_PAGE_SIZE];
    unsigned long const     page_start = aaddr & ~(LM3SXXX_PAGE_SIZE - 1);
    unsigned long const     buf_offset = aaddr & LM3SXXX_PAGE_SIZE - 1;

    memcpy(page_buf, (void*)page_start, LM3SXXX_PAGE_SIZE);
    memcpy(&page_buf[buf_offset], buf, len);

    tn_cpu_int_disable();

    if (MAP_FlashErase(page_start) != 0)
    {
        dbg_printf("FlashErase(0x%08X) error\n", page_start);
        dbg_trace();
        tn_halt();
    }

    if (MAP_FlashProgram((unsigned long*)page_buf, page_start, LM3SXXX_PAGE_SIZE) != 0)
    {
        dbg_printf("FlashProgram(0x%08X) error\n", page_start);
        dbg_trace();
        tn_halt();
    }
    tn_cpu_int_enable();

}
