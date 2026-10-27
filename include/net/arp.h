#pragma once

#include "sys/types.h"

#define ARP_HWTYPE_ETHER 1
#define ARP_PROTYPE_IPV4 0x0800
#define ARP_OP_REQUEST   1
#define ARP_OP_REPLY     2

typedef struct {
    u16 hw_type;
    u16 proto_type;
    u8  hw_size;
    u8  proto_size;
    u16 op;
    u8  sender_mac[6];
    u32 sender_ip;
    u8  target_mac[6];
    u32 target_ip;
} __attribute__((packed)) arp_packet_t;

void arp_init(void);
void arp_handle(u8 *frame, u16 len);
void arp_request(u32 target_ip);
int arp_resolve(u32 ip, u8 out_mac[6]);
