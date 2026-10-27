#pragma once

#include "sys/types.h"

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0

typedef struct {
    u8  type;
    u8  code;
    u16 checksum;
    u16 identifier;
    u16 sequence;
    u8  payload[];
} __attribute__((packed)) icmp_header_t;

void icmp_handle(u32 src_ip, u8 *data, u16 len);
void icmp_send_echo_request(u32 dst_ip, u16 id, u16 seq);
int  icmp_got_reply(void);
void icmp_clear_reply(void);
