/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __MEMORY_H__
#define __MEMORY_H__

#define FLASH_MAX_PKT_LEN  512


#define FLASH_PREF_SIZE    4096
#define FLASH_PROGRAM_SIZE 102400
#define FLASH_EVENT_SIZE   512000
//#define FLASH_EVENT_SIZE   192
#define FLASH_PROGS_SIZE   122880*4


unsigned char memory_init();
void flash_erase(void);

void iflash_init(); // used only in boot loader

/*
// old version
enum flash_region_t
{
    FLASH_PREF,    // preferences
    FLASH_PROGRAM, // project memory
    FLASH_EVENT,   // event log memory
    FLASH_PROGS,   // Programs memory
    FLASH_FREE,

    FLASH_CNT      // count of FLASH regions
};
*/
enum flash_region_t
{
    FLASH_PREF,    // preferences
    FLASH_PROGRAM, // project memory
    FLASH_PROGS,   // Programs memory
    FLASH_EVENT,   // event log memory
    FLASH_FREE,

    FLASH_CNT      // count of FLASH regions
};


unsigned    flash_sz(enum flash_region_t rg);
void        flash_rd(enum flash_region_t rg, unsigned long addr, unsigned char* buf, unsigned len);
void        flash_wr(enum flash_region_t rg, unsigned long addr, unsigned char* buf, unsigned len);
void        flash_rd_direct(unsigned long addr, unsigned char* buf, unsigned len);
void        flash_wr_direct(unsigned long addr, unsigned char* buf, unsigned len);


enum iflash_region_t    // internal CPU flash memory
{
    IFLASH_FIRMWARE,    // used only in boot loader!
    IFLASH_FIRMWARE_BUFFER,
    IFLASH_CNT          // count of internal FLASH regions
};

unsigned    iflash_sz(enum iflash_region_t rg);
void        iflash_rd(enum iflash_region_t rg, unsigned long addr, unsigned char* buf, unsigned len);
void        iflash_wr(enum iflash_region_t rg, unsigned long addr, unsigned char* buf, unsigned len);
unsigned long get_region_start(enum flash_region_t rg);

#endif // __MEMORY_H__
