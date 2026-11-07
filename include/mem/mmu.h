// bonk enjoyer (dorito girl)

// Should i write down everything i did? (Pumpkicks)
#ifndef _MEM_H
#define _MEM_H
//idk but they say aligning is important
#include <stdbool.h>
#include "sys/types.h"

#define ALIGN(x, y) (((x) + (y-1)) & ~(y-1))
#define assert(x) if(!x) { printf("%s: assertion failed (%s)\n", __func__, #x); for(;;); }

#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000
#define BLOCK_MAGIC 0xABACBC112

typedef struct block block;
struct block{
    u64 magic; // Who need magic numbers
    size_t size;
    bool free;
    block* next;
};

// Ember2819
static block* create_block(unsigned long size, size_t alignment);

void  kalloc_init(u64 start, u64 size);
void* kmalloc(unsigned long size);
void* realloc(void* p, size_t size);
void* kmalloc_align(unsigned long size, size_t alignment);

void dump_heap();
void kfree(void* p);
void combine_blocks();

void* dma_create_pool(u64 size, u64 n, u32 alignment);

#endif
