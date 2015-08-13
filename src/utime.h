/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __UTIME_H__
#define __UTIME_H__

#include "tnkernel/tn.h"

#define MIN_CORRECT_TIME    1293840000  // 00:00 01/01/2011

typedef unsigned long       time_t;

struct tm
{
    unsigned tm_sec;     /* seconds after the minute - [0,59] */
    unsigned tm_min;     /* minutes after the hour - [0,59] */
    unsigned tm_hour;    /* hours since midnight - [0,23] */
    unsigned tm_mday;    /* day of the month - [1,31] */
    unsigned tm_mon;     /* months since January - [0,11] */
    unsigned tm_year;    /* years since 1900 */
    //unsigned tm_wday;    /* days since Sunday - [0,6] */
    //unsigned tm_yday;    /* days since January 1 - [0,365] */
    //unsigned tm_isdst;   /* daylight savings time flag */
};

// unix time functions

time_t      time(time_t *t);
int         stime(time_t *t);
time_t      mktime(struct tm* tp);
struct tm*  gmtime(time_t t, struct tm* tp);

BOOL        time_is_valid(time_t const t);
BOOL        tm_is_valid(struct tm* tp);

// time interrupt handler

void        hw_unix_time_int_handler(); // call this every one second

#endif // __UTIME_H__
