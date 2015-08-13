/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include <stdio.h>
#include "mem_map.h"
#include "version.h"

int get_version(char* buf, unsigned len)
{

#ifdef DEBUG
    char const mod[] = "d";
#else
    char const mod[] = "";
#endif // DEBUG

    if (FW_VER->fix_rev != 0)
        return snprintf(buf, len, APP_NAME " %u.%u.%u.%u%s [" VER_SVN_DATE"]", (unsigned)FW_VER->major, (unsigned)FW_VER->minor, (unsigned)FW_VER->svn_rev, (unsigned)FW_VER->fix_rev, mod);
    else
        return snprintf(buf, len, APP_NAME " %u.%u.%u%s [" VER_SVN_DATE"]", (unsigned)FW_VER->major, (unsigned)FW_VER->minor, (unsigned)FW_VER->svn_rev, mod);
}
