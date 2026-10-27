#define NANOPRINTF_IMPLEMENTATION
#include "drivers/vga.h"
#include "sys/types.h"
#include "terminal/printf.h"
#include "terminal/terminal.h"
#include "stdarg.h"


u8 printf_color = VGA_COLOR_WHITE;
static void wrapper(int c, void *ctx) {
  (void)ctx;
  putchar(c, printf_color);
}

void set_printf_color(u8 color) { printf_color = color; }

int printf(const char *fmt, ...) {
  va_list a;
  va_start(a, fmt);
  int r = npf_vpprintf(wrapper, NULL, fmt, a);
  va_end(a);
  return r;
}

int snprintf(char *restrict buf, size_t siz, const char *restrict fmt, ...) {
  va_list a;
  va_start(a, fmt);
  int r = npf_vsnprintf(buf, siz, fmt, a);
  va_end(a);
  return r;
}