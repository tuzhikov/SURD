/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#ifndef __IPMULTICAST_H__
#define __IPMULTICAST_H__

// UDP port 11990 - command channel;
// Default IP Multicast Group - 224.0.224.1
// UDP port 11991 - common audio channel;   udp://@224.0.224.1:11991
// UDP port 11992 - messages audio channel; udp://@224.0.224.1:11992

void ipmulticast_init();
void get_multicast_ip(struct ip_addr* ipaddr);
BOOL If_ready(void);

#endif // __IPMULTICAST_H__
