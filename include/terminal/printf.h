#ifndef PRINTF_H
#define PRINTF_H

#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS   1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS       1
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS       1
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS       1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS      1
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS   0

#include "terminal/npf.h"

int printf(const char *fmt, ...);
int snprintf(char *buf, size_t siz, const char *fmt, ...);
void set_printf_color(u8 color);

#define pretty_printf(...) {printc((char*)__FUNCTION__, VGA_COLOR_WHITE); printc((char*)": ", VGA_COLOR_WHITE); printf(__VA_ARGS__);}

#endif