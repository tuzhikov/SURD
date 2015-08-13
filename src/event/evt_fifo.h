/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __EVT_FIFO_H__
#define __EVT_FIFO_H__

#include "../tnkernel/tn.h"
#include "../utime.h"

#pragma pack(push, 1)

#define EV_SIZE 26

struct evt_fifo_item_t  // sizeof(evt_fifo_item_t) == 16 bytes
{
    time_t          ts;         // timestamp
    unsigned char   dat_len;    // event data length, dat_len == 0 - item is empty
    unsigned char   dat[EV_SIZE];    // event data
    unsigned char   crc;        // CRC8, internal field, do not use it in common program
};

#pragma pack(pop)

void        evt_fifo_init();                                // finding and reading top event to RAM cache
void        evt_fifo_clear();                               // clear all items from queue (write into EEPROM and RAM cache clear)
                                                            // может вызываться без вызова evt_fifo_init();

BOOL        Event_Push_Str(char *st);
BOOL        evt_fifo_push(struct evt_fifo_item_t* item);    // add new event into queue (write into EEPROM and RAM cache update if need)
BOOL        evt_fifo_get(struct evt_fifo_item_t* item);     // get top event from queue (read from RAM cache, fast)
BOOL        evt_fifo_pop();                                 // erase top event from queue (write into EEPROM and RAM cache update)
unsigned    evt_fifo_size();                                // queue max size (max items count)
unsigned    evt_fifo_used();                                // queue size (used items count)
unsigned    evt_fifo_free();                                // queue free size (unused items count)
BOOL        evt_fifo_empty();                               // queue size == 0 (used items count == 0)
BOOL        evt_fifo_full();                                // queue free size == 0 (unused items count == 0)
//void        cache_update();
BOOL evt_find_pointers();
void Event_Check_Pointers();

void        evt_fifo_prepare_evt(struct evt_fifo_item_t* item);
BOOL evt_fifo_get_direct(unsigned long pos, struct evt_fifo_item_t* item);

#endif // __EVT_FIFO_H__
