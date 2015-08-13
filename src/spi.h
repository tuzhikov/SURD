/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __SPI_H__
#define __SPI_H__

// DIGI-RF
void            spi0_init();
void            spi0_set_bitrate(unsigned long bitrate);
unsigned char   spi0_rw(unsigned char b);
void            spi0_lock();
void            spi0_lock_br(unsigned long bitrate);
void            spi0_unlock();

#define rf_spi_rw               spi0_rw
#define rf_spi_set_bitrate      spi0_set_bitrate
#define rf_spi_lock             spi0_lock
#define rf_spi_lock_br          spi0_lock_br
#define rf_spi_unlock           spi0_unlock



// FLASH, CLOCK
void            spi1_init();
void            spi1_set_bitrate(unsigned long bitrate);
unsigned char   spi1_rw(unsigned char b);
void            spi1_lock();
void            spi1_lock_br(unsigned long bitrate);
void            spi1_unlock();

#define memory_spi_rw           spi1_rw
#define memory_spi_set_bitrate  spi1_set_bitrate
#define memory_spi_lock         spi1_lock
#define memory_spi_lock_br      spi1_lock_br
#define memory_spi_unlock       spi1_unlock

#endif // __SPI_H__
