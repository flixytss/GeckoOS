#ifndef _MEM_H
#define _MEM_H

#include "sys/types.h"

#define ALIGN(x, y) (((x) + (y-1)) & ~(y-1))
#define assert(x) if(!x) { printf("%s: assertion failed (%s)\n", __func__, #x); for(;;); }
#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000
#define HEAP_ALIGN 16

typedef struct header {
    u64 size;
    unsigned int free : 1;
    struct header* next;
} __attribute__((aligned(HEAP_ALIGN))) header_t;

static header_t* create_block(unsigned long size, size_t alignment);

void  kalloc_init(u64 start, u64 size);

void* kmalloc(u64 size);
void kfree(void* p);

#ifdef DEBUG
    void dump_heap();
#endif

#endif
