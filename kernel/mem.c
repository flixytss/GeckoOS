#include "sys/limits.h"
#include "terminal/printf.h"
#include <mem.h>
#include <drivers/vga.h>
#include <gk/gk.h>
#include <stdbool.h>
#include <stddef.h>
#include "sys/types.h"
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
u8 strcmp(const char *s, const char *d) {
    while (*s == *d && *s)
        s++, d++;
    return *(u8 *)s - *(u8 *)d;
}
u8 strcasecmp(const char* a, const char* b) {
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

u8 dlim(char c, const char *delim) {
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

u8 strncmp(const char *s1, const char *s2, size_t siz) {
    while (siz && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        siz--;
    }

    if (siz == 0)
        return 0;

    return *(u8 *)s1 - *(u8 *)s2;
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
    u64 s = strlen(str);
    char* ret = kmalloc(s);
    memcpy(ret, str, s);
    ret[s] = 0;
    return ret;
}

// replace with real allocator later but should be fine for now
// kotofyt: it is not
// pumpkicks: is this enough?

static void *heap_ptr = NULL;
static void *heap_end = NULL;
static block *free_list_head = NULL;

#define BLOCK_BUFFER(x) ((u64)x + sizeof(block))
#define BLOCK_BUFFER_METADATA(x) ((u64)x - sizeof(block))

void kalloc_init(u64 start, u64 size) {
    heap_ptr = (void*)start;
    heap_end = heap_ptr + size;

    free_list_head = (block*)heap_ptr;
    free_list_head->free = true; // CRASH
    free_list_head->size = size;
    free_list_head->next = NULL;
}

// Divide the free_list_head into smaller blocks with the wanted size
static block *create_block(size_t size, size_t alignment) {
    if (!heap_ptr) {
        printf("You should initialize the heap\n");
        return NULL;
    }

    if (size == 0 || size > ULONG_LONG_MAX - sizeof(block) - alignment)
        return NULL;

    size = ALIGN(size + sizeof(block), alignment);

    block *curl = free_list_head;

    // printf("kmalloc\n");

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
            }

            curl->free = false;
            return curl;
        }

#if 1
        if (curl->free && curl->next && curl->next->free) {
            curl->size += curl->next->size;
            curl->next = curl->next->next;
            continue;
        }
#endif

        curl = curl->next;
    }

    return NULL;   // out of memory
}
// tehnically we should not occupy more than needed
// allocates memory on the heap(i hope idk where the pointer above leads)
// using blocks(struct size,free,next) of memory
// i am going to trust that nobody passes size 0
void *kmalloc(unsigned long size) {
    block* b = create_block(size, 4);

    return b ? (void *)(BLOCK_BUFFER(b)) : NULL;
}
void* realloc(void* p, size_t size) { // DONT USE
    if (!p) return NULL;
    block* b = (block*)BLOCK_BUFFER_METADATA(p);

    b->free = true;
    char* new = kmalloc(size);

    memcpy(new, p, size > b->size ? b->size : size);

    return b;
}
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
            printf("  Block at %p (Buffer at %p) with size = %d bytes, free = %d, next = %x\n", curl, ((u64)curl + sizeof(block)), curl->size, curl->free, curl->next);
        #else
            printf("  Block at %p with size = %d bytes, free = %d, next = %p\n", curl, curl->size, curl->free, curl->next);
        #endif

        curl = curl->next;
    }
    printf("Free memory: %dMb\n", (u32)last->size / 1048576);
}

// frees the block allocated at ptr by seeting the free = 1
void kfree(void *ptr) {
    if (!ptr) return;
    // printf("kfree\n");

    block *b = (block*)(BLOCK_BUFFER_METADATA(ptr));

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
