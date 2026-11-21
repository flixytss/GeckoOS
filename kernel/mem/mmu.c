#include "mem/mmu.h"
#include "terminal/printf.h"

static void *heap_ptr;
static void *heap_end;

static header_t* head;

void kalloc_init(u64 start, u64 size) {
    heap_ptr = (void*)start;
    heap_end = heap_ptr + size;

    head = (header_t*)heap_ptr;
    head->size = size;
    head->free = 1;
    head->next = NULL;

    #ifdef DEBUG
        dprintf("OK : kalloc initialized with; {address: %p, size: %llub}\n", head, head->size);
    #endif
}

static header_t* get_free_block(u64 size) {
    for (header_t* cur = head; cur; cur = cur->next) {
        if (cur->free && cur->size >= size)
            return cur;
    }
    return NULL;
}
 
static void split_block(header_t* block, u64 size) {
    u64 spare = block->size - size;
    if (spare < sizeof(header_t) + HEAP_ALIGN)
        return;
 
    header_t* rest = (header_t*)((char*)(block + 1) + size);
    rest->size = spare - sizeof(header_t);
    rest->free = 1;
    rest->next = block->next;
    block->next = rest;
    block->size = size;
}
 
void* kmalloc(u64 size) {
    if (!size) return NULL;
    if (size > (u64)-1 - (HEAP_ALIGN - 1)) return NULL;
    size = ALIGN(size, HEAP_ALIGN);
 
    header_t* block = get_free_block(size);
    if (!block) {
#ifdef DEBUG
        dprintf("ERR: there is no available header\n");
#endif
        return NULL;
    }
 
    split_block(block, size);
    block->free = 0;
 
#ifdef DEBUG
    dprintf("OK : block %p, size %llu\n", (void*)block, (unsigned long long)block->size);
#endif
    return block + 1;
}

void kfree(void *p) {
    if (!p) return;

    header_t* header = p - sizeof(header_t);
    header->free = 1;

    #ifdef DEBUG
        dprintf("LOG: header freed (addr: %p, size: %llu)\n", header, header->size);
    #endif
}

#ifdef DEBUG
    void dump_heap() {
        debug_printf(NULL, "\n-- Heap dump --\n\n\r");

        header_t* curl = head;

        while (curl) {
            debug_printf(NULL, "block; {address: 0x%p, size: %llu, free: %d}\n\r", curl, curl->size, curl->free);

            curl = curl->next;
        }

        debug_printf(NULL, "\n-- Heap dump end --\n\r");
    }
#endif