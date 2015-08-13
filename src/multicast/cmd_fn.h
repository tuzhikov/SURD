/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __CMD_FN_H__
#define __CMD_FN_H__

#include "cmd_ch.h"

void cmd_debugee_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_light_func(struct cmd_raw* cmd_p, int argc, char** argv);

void cmd_polling_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_setphase_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_answer_surd_func(struct cmd_raw* cmd_p, int argc, char** argv);

void cmd_ifconfig_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_config_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_get_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_set_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_set_rf_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_reboot_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_prefReset_func(struct cmd_raw* cmd_p, int argc, char** argv);

void cmd_flashwr_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_pflashwr_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_proj_flashwr_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_progs_flashwr_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_flashrd_func(struct cmd_raw* cmd_p, int argc, char** argv);

void cmd_proj_flashrd_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_progs_flashrd_func(struct cmd_raw* cmd_p, int argc, char** argv);


void cmd_flash_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_flashNFO_func(struct cmd_raw* cmd_p, int argc, char** argv);

void cmd_test_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_chan_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_line_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_help_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_event_func(struct cmd_raw* cmd_p, int argc, char** argv);
void cmd_calibr_bat(struct cmd_raw* cmd_p, int argc, char** argv);

void debugee(char const* s);
void Byte_to_Bin(unsigned char bb, char *str);

#endif // __CMD_FN_H__
