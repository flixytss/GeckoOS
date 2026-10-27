#include <net/icmp.h>
#include <net/ip.h>
#include <net/net.h>
#include <mem.h>
#include "terminal/printf.h"

static volatile int reply_received = 0;

int icmp_got_reply(void) {
    return reply_received;
}

void icmp_clear_reply(void) {
    reply_received = 0;
}

static u16 icmp_checksum(void *data, u16 len) {
    u32 sum = 0;
    u16 *buf = (u16 *)data;

    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    if (len == 1)
        sum += *(u8 *)buf;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return ~sum;
}

void icmp_handle(u32 src_ip, u8 *data, u16 len) {
    icmp_header_t *icmp = (icmp_header_t *)data;

    if (icmp->type == ICMP_ECHO_REQUEST) {
        u16 payload_len = len - sizeof(icmp_header_t);
        static u8 reply_buf[1024];

        icmp_header_t *reply = (icmp_header_t *)reply_buf;
        reply->type       = ICMP_ECHO_REPLY;
        reply->code       = 0;
        reply->checksum   = 0;
        reply->identifier = icmp->identifier;
        reply->sequence   = icmp->sequence;
        memcpy(reply->payload, icmp->payload, payload_len);
        reply->checksum = icmp_checksum(reply, sizeof(icmp_header_t) + payload_len);

        ip_send(src_ip, IP_PROTO_ICMP, reply, sizeof(icmp_header_t) + payload_len);
    } else if (icmp->type == ICMP_ECHO_REPLY) {
        reply_received = 1;
    }
}

void icmp_send_echo_request(u32 dst_ip, u16 id, u16 seq) {
    static u8 buf[128];
    const char *msg = "GeckoOS ping";
    u16 payload_len = 12;

    icmp_header_t *icmp = (icmp_header_t *)buf;
    icmp->type       = ICMP_ECHO_REQUEST;
    icmp->code       = 0;
    icmp->checksum   = 0;
    icmp->identifier = __builtin_bswap16(id);
    icmp->sequence   = __builtin_bswap16(seq);
    memcpy(icmp->payload, msg, payload_len);
    icmp->checksum = icmp_checksum(icmp, sizeof(icmp_header_t) + payload_len);

    ip_send(dst_ip, IP_PROTO_ICMP, icmp, sizeof(icmp_header_t) + payload_len);
}
