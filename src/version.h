/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
* SVN Repository URL: https://192.168.19.4/megaphone2/trunk
*
*****************************************************************************/

#ifndef __VERSION_H__
#define __VERSION_H__

// Utility

//Контролировать версию при перепрошивке
#define FIRMWARE_CONTROL

#define STRINGIZE1(text)        #text
#define STRINGIZE(text)         STRINGIZE1(text)

#define MAKE_VER_CSV(m,n,b,r)   m,n,b,r
#define MAKE_VER_STR(m,n,b,r)   STRINGIZE(m.n.b.r)
#define MAKE_VER_STR_SHORT(m,n) STRINGIZE(m.n)

// Boot loader part

#define BOOT_VER_MAJOR          1
#define BOOT_VER_MINOR          0

// Common part

#define VER_MAJOR               2
#define VER_MINOR               0
#define VER_MINORER             4
#define VER_SVN_REV             0     // SVN Revision
#define VER_FIX_REV             0

#define VER_SVN_DATE            "2015/07/27 11:25:25"  // SVN Revision Date

#define TYPE_DEVICE             0

#define APP_NAME                "Traffic controller-KURS-R(SURD)"
//#define APP_VERSION             MAKE_VER_STR(VER_MAJOR, VER_MINOR, VER_SVN_REV, VER_FIX_REV)
//#define APP_VERSION_SHORT       MAKE_VER_STR_SHORT(VER_MAJOR, VER_MINOR)
//#define APP_VERSION_RC          MAKE_VER_CSV(VER_MAJOR, VER_MINOR, VER_SVN_REV, VER_FIX_REV)
//#define APP_COMPANY             "Cyber-CB"
#define APP_COPYRIGHT           "(C) 2014 Cyber-SB"
//#define APP_DESC                "Светофорный контроллер"
//#define APP_FILENAME            "led_control.bin"

int get_version(char* buf, unsigned len);

#endif // __VERSION_H__
