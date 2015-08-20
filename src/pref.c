/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex, added  Tuzhikov Alex
*
*****************************************************************************/

#include <string.h> // memcpy()
#include <stdio.h>
#include "tnkernel/tn.h"
#include "lwip/lwiplib.h"
#include "memory/memory.h"
#include "crc32.h"
#include "pref.h"
#include "dk/dk.h"

#include "debug/debug.h"

static TN_MUTEX g_mutex;
static long     g_pref_cache[PREF_L_CNT];
static volatile BOOL fRestoreIP = false; // флаг, надо востановить данные IP предыдущие

#if PREF_L_CNT * 4 > EEPROM_PREF_SIZE/2 // PREF_L_CNT * sizeof(long)
    #error Too many preferences data, see EEPROM_PREF_SIZE in memory/memory.h
#endif

// local functions
static BOOL pref_load();
static void pref_save();
static void pref_load_def();
static void lock();
static void unlock();

/*init IP parameters---------------------------------------------------------*/
void pref_init()
{
if (tn_mutex_create(&g_mutex, TN_MUTEX_ATTR_INHERIT, 0) != TERR_NO_ERR){
    dbg_puts("tn_mutex_create(&g_mutex) != TERR_NO_ERR");
    dbg_trace();
    tn_halt();
    }

lock();
  dbg_printf("Loading preferences...");
    const int pref_b = pref_load(); // ip параметры получить

    if (pref_b){
      dbg_puts("[done]");
      }else{
       pref_load_def();
       pref_save();
       dbg_puts("[done, defaults was loaded]");
      }
unlock();
}
/* ------------------------установка ip по умолчанию  с сохранение -----------*/
void pref_reset()
{
  lock();
  pref_load_def();
  pref_save();
  unlock();
}
/* ------------------------установка ip по умолчанию  без сохранения ---------*/
void pref_reset_default()
{
lock();
pref_load_def();
struct ip_addr ipaddr   = {pref_get_long(PREF_L_NET_IP)};
struct ip_addr netmask  = {pref_get_long(PREF_L_NET_MSK)};
struct ip_addr gw       = {pref_get_long(PREF_L_NET_GW)};
setNetParamerts(&ipaddr,&netmask,&gw);
unlock();
}
/*--------------установить новые IP адреса------------------------------------*/
void setParametrIP(struct ip_addr* ipaddr, struct ip_addr* netmask, struct ip_addr* gw)
{
lock();
setNetParamerts(ipaddr,netmask,gw);
unlock();
}
/*--- запрос ip параметров----------------------------------------------------*/
long pref_get_long(enum pref_l_idx idx)
{
#ifdef DEBUG
    if ((unsigned)idx >= PREF_L_CNT)
    {
        dbg_printf("pref_get_long(idx == UNKNOWN[%d]) error", idx);
        tn_halt();
    }
#endif // DEBUG

    return g_pref_cache[idx];
}
/*установить ip параметров----------------------------------------------------*/
void pref_set_long(enum pref_l_idx idx, long value)
{
#ifdef DEBUG
    if ((unsigned)idx >= PREF_L_CNT)
    {
        dbg_printf("pref_get_long(idx == UNKNOWN[%d]) error", idx);
        tn_halt();
    }
#endif // DEBUG

    lock();
    g_pref_cache[idx] = value;
    pref_save();
    unlock();
}
/*--- чтение ip параметров из flash-------------------------------------------*/
static BOOL pref_load()
{
    unsigned char count = 5; // попытки чтения настроек
    unsigned long crc32;


/*
    memset (g_pref_cache, 0xFF, sizeof(g_pref_cache));
    eeprom_wr(EEPROM_PREF, 0, (unsigned char*)g_pref_cache, sizeof(g_pref_cache));
    eeprom_wr(EEPROM_PREF, 256, (unsigned char*)g_pref_cache, sizeof(g_pref_cache));
    eeprom_wr(EEPROM_PREF, 512, (unsigned char*)g_pref_cache, sizeof(g_pref_cache));
    eeprom_wr(EEPROM_PREF, 512+256, (unsigned char*)g_pref_cache, sizeof(g_pref_cache));
    while (1);
*/

    do
    {
        flash_rd(FLASH_PREF, 0, (unsigned char*)g_pref_cache, sizeof(g_pref_cache));
        crc32 = crc32_calc((unsigned char*)g_pref_cache, sizeof(g_pref_cache)-sizeof(g_pref_cache[PREF_L_CRC32]));
        count--;
    }while (count && crc32 != g_pref_cache[PREF_L_CRC32]);

    if (crc32 != g_pref_cache[PREF_L_CRC32])
    {
        flash_wr(FLASH_PREF, 256, (unsigned char*)g_pref_cache, sizeof(g_pref_cache));

        count = 5;
        do
        {
            flash_rd(FLASH_PREF, 512, (unsigned char*)g_pref_cache, sizeof(g_pref_cache));
            crc32 = crc32_calc((unsigned char*)g_pref_cache, sizeof(g_pref_cache)-sizeof(g_pref_cache[PREF_L_CRC32]));
            count--;
        }while (count && crc32 != g_pref_cache[PREF_L_CRC32]);

        if (crc32 != g_pref_cache[PREF_L_CRC32])
        {
            flash_wr(FLASH_PREF, 512+256, (unsigned char*)g_pref_cache, sizeof(g_pref_cache));
            return FALSE;
        }
        flash_wr(FLASH_PREF, 0, (unsigned char*)g_pref_cache, sizeof(g_pref_cache));
    }

    return TRUE;
}
/*--- save to flash-----------------------------------------------------------*/
static void pref_save()
{
    g_pref_cache[PREF_L_CRC32] = crc32_calc((unsigned char*)g_pref_cache, sizeof(g_pref_cache)-sizeof(g_pref_cache[PREF_L_CRC32]));
    flash_wr(FLASH_PREF, 0, (unsigned char*)g_pref_cache, sizeof(g_pref_cache));
    flash_wr(FLASH_PREF, 512, (unsigned char*)g_pref_cache, sizeof(g_pref_cache));
}
/*установк параметров IP по умолчанию ----------------------------------------*/
static void pref_load_def()
{
    struct ip_addr addr;

    g_pref_cache[PREF_L_NET_MODE]           = NET_MODE_STATIC_IP; //NET_MODE_DHCP
   #ifdef DEBUG
    IP4_ADDR(&addr, 169, 254, 16, 1);
    #else
    IP4_ADDR(&addr, 169, 254, 16, 1);
   #endif
    g_pref_cache[PREF_L_NET_IP]             = addr.addr;

    IP4_ADDR(&addr, 255, 255, 255, 0);
    g_pref_cache[PREF_L_NET_MSK]            = addr.addr;

    IP4_ADDR(&addr, 0, 0, 0, 0);
    g_pref_cache[PREF_L_NET_GW]             = addr.addr;

    IP4_ADDR(&addr,0,0,0,0);
    g_pref_cache[PREF_L_CMD_SRV_IP] = addr.addr;
    g_pref_cache[PREF_L_CMD_PORT]           = 11990;
//    g_pref_cache[PREF_L_MAC_1]              = 0x001AB600;
//    g_pref_cache[PREF_L_MAC_2]              = 0;
    g_pref_cache[PREF_L_PWM_RGY]             = 0x623212;
    g_pref_cache[PREF_DELAY_LIGHT_ON]        = 0;
    g_pref_cache[PREF_DELAY_LIGHT_OFF]       = 0;
    g_pref_cache[PREF_RF_ADDR_PULT_HI]        = 0x00000000;
    g_pref_cache[PREF_RF_ADDR_PULT_LO]        = 0x00000000;
}
/* LOCK-----------------------------------------------------------------------*/
static void lock()
{
    if (tn_mutex_lock(&g_mutex, TN_WAIT_INFINITE) != TERR_NO_ERR)
    {
        dbg_puts("tn_mutex_lock(&g_mutex) != TERR_NO_ERR");
        dbg_trace();
        tn_halt();
    }
}
/*  unlock--------------------------------------------------------------------*/
static void unlock()
{
    if (tn_mutex_unlock(&g_mutex) != TERR_NO_ERR)
    {
        dbg_puts("tn_mutex_unlock(&g_mutex) != TERR_NO_ERR");
        dbg_trace();
        tn_halt();
    }
}
/* запись данных -------------------------------------------------------------*/
void pref_data_wr(unsigned short addr, unsigned char* data, unsigned short len)
{
    lock();
    memcpy(((unsigned char*)g_pref_cache)+addr, data, len);
    pref_save();
    unlock();
}
/*  чтение данных ------------------------------------------------------------*/
void pref_data_rd(unsigned short addr, unsigned char* dest, unsigned short len)
{
    lock();
    memcpy(dest, ((unsigned char*)g_pref_cache)+addr, len);
    unlock();
}
/* флаг перекинуть адреса по умолчанию*/
void setFlagDefaultIP(BOOL fl)
{
lock();
fRestoreIP = fl;
unlock();
}
BOOL getFlagDefaultIP(void)
{
bool temp = false;
lock();
temp = fRestoreIP;
fRestoreIP = false;
unlock();
if(temp)return true;
return false;
}
/* сохранить параметры ip из проекта------------------------------------------*/
void saveDatePorojectIP(void)
{
const TPROJECT *prj = retPointPROJECT();

if(prj->surd.ID_DK_CUR < prj->maxDK){ //проверим
  // save IP addr
  const char *pStrIP = (char*)prj->surd.ip[prj->surd.ID_DK_CUR];
  unsigned ip[4];
  if(sscanf(pStrIP,"%u.%u.%u.%u",
            &ip[0],&ip[1],&ip[2],&ip[3])==4){
    struct ip_addr addr;
    IP4_ADDR(&addr,ip[0],ip[1],ip[2],ip[3]);
    // проверка ip на пустышку
    if((addr.addr==0)||(addr.addr==0xFFFFFFFF)||
       (addr.addr==0x0100007F)){// ip адресс пустышка
      IP4_ADDR(&addr,169,254,16,1);
      }
    //проверка IP на равенство
    struct in_addr ipold = {pref_get_long(PREF_L_NET_IP)};
    if(addr.addr!=ipold.s_addr){// сохранить если ip другой
            pref_set_long(PREF_L_NET_IP,addr.addr);
            tn_reset(); // сбросс CPU
            }
    }
  //save mac addr
  unsigned hw[6];
  const char *pStrMAC = (char*)prj->surd.mac[prj->surd.ID_DK_CUR];
  if(sscanf(pStrMAC, "%02X:%02X:%02X:%02X:%02X:%02X", &hw[0], &hw[1], &hw[2], &hw[3], &hw[4], &hw[5]) == 6){
    if( (hw[0] != 0xFF && hw[1] != 0xFF &&
         hw[2] != 0xFF && hw[3] != 0xFF &&
         hw[4] != 0xFF && hw[5] != 0xFF)||
       ((hw[0]|hw[1]|hw[2]|hw[3]|hw[4]|hw[5])!= 0)){
       long MAC1 = hw[0] | (hw[1]<<8) | (hw[2]<<16) | (hw[3]<<24);
       long MAC2 = hw[4] | (hw[5]<<8);
       long MAC1OLD = pref_get_long(PREF_L_MAC_1);
       long MAC2OLD = pref_get_long(PREF_L_MAC_2);
       //проверка MAC на равенство
       if((MAC1!=MAC1OLD)||(MAC2!=MAC2OLD)){
          pref_set_long(PREF_L_MAC_1,MAC1);
          pref_set_long(PREF_L_MAC_2,MAC2);
          tn_reset(); // сбросс CPU
          }
       }
    }
  //save PORT
  if(prj->surd.PORT){
    if(pref_get_long(PREF_L_CMD_PORT)!=prj->surd.PORT){
          pref_set_long(PREF_L_CMD_PORT,prj->surd.PORT);
          tn_reset(); // сбросс CPU
          }
    }
  }
}
/*----------------------------------------------------------------------------*/