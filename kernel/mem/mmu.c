#include "mem/mmu.h"
#include "sys/std.h"
#include "sys/limits.h"
#include "terminal/printf.h"

static void *heap_ptr = NULL;
static void *heap_end = NULL;
static block *free_list_head = NULL;

#define BLOCK_BUFFER(x) ((u64)x + sizeof(block))
#define BLOCK_BUFFER_METADATA(x) ((u64)x - sizeof(block))

void kalloc_init(u64 start, u64 size) {
    heap_ptr = (void*)start;
    heap_end = heap_ptr + size;

    free_list_head = (block*)heap_ptr;
    free_list_head->free = true;
    free_list_head->size = size;
    free_list_head->next = NULL;
    free_list_head->magic = BLOCK_MAGIC;
}

// Divide the free_list_head into smaller blocks with the wanted size
static block *create_block(size_t size, size_t alignment) {
    if (!heap_ptr)
        return NULL;

    if (size == 0 || size > ULONG_LONG_MAX - sizeof(block) - alignment)
        return NULL;

    size = ALIGN(size + sizeof(block), alignment);

    block *curl = free_list_head;

    while (curl) {
        if (curl->free && curl->size >= size) {
            size_t leftover = curl->size - size;

            if (leftover >= sizeof(block) + 16) {
                block* new_free = (block *)(curl + size);

                new_free->free = true;
                new_free->size = leftover;
                new_free->next = curl->next;

                curl->next = new_free;
                curl->size = size;
                curl->magic = BLOCK_MAGIC;
            }

            curl->free = false;
            return curl;
        }

        if (curl->free && curl->next && curl->next->free) {
            if (curl->next->magic != BLOCK_MAGIC) continue;
            curl->size += curl->next->size;
            curl->next = curl->next->next;
            continue;
        }

        curl = curl->next;
    }

    return NULL; // Out of memory
}


void *kmalloc(unsigned long size) {
    block* b = create_block(size, 4);

    return b ? (void *)(BLOCK_BUFFER(b)) : NULL;
}

#if 0
void* realloc(void* p, size_t size) { // DONT USE
    if (!p) return NULL;
    block* b = (block*)BLOCK_BUFFER_METADATA(p);

    b->free = true;
    char* new = kmalloc(size);

    memcpy(new, p, size > b->size ? b->size : size);

    return b;
}
#endif

void *kmalloc_align(unsigned long size, size_t alignment) {
    block* b = create_block(size, alignment);

    return b ? (void *)(BLOCK_BUFFER(b)) : NULL;
}

void dump_heap() {
    block* curl = heap_ptr;
    block* last = heap_ptr;
    while (curl) {
        last = curl;
        #ifdef DEBUG
            printf("  Block at %p (Buffer at %llu) with size = %zu bytes, free = %d, next = %p\n", curl, ((u64)curl + sizeof(block)), curl->size, curl->free, curl->next);
        #else
            printf("  Block at %p with size = %d bytes, free = %d, next = %p\n", curl, curl->size, curl->free, curl->next);
        #endif

        curl = curl->next;
    }
    printf("Free memory: %dMb\n", (u32)last->size / 1048576);
}

void kfree(void *ptr) {
    if (!ptr) return;

    block *b = (block*)(BLOCK_BUFFER_METADATA(ptr));
    if (b->magic != BLOCK_MAGIC) return;

    b->free = true;
}

// Something basic about dma, for pci devices
void* dma_create_pool(u64 size, u64 n, u32 alignment) {
    return memset((void*)ALIGN((u64)kmalloc_align(size * n, alignment), alignment), 0, size * n);
    /*
    * kmalloc_align just align the size with the passed alignment, so you just can align the
    * returned address. someone has to remake that, it waste memory
    */
}
