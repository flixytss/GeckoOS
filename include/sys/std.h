#pragma once

#include "types.h"

void*  memset(void* dest, int val, unsigned long n);
void*  memcpy(void* dest, const void* src, unsigned long n);
void*  memmove(void* dest, const void* src, unsigned long n);
size_t strlen(const char* ptr);
int    strncmp(const char *s1, const char *s2, size_t siz);
int    strcmp(const char *s, const char *d);
int    strcasecmp(const char *s, const char *d);
u8     dlim(char c, const char *delim);
char*  strtok_r(char *restrict str, const char *restrict delim, char **saveptr);
char*  strtok(char *restrict str, const char *restrict delim);
char*  strncpy(char *restrict dst, const char *restrict src, size_t siz);
char*  strcpy(char *restrict dst, const char *restrict src);
char*  strchr(const char *s, int c);
char*  strrchr(const char *s, int c);
char*  strdup(const char* str);