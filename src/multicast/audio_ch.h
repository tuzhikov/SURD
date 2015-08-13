/*****************************************************************************
*
* © 2011 BOLID Security Systems. Written by Alexander Kotikov.
*
*****************************************************************************/

#ifndef __AUDIO_CH_H__
#define __AUDIO_CH_H__

#include "../lwip/lwiplib.h"

enum audio_ch   { CH0, CH1, CH_CNT };

void            audio_ch_init();
BOOL            is_audio_ch_playing();

void            audio_ch_disable(); // used for playing flash messages
BOOL            audio_ch_is_enabled();
void            audio_ch_enable();  // used for playing flash messages

enum audio_ch   get_act_audio_ch();                 // get active audio channel
void            set_act_audio_ch(enum audio_ch ch); // set active audio channel
void            get_bitrate(unsigned* bitrate, unsigned* cbitrate);
BOOL            get_audio_srv_ip(struct ip_addr* srv_addr);
unsigned        get_audio_srv_port_ch0();
unsigned        get_audio_srv_port_ch1();
BOOL            is_audio_ch_playing();
void            audio_ch_timer_int_processing();

#endif // __AUDIO_CH_H__
