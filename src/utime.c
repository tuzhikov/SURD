/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "utime.h"

#define YEAR_TO_DAYS(y)     ((y)*365 + (y)/4 - (y)/100 + (y)/400)
#define IS_LEAP_YEAR(y)     (((y) % 4 == 0 && (y) % 100 != 0) || (y) % 400 == 0)

// local variables

static time_t volatile  g_time;

// unix time functions

time_t time(time_t *t)
{
    TN_INTSAVE_DATA;
    tn_disable_interrupt();
    time_t const time = g_time;
    tn_enable_interrupt();
    if (t != NULL)
        *t = time;
    return time;
}

int stime(time_t *t)
{
    if (t == NULL)
        return -1;

    TN_INTSAVE_DATA;
    tn_disable_interrupt();
    g_time = *t;
    tn_enable_interrupt();
    return 0;
}

time_t mktime(struct tm* tp)
{
    unsigned year = tp->tm_year, mon = tp->tm_mon;
    if ((int)(mon -= 2) <= 0) // 1..12 -> 11,12,1..10
    {
        mon += 12;  // Puts Feb last since it has leap day
        year -= 1;
    }

    return ((((time_t)(year/4 - year/100 + year/400 + 367*mon/12 + tp->tm_mday) +
        year*365 - 719499
        )*24 + tp->tm_hour  // now have hours
        )*60 + tp->tm_min   // now have minutes
        )*60 + tp->tm_sec;  // finally seconds
}

struct tm* gmtime(time_t t, struct tm* tp)
{
    /* First take out the hour/minutes/seconds - this part is easy. */
    tp->tm_sec = t % 60;
    t /= 60;

    tp->tm_min = t % 60;
    t /= 60;

    tp->tm_hour = t % 24;
    t /= 24;

    /* t is now days since 01/01/1970 UTC
     * Rebaseline to the Common Era */

    t += 719499;

    /* Roll forward looking for the year.  This could be done more efficiently
     * but this will do.  We have to start at 1969 because the year we calculate here
     * runs from March - so January and February 1970 will come out as 1969 here.*/
    for (tp->tm_year = 1969; t > YEAR_TO_DAYS(tp->tm_year + 1) + 30; ++tp->tm_year);

    /* OK we have our "year", so subtract off the days accounted for by full years. */
    t -= YEAR_TO_DAYS(tp->tm_year);

    /* t is now number of days we are into the year (remembering that March 1
     * is the first day of the "year" still). */

    /* Roll forward looking for the month.  1 = March through to 12 = February. */
    for (tp->tm_mon = 1; tp->tm_mon < 12 && t > 367*(tp->tm_mon+1)/12; ++tp->tm_mon);

    /* Subtract off the days accounted for by full months */
    t -= 367*tp->tm_mon/12;

    /* t is now number of days we are into the month */

    /* Adjust the month/year so that 1 = January, and years start where we
     * usually expect them to. */
    tp->tm_mon += 2;
    if (tp->tm_mon > 12)
    {
        tp->tm_mon -= 12;
        tp->tm_year++;
    }

    tp->tm_mday = t;
    return tp;
}

BOOL time_is_valid(time_t const t)
{
    return t >= MIN_CORRECT_TIME; // >= !!!
}

BOOL tm_is_valid(struct tm* tp)
{
    static unsigned char const mdays[] = { 31, 0 /*feb not used*/, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (tp->tm_year < 1970)
        return FALSE;

    if (tp->tm_mon < 1 || tp->tm_mon > 12)
        return FALSE;

    if (tp->tm_mon == 2) // feb
    {
        if (tp->tm_mday > (IS_LEAP_YEAR(tp->tm_year) ? 29 : 28))
            return FALSE;
    }
    else
    {
        if (tp->tm_mday > mdays[tp->tm_mon - 1])
            return FALSE;
    }

    if (tp->tm_sec > 59 || tp->tm_min > 59 || tp->tm_hour > 23)
        return FALSE;

    return TRUE;
}

// unix time interrupt handler

void hw_unix_time_int_handler()
{
    TN_INTSAVE_DATA_INT;
    tn_idisable_interrupt();
    ++g_time;
    tn_ienable_interrupt();
}
