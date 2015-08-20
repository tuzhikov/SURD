/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex, add  Tuzhikov Alex
*
*****************************************************************************/

#ifndef __PREF_H__
#define __PREF_H__

enum pref_l_idx
{
    PREF_L_NET_MODE,
    PREF_L_NET_IP,
    PREF_L_NET_MSK,
    PREF_L_NET_GW,
    PREF_L_CMD_SRV_IP,
    PREF_L_CMD_PORT,
    PREF_L_MAC_1,
    PREF_L_MAC_2,
    PREF_L_PWM_RGY,
    PREF_DELAY_LIGHT_ON,
    PREF_DELAY_LIGHT_OFF,
    PREF_RF_ADDR_PULT_HI,
    PREF_RF_ADDR_PULT_LO,
    RESERVED_00,
    RESERVED_01,
    RESERVED_02,
    RESERVED_03,
    RESERVED_04,
    RESERVED_05,
    RESERVED_06,
    RESERVED_07,
    RESERVED_08,
    RESERVED_09,
    PREF_L_CRC32,
    PREF_L_CNT
};
/*----------------------------------------------------------------------------*/
void pref_init();
void pref_reset();
void pref_reset_default();
long pref_get_long(enum pref_l_idx idx);
void pref_set_long(enum pref_l_idx idx, long value);
// задать новые параметры связи для ethernet
void saveDatePorojectIP(void);
//void setParametrIP(struct ip_addr* ipaddr, struct ip_addr* netmask, struct ip_addr* gw);
void setFlagDefaultIP(const BOOL fl);
BOOL getFlagDefaultIP(void);
//void pref_data_wr(unsigned short addr, unsigned char* data, unsigned short len);
//void pref_data_rd(unsigned short addr, unsigned char* dest, unsigned short len);

#endif // __PREF_H__
