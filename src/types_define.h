/*****************************************************************************
*
* © 2015 Alex Tuzhikov
*
*****************************************************************************/

#ifndef __TYPES_DEFINE_H__
#define __TYPES_DEFINE_H__

#ifndef bool
#define bool  unsigned char
#endif
#ifndef true
#define true  1
#endif
#ifndef false
#define false 0
#endif
#ifndef BOOL
#define BOOL int
#endif

typedef unsigned char  U08;
typedef unsigned char  U8;
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned short U16;
typedef signed short   S16;
typedef unsigned long  DWORD;
typedef unsigned long  U32;


/* type Buffer UART*/
typedef struct __UART_SBUF
{
    U08 *buff;           /* Указатель на буфер           */
    U08 *pWR;            /* Указатель чтения             */
    U08 *pRD;            /* Указатель записи             */
    U16 cnt;             /* Количество символов в буфере */
    U16 size;            /* Размер буфера                */

    struct
    {
        unsigned FULL  : 1;
        unsigned EMPTY : 1;
        unsigned OERR  : 1;
        unsigned ERR   : 1;
        unsigned       : 4;
    } stat;

} UART_SBUF;

/* int to two char*/
typedef union _INT_TO_TWO_CHAR_{
	unsigned char  Byte[2];
	unsigned short Int;
}INT_TO_TWO_CHAR;
/*return value */
typedef enum _Type_Return_{retNull=0x0000,retOk=0x0001,retError=0x0002,retDelay=0x0004}Type_Return;
/*step machine */
//typedef enum _Type_Step_Machine_{Null,One,Two,Three,For,Five,Sex,Seven,Eight,Nine,Ten}Type_Step_Machine;
#define Null  0
#define One   1
#define Two   2
#define Three 3
#define For   4
#define Five  5
#define Sex   6
#define Seven 7
#endif // __TYPES_DEFINE_H__
