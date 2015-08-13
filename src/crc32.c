/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
******************************************************************************/

#include "crc32.h"

static unsigned long g_crc_tab[256];

void crc32_init()
{
    unsigned long const poly = 0xEDB88320;
    unsigned long i, j, crc;

    for (i = 0; i < 256; ++i)
    {
        crc = i;
        for (j = 8; j > 0; --j)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ poly;
            else
                crc >>= 1;
        }

        g_crc_tab[i] = crc;
    }
}

unsigned long crc32_calc(unsigned char* buf, unsigned len)
{
    unsigned long crc = 0xFFFFFFFF;

    while (len--)
        crc = crc >> 8 ^ g_crc_tab[(crc ^ *buf++) & 0xFF];

    return crc ^ 0xFFFFFFFF;
}