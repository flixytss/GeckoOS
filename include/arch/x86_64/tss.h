#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct TSSStruct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb;
} __attribute__((packed)) TSSStruct_t;

extern TSSStruct_t global_tss;
// unsigned char tss_size = sizeof(global_tss);