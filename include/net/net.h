#pragma once

#include "sys/types.h"

#define IP(a,b,c,d) ((u32)(((a)&0xFF)<<24|((b)&0xFF)<<16|((c)&0xFF)<<8|((d)&0xFF)))

extern u32 net_ip;
extern u32 net_gateway;
extern u32 net_subnet;
extern u8  net_mac[6];

void net_init(void);
void net_handle_packet(u8 *data, u16 len);
void net_send_frame(u8 dst_mac[6], u16 ether_type, const void *payload, u16 len);
