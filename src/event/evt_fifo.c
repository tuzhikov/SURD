/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include <string.h>
#include "../memory/memory.h"
#include "evt_fifo.h"
#include "../memory/ds1390.h"

#include "../debug/debug.h"
#include "../crc32.h"

#define EVT_FIFO_TRACE      FALSE
#define INVALID_PTR         ((void*)-1)

// local variables

static TN_MUTEX                     g_mutex;
static struct evt_fifo_item_t*      g_read_p;
static struct evt_fifo_item_t*      g_read_pu; //для сохранения

static struct evt_fifo_item_t*      g_write_p;
static unsigned                     g_fifo_size;    // queue max size in elements
static struct evt_fifo_item_t       g_cache;
static struct evt_fifo_item_t       g_null_item;
static BOOL                         g_cache_is_valid;

// local functions

static BOOL crc_is_valid(struct evt_fifo_item_t* item_p);
static void crc_calc(struct evt_fifo_item_t* item_p);
static BOOL cache_get(struct evt_fifo_item_t* item_p);
static void cache_set(struct evt_fifo_item_t* item_p);
static void cache_update();

//------------------------------------------------------------------------------
// прибавляет к указателю 1 значение
// true - переходим через начало
BOOL Next_Pointer(struct evt_fifo_item_t** poi )
{
    (*poi)++;
    if ((unsigned)(*poi) >= flash_sz(FLASH_EVENT))
    {
        (*poi)=NULL;
        return (TRUE);
    }
    ///
    return (FALSE);
}
//------------------------------------------------------------------------------

BOOL Event_Push_Str(char *st)
{
    DS1390_TIME time;
    BOOL true_time = GetTime_DS1390(&time);
    ///
    struct evt_fifo_item_t item;
    struct tm  tim;
    //
    memset(&item,0,sizeof(item));
    //
    tim.tm_hour = time.hour;
    tim.tm_mday = time.date;
    tim.tm_min = time.min;
    tim.tm_sec = time.sec;
    tim.tm_mon = time.month;
    tim.tm_year = time.year;
    //
    item.ts = mktime(&tim);

    ///////////////
    int il = strlen(st);
    if (il>EV_SIZE)
      st[EV_SIZE]=0;
    item.dat_len=strlen(st);
    //
    strcpy((char*)item.dat,st);
    //
    return( evt_fifo_push(&item));

         // get_state

}
//------------------------------------------------------------------------------
// пишем item по g_write_p и всё
BOOL item_write(struct evt_fifo_item_t* item)
{
    BOOL ret = FALSE;
     struct evt_fifo_item_t tmp_item;
    /////////
    tn_mutex_lock(&g_mutex, TN_WAIT_INFINITE);
    //
    crc_calc(item);
    for (int i_try=0; i_try<7; i_try++)
    {
       flash_wr(FLASH_EVENT, (unsigned long)g_write_p, (unsigned char*)item, sizeof(*item));
       /// check
       flash_rd(FLASH_EVENT, (unsigned long)g_write_p, (unsigned char*)&tmp_item, sizeof(tmp_item));
       //
       if (memcmp((unsigned char*)item, (unsigned char*)&tmp_item, sizeof(tmp_item))==0)
       {
         ret=TRUE;
         break;
       }
       else
       {
         ret=FALSE;
       }

    }
    ///
    tn_mutex_unlock(&g_mutex);
    //////
    return (ret);
}
//------------------------------------------------------------------------------
void evt_fifo_clear()
{
    tn_mutex_lock(&g_mutex, TN_WAIT_INFINITE);

    cache_set(NULL); // clear top item cache
    //struct evt_fifo_item_t* const last_p = (struct evt_fifo_item_t*)(flash_sz(FLASH_EVENT) - sizeof(struct evt_fifo_item_t));
   //static  struct evt_fifo_item_t* item_p;// = NULL;
   //g_write_p
   g_write_p = NULL;
   item_write(&g_null_item);
   //
   //item_p = NULL;
   //flash_wr(FLASH_EVENT, (unsigned long)item_p++, (unsigned char*)&g_null_item, sizeof(g_null_item));
   //g_write_p = item_p;
   Event_Push_Str("Журнал очищен!!");
   //(unsigned long)item_p++;
   //flash_wr(FLASH_EVENT, (unsigned long)item_p++, (unsigned char*)&g_null_item, sizeof(g_null_item));
   //
   //evt_find_pointers();
   //
   tn_mutex_unlock(&g_mutex);
}
//------------------------------------------------------------------------------
BOOL evt_find_pointers()
{
    g_read_p    = INVALID_PTR;
    g_read_pu    = INVALID_PTR;
    g_write_p   = INVALID_PTR;
    ///
    struct evt_fifo_item_t  tmp_item[2];
    struct evt_fifo_item_t* item_p = NULL;
    struct evt_fifo_item_t* last_p = (struct evt_fifo_item_t*)(flash_sz(FLASH_EVENT) - sizeof(tmp_item));
    while (item_p <= last_p)
    {
        if ((g_read_pu!=INVALID_PTR) && (g_write_p!=INVALID_PTR))
          break;
        //if (g_write_p!=INVALID_PTR)
        //  break;
        //////
        hw_watchdog_clear();
        //
        flash_rd(FLASH_EVENT, (unsigned long)item_p, (unsigned char*)tmp_item, sizeof(tmp_item));
        //hw_watchdog_clear();

        if (tmp_item[0].dat_len == 0 && tmp_item[1].dat_len != 0) // переход: свободное место -> данные
        {
            if (crc_is_valid(&tmp_item[1]))
            {
                g_read_pu = item_p;
                //cache_set(&tmp_item[1]);
            }
        }
        else
        if (tmp_item[0].dat_len != 0 && tmp_item[1].dat_len == 0) // переход: данные -> свободное место
        {
            if (crc_is_valid(&tmp_item[0]))
            {
                g_write_p = item_p;
            }
        }
        ///
        item_p++;

    }
    /////////////////
    if (g_write_p != INVALID_PTR)
    {
       g_read_p = g_write_p;
       return (TRUE);
    }
    else
    {
       return (FALSE);
    }

    //
if(g_read_p == INVALID_PTR || g_write_p == INVALID_PTR)
    return (FALSE);
  else
    return (TRUE);
//
    /*
    if (g_read_p == INVALID_PTR || g_write_p == INVALID_PTR)
    {
        item_p = NULL;
        last_p = (struct evt_fifo_item_t*)(flash_sz(FLASH_EVENT) - sizeof(tmp_item[0]));
        while (item_p <= last_p)
        {
            flash_rd(FLASH_EVENT, (unsigned long)item_p, (unsigned char*)tmp_item, sizeof(tmp_item[0]));
            //hw_watchdog_clear();

            BOOL const crc_valid = crc_is_valid(tmp_item);
            if (g_read_p == INVALID_PTR && tmp_item->dat_len != 0 && crc_valid)
            {
                g_read_p = item_p;
                cache_set(tmp_item);
            }

            if (g_write_p == INVALID_PTR && (tmp_item->dat_len == 0 || !crc_valid))
                g_write_p = item_p;

            ++item_p;
        }
    }

    if (g_read_p == INVALID_PTR)
        g_read_p = g_write_p;

    if (g_write_p == INVALID_PTR) // на случай ядерной войны :)
        g_write_p = g_read_p;
    */
// return (FALSE);
}
//------------------------------------------------------------------------------
void Event_Check_Pointers()
{
        if (!evt_find_pointers())
        {
          evt_fifo_clear();
          evt_find_pointers();
        }
}
//------------------------------------------------------------------------------
void evt_fifo_init()
{
    dbg_printf("Initializing events queue...");

    g_fifo_size = flash_sz(FLASH_EVENT) / sizeof(struct evt_fifo_item_t) ;//- 1;
    g_read_p    = INVALID_PTR;
    g_write_p   = INVALID_PTR;
    cache_set(NULL);
    ///
    memset((void *)&g_null_item,0, sizeof(g_null_item));

    // create sync mutex
    if (tn_mutex_create(&g_mutex, TN_MUTEX_ATTR_INHERIT, 0) != TERR_NO_ERR)
    {
        dbg_puts("tn_mutex_create(&g_mutex) error");
        dbg_trace();
        tn_halt();
    }
    /////////////////////
    Event_Check_Pointers();
    ///////////////////


    /*
    if (g_write_p != g_read_p)
    {
        if (g_write_p)
            flash_rd(FLASH_EVENT, (unsigned long)(g_write_p-1), (unsigned char*)tmp_item, sizeof(tmp_item[0]));
        else
            flash_rd(FLASH_EVENT, (unsigned long)flash_sz(FLASH_EVENT) - sizeof(tmp_item[0]), (unsigned char*)tmp_item, sizeof(tmp_item[0]));
        g_read_p++;
    }
    */

    dbg_printf("[done, size %u, used %u, free %u]\n", evt_fifo_size(), evt_fifo_used(), evt_fifo_free());
}


//------------------------------------------------------------------------------
BOOL evt_fifo_push(struct evt_fifo_item_t* item)
{
    BOOL ret = TRUE;
    //struct evt_fifo_item_t tmp_item;
    struct evt_fifo_item_t* g_wr_save;
    struct evt_fifo_item_t* g_wr_zero_save;
    //////
    //if (evt_fifo_full())
    //    return FALSE;
    ///
    g_wr_save = g_write_p;
    Next_Pointer(&g_write_p);
    g_wr_zero_save = g_write_p;
    ///
     if (!item_write(item))
        ret=FALSE;
    ////////////
    if (  Next_Pointer(&g_write_p))
    {
      g_wr_zero_save = g_write_p;
      if (!item_write(item))
          ret=FALSE;
      //
      Next_Pointer(&g_write_p);

    }
    // В g_write - указатель на 0
    /// пишем впереди чистый ивент
    if (!item_write(&g_null_item))
        ret=FALSE;
    //
    g_write_p = g_wr_zero_save;
    //flash_wr(FLASH_EVENT, (unsigned long)g_write_p, (unsigned char*)&g_null_item, sizeof(*item));
    if (!ret)
    {
      //восстановим указатель, хотя зачем???))
       g_write_p = g_wr_save;

    }

    ////////////
#if defined(EVT_FIFO_TRACE) && EVT_FIFO_TRACE != FALSE
    dbg_printf("size %u, used %u, free %u, data 0x%08X, empty 0x%08X\n",
        evt_fifo_size(), evt_fifo_used(), evt_fifo_free(),
        (unsigned)g_read_p, (unsigned)g_write_p);
#endif // EVT_FIFO_TRACE

    return (ret);
}
//------------------------------------------------------------------------------

BOOL evt_fifo_get(struct evt_fifo_item_t* item)
{
    if (tn_mutex_lock_polling(&g_mutex) != TERR_NO_ERR)
        return FALSE;

    //if (g_read_p != g_write_p) // !evt_fifo_empty() without recursive locking
    //{
        cache_update();
        BOOL const rc = cache_get(item);
        //

        //
        tn_mutex_unlock(&g_mutex);
        return rc;
    //}

   // tn_mutex_unlock(&g_mutex);
   // return FALSE;
}
//------------------------------------------------------------------------------

BOOL evt_fifo_get_direct(unsigned long pos, struct evt_fifo_item_t* item)
{
    BOOL ret=FALSE;
    ///
    if (tn_mutex_lock_polling(&g_mutex) != TERR_NO_ERR)
        return FALSE;
    ////
    if (pos)
      pos--;
    ///
    unsigned long read_addr = pos*sizeof(struct evt_fifo_item_t);
    if (read_addr< flash_sz(FLASH_EVENT) )
    {
       flash_rd(FLASH_EVENT, (unsigned long)read_addr, (unsigned char*)item, sizeof(struct evt_fifo_item_t));
       //
       if (item->dat_len != 0 && crc_is_valid(item))
         ret=TRUE;
    }
    //
    tn_mutex_unlock(&g_mutex);
    return (ret);
}

//------------------------------------------------------------------------------

BOOL evt_fifo_pop()
{
    if (evt_fifo_empty())
        return FALSE;

    tn_mutex_lock(&g_mutex, TN_WAIT_INFINITE);

    if (g_read_p)
        flash_wr(FLASH_EVENT, (unsigned long)(g_read_p-1), (unsigned char*)&g_null_item, sizeof(g_null_item));
    else
        flash_wr(FLASH_EVENT, (unsigned long)flash_sz(FLASH_EVENT)-sizeof(g_null_item), (unsigned char*)&g_null_item, sizeof(g_null_item));
    if ((unsigned)++g_read_p >= flash_sz(FLASH_EVENT))
        g_read_p = NULL;

    cache_update();
    tn_mutex_unlock(&g_mutex);

#if defined(EVT_FIFO_TRACE) && EVT_FIFO_TRACE != FALSE
    dbg_printf("size %u, used %u, free %u, data 0x%08X, empty 0x%08X\n",
        evt_fifo_size(), evt_fifo_used(), evt_fifo_free(),
        (unsigned)g_read_p, (unsigned)g_write_p);
#endif // EVT_FIFO_TRACE

    return TRUE;
}
//------------------------------------------------------------------------------

unsigned evt_fifo_size()
{
    return g_fifo_size;
}
//------------------------------------------------------------------------------

unsigned evt_fifo_used()
{
    tn_mutex_lock(&g_mutex, TN_WAIT_INFINITE);

    unsigned const used =
        (unsigned)g_write_p >= (unsigned)g_read_pu ?
        ((unsigned)g_write_p - (unsigned)g_read_pu) / sizeof(struct evt_fifo_item_t) :
        (flash_sz(FLASH_EVENT) - (unsigned)g_read_pu + (unsigned)g_write_p) / sizeof(struct evt_fifo_item_t);

    tn_mutex_unlock(&g_mutex);
    return used;
}
//------------------------------------------------------------------------------

unsigned evt_fifo_free()
{
    return g_fifo_size - evt_fifo_used();
}
//------------------------------------------------------------------------------
BOOL evt_fifo_empty()
{
    tn_mutex_lock(&g_mutex, TN_WAIT_INFINITE);
    BOOL const empty = g_read_p == g_write_p;
    tn_mutex_unlock(&g_mutex);
    return empty;
}
//------------------------------------------------------------------------------
BOOL evt_fifo_full()
{
    return evt_fifo_used() == g_fifo_size;
}
//------------------------------------------------------------------------------
void evt_fifo_prepare_evt(struct evt_fifo_item_t* item)
{
    memset(item, 0, sizeof(*item));
    item->ts = time(NULL);
}
//------------------------------------------------------------------------------
// local functions

static BOOL crc_is_valid(struct evt_fifo_item_t* item_p)
{
    static unsigned char cr;
    ///
    cr = crc32_calc((unsigned char*)item_p, sizeof(*item_p) - 1);
    return (item_p->crc == cr);
}
//------------------------------------------------------------------------------
static void crc_calc(struct evt_fifo_item_t* item_p)
{
    item_p->crc = crc32_calc((unsigned char*)item_p, sizeof(*item_p) - 1);
}
//------------------------------------------------------------------------------
static BOOL cache_get(struct evt_fifo_item_t* item_p)
{
    if (g_cache_is_valid)
        memcpy(item_p, &g_cache, sizeof(g_cache));
    return g_cache_is_valid;
}
//------------------------------------------------------------------------------
static void cache_set(struct evt_fifo_item_t* item_p)
{
    if (item_p != NULL)
    {
        memcpy(&g_cache, item_p, sizeof(g_cache));
        g_cache_is_valid = TRUE;
    }
    else
        g_cache_is_valid = FALSE;
}
//------------------------------------------------------------------------------
static void cache_update()
{
    struct evt_fifo_item_t tmp_item;
    //////
    cache_set(NULL);
    //
    for (int i=0; i<7; i++)
    {
        flash_rd(FLASH_EVENT, (unsigned long)g_read_p, (unsigned char*)&tmp_item, sizeof(tmp_item));
        if (tmp_item.dat_len != 0 && crc_is_valid(&tmp_item))
        {
            cache_set(&tmp_item);
            ////
            //if ((unsigned)++g_read_p >= flash_sz(FLASH_EVENT))
            if (g_read_p)
            {
              g_read_p--;
            }
            else
            {
              g_read_p = (struct evt_fifo_item_t*)(flash_sz(FLASH_EVENT) - sizeof(tmp_item));
            }
             //break;
            //////
            return;
        }
     }
    //}
}
//------------------------------------------------------------------------------