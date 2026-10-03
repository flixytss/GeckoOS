#include "terminal/printf.h"
#include <mem.h>
#include <drivers/vga.h>
#include <gk/gk.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <terminal/terminal.h>

void *memcpy(void *dest, const void *src, unsigned long n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    for (unsigned long i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void *memmove(void *dest, const void *src, unsigned long n) {
    asm volatile("rep movsb" : : "D"(dest), "S"(src), "c"(n));
    return dest;
}

// [Ember2819: BEGIN - memset implementation]
void *memset(void *dest, int val, unsigned long n) {
    asm volatile("rep stosb" : : "D"(dest), "c"(n), "a"(val));
    return dest;
}
// [Ember2819: END]

// Pumpkicks
int strlen(char *ptr) {
    int i = 0;
    while (ptr[i])
        i++;
    return i;
}
uint8_t strcmp(const char *s, const char *d) {
    while (*s == *d && *s)
        s++, d++;
    return *(uint8_t *)s - *(uint8_t *)d;
}
uint8_t strcasecmp(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a;
        char cb = *b;

        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';

        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';

        if (ca != cb)
            return ca - cb;

        a++;
        b++;
    }

    return *a - *b;
}

uint8_t dlim(char c, const char *delim) {
    while (*delim) {
        if (c == *delim)
            return 1;
        delim++;
    }
    return 0;
}

char *strtok_r(char *restrict str, const char *restrict delim, char **saveptr) {
    char  *start;
    char **next = saveptr;

    if (str != NULL)
        *next = str;
    if (!next || !*next)
        return NULL;

    while (**next && dlim(**next, delim))
        (*next)++;

    if (**next == '\0') {
        *next = NULL;
        return NULL;
    }

    start = *next;

    while (**next && !dlim(**next, delim))
        (*next)++;

    if (**next) {
        **next = '\0';
        (*next)++;
    } else {
        *next = NULL;
    }

    return start;
}

uint8_t strncmp(const char *s1, const char *s2, size_t siz) {
    while (siz && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        siz--;
    }

    if (siz == 0)
        return 0;

    return *(uint8_t *)s1 - *(uint8_t *)s2;
}

char *strtok(char *restrict str, const char *restrict delim) {
    static char *saveptr;
    return strtok_r(str, delim, &saveptr);
}
 

void strncpy(char *restrict dst, const char *restrict src, size_t siz) {
    while (siz--) {
        *dst++ = *src++;
    }
}

void strcpy(char *restrict dst, const char *restrict src) {
    while ((*dst++ = *src++));
}

char *strchr(const char *s, int c) {
    while(*s) {
        if(*s == c) break;
        s++;
    }
    return (char *)s;
}

char *strrchr(const char *s, int c) {
    const char *l = NULL;
    while (*s) {
        if (*s == (char)c)
            l = s;
        s++;
    }
    if (!c)
        return (char *)s;
    return (char *)l;
}

char* strdup(const char* str) {
    uint64_t s = strlen(str);
    char* ret = kmalloc(s);
    memcpy(ret, str, s);
    return ret;
}

// replace with real allocator later but should be fine for now
// kotofyt: it is not
// pumpkicks: is this enough?

static void *heap_ptr = NULL;
static void *heap_end = NULL;
static block *free_list_head = NULL;

#define BLOCK_BUFFER(x) ((uint64_t)x + sizeof(block))
#define BLOCK_BUFFER_METADATA(x) ((uint64_t)x - sizeof(block))

// Let's just have a heap with 3MB of size, that is enough (i think)
// Grub needs at least 5MB to boot so that is fine

void kalloc_init(uint64_t start, uint64_t size) {
    heap_ptr = (void*)start;
    heap_end = heap_ptr + size;

    free_list_head = (block*)heap_ptr;
    free_list_head->free = true; // CRASH
    free_list_head->size = heap_end - heap_ptr;
    free_list_head->next = NULL;
}

// Divide the free_list_head into smaller blocks with the wanted size
static block *create_block(unsigned long size, size_t alignment) {
    if (!heap_ptr) {
        printf("You should initialize the heap\n");
        return NULL;
    }

    size_t needed = ALIGN(size + sizeof(block), alignment);

    block *curl = heap_ptr;

    while (curl) {
        if (curl->free && curl->size >= needed) {
            size_t leftover = curl->size - needed;

            if (leftover >= sizeof(block) + 16) {
                block *new_free = (block *)(BLOCK_BUFFER(curl) + needed - sizeof(block));

                new_free->free = true;
                new_free->size = leftover;
                new_free->next = curl->next;

                curl->next = new_free;
                curl->size = needed;
            }

            curl->free = false;
            return curl;
        }

        if (curl->free && curl->next && curl->next->free) {
            curl->size += curl->next->size;
            curl->next = curl->next->next;
            continue;
        }

        curl = curl->next;
    }

    return NULL;   // out of memory
}
// tehnically we should not occupy more than needed
// allocates memory on the heap(i hope idk where the pointer above leads)
// using blocks(struct size,free,next) of memory
// i am going to trust that nobody passes size 0
void *kmalloc(unsigned long size) {
    size = ALIGN8(size);

    block* b = create_block(size, 1);

    return b ? (void *)(BLOCK_BUFFER(b)) : NULL;
}
void* realloc(void* p, size_t size) {
    if (!p) return NULL;
    block* b = (block*)BLOCK_BUFFER_METADATA(p);

    b->free = true;
    char* new = kmalloc(size);

    memcpy(new, p, size > b->size ? b->size : size);

    return b;
}
void *kmalloc_align(unsigned long size, size_t alignment) {
    size = ALIGN8(size);
    // if no block exists that is free increase size
    block* b = create_block(size, alignment);

    // i have a free var in a block and
    return b ? (void *)(BLOCK_BUFFER(b)) : NULL;
}

void dump_heap() {
    block* curl = heap_ptr;
    while (curl) {
        #ifdef DEBUG
            printf("  Block at %p (Buffer at %p) with size = %d bytes, free = %d, next = %x\n", curl, ((uint64_t)curl + sizeof(block)), curl->size, curl->free, curl->next);
        #else
            printf("  Block at %p with size = %d bytes, free = %d, next = %p\n", curl, curl->size, curl->free, curl->next);
        #endif

        curl = curl->next;
    }
    // printf("Free memory: %dMb\n", free_list_head->size / 1048576);
}

// frees the block allocated at ptr by seeting the free = 1
void kfree(void *ptr) {
    if (!ptr) return;

    block *b = (block*)(BLOCK_BUFFER_METADATA(ptr));

    b->free = true;
}