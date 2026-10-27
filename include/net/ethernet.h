#pragma once

#include "sys/types.h"

#define ETHER_TYPE_ARP  0x0806
#define ETHER_TYPE_IPV4 0x0800

#define MAC_BROADCAST   ((u8[6]){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF})

typedef struct {
    u8  dst[6];
    u8  src[6];
    u16 ether_type;
    u8  payload[];
} __attribute__((packed)) eth_frame_t;
