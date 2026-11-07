#include "mem/mmu.h"
#include "sys/types.h"

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

void *memset(void *dest, int val, unsigned long n) {
    asm volatile("rep stosb" : : "D"(dest), "c"(n), "a"(val));
    return dest;
}

size_t strlen(const char *ptr) {
    int i = 0;
    while (ptr[i]) i++;
    return i;
}
int strcmp(const char *s, const char *d) {
    while (*s == *d && *s)
        s++, d++;
    return *(u8 *)s - *(u8 *)d;
}
int strcasecmp(const char* a, const char* b) {
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

int strncmp(const char *s1, const char *s2, size_t siz) {
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
 

char* strncpy(char *restrict dst, const char *restrict src, size_t siz) {
    while (siz--) *dst++ = *src++;
    return dst;
}

char* strcpy(char *restrict dst, const char *restrict src) {
    while ((*dst++ = *src++));
    return dst;
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

