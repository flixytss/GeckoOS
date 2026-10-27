#pragma once
#include "boot/multiboot2.h"
#include "sys/types.h"

#define AP_TRAMPOLINE_ADDR 0x8000
#define AP_TRAMPOLINE_SIZE 0x1000

void free_blocks(const void *address, u32 num_blocks);

void set_block(u32 bit);

void *allocate_blocks(u32 num_blocks);

void deinitialize_memory_region(u64 base_address, u64 size);

void initialize_memory_manager_from_mbi(u64 mbi_addr);

i32 find_first_free_blocks(u32 num_blocks);
