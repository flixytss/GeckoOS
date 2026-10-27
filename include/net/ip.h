#pragma once

#include "sys/types.h"

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

#define IP_DF 0x4000

typedef struct {
    u8  ihl:4;
    u8  version:4;
    u8  dscp:6;
    u8  ecn:2;
    u16 total_length;
    u16 identification;
    u16 flags_fragment;
    u8  ttl;
    u8  protocol;
    u16 checksum;
    u32 src_ip;
    u32 dst_ip;
    u8  payload[];
} __attribute__((packed)) ip_header_t;

u16 ip_checksum(void *data, u16 len);
void ip_send(u32 dst_ip, u8 protocol, const void *payload, u16 payload_len);
void ip_handle(u8 *frame, u16 len);
