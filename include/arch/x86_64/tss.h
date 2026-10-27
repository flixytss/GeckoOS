#pragma once

#include "sys/types.h"

typedef struct TSSStruct {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1;
    u64 ist2;
    u64 ist3;
    u64 ist4;
    u64 ist5;
    u64 ist6;
    u64 ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iopb;
} __attribute__((packed)) TSSStruct_t;

extern TSSStruct_t global_tss;
// unsigned char tss_size = sizeof(global_tss);