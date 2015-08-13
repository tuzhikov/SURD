/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include <string.h>
#include "../../stellaris.h"
#include "rit128x96x4.h"
#include "rit_p14201.h"

#define RIT_ROW 12
#define RIT_COL 22 // 21 + zero char
#define RIT_BRIGHTNESS  0x0F
#define RIT_MAX_LINE_LN (RIT_COL - 1)

static char g_rit_buf[RIT_ROW][RIT_COL];
static char g_rit_empty_line[RIT_COL];

static void rit_add_lines(char const* s);
static void rit_add_line(char const* s, int len);

void rit_p14201_init()
{
    memset(g_rit_buf, 0, sizeof(g_rit_buf));
    memset(g_rit_empty_line, 0x20, RIT_COL - 1); g_rit_empty_line[RIT_COL - 1] = 0;
    RIT128x96x4Init(MAP_SysCtlClockGet() / 20);
}

void rit_p14201_out(char const* s)
{
    rit_add_lines(s);

    for (int i = 0; i < RIT_ROW; ++i)
    {
        RIT128x96x4StringDraw(g_rit_empty_line, 2, i * 8, 0);
        RIT128x96x4StringDraw(g_rit_buf[i], 2, i * 8, RIT_BRIGHTNESS);
    }
}

static void rit_add_lines(char const* s)
{
    if (s == 0 || s[0] == 0)
    {
        rit_add_line(" ", 1);
        return;
    }

    int start = 0;

    for (;;)
    {
        int spn = strcspn(&s[start], "\n");

        if (spn == 0)
            break;

        if (spn > RIT_MAX_LINE_LN)
        {
            spn = RIT_MAX_LINE_LN;
            rit_add_line(&s[start], spn);
        }
        else
        {
            rit_add_line(&s[start], spn);
            if (s[start + spn] == '\n')
                ++spn;
        }

        start += spn;
    }
}

static void rit_add_line(char const* s, int len)
{
    for (int i = 0; i < RIT_ROW - 1; ++i)
        strcpy(g_rit_buf[i], g_rit_buf[i + 1]);

    if (len > RIT_MAX_LINE_LN)
        len = RIT_MAX_LINE_LN;

    for (int i = 0; i < len; ++i)
        g_rit_buf[RIT_ROW - 1][i] = s[i];
    g_rit_buf[RIT_ROW - 1][len] = 0;
}
