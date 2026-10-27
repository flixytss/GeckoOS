#ifndef _VGA_H
#define _VGA_H

#include "sys/types.h"

#define VGA_TEXT_ADDR 0xb8000
#define VGA_GRAPHICS_ADDR 0xa0000
#define VGA_TEXT_WIDTH 80
#define VGA_TEXT_HEIGHT 25

enum VGA_COLOR {
  VGA_COLOR_BLACK = 0,
  VGA_COLOR_BLUE = 1,
  VGA_COLOR_GREEN = 2,
  VGA_COLOR_CYAN = 3,
  VGA_COLOR_RED = 4,
  VGA_COLOR_MAGENTA = 5,
  VGA_COLOR_BROWN = 6,
  VGA_COLOR_LIGHT_GREY = 7,
  VGA_COLOR_DARK_GREY = 8,
  VGA_COLOR_LIGHT_BLUE = 9,
  VGA_COLOR_LIGHT_GREEN = 10,
  VGA_COLOR_LIGHT_CYAN = 11,
  VGA_COLOR_LIGHT_RED = 12,
  VGA_COLOR_LIGHT_MAGENDA = 13,
  VGA_COLOR_LIGHT_BROWN = 14,
  VGA_COLOR_WHITE = 15,
  VGA_COLOR_LIGHT_YELLOW = 0xE,
  VGA_COLOR_YELLOW = 0x2C
};

u8 vga_entry_color(enum VGA_COLOR fg, enum VGA_COLOR bg);

u16 vga_entry(unsigned char c, u8 color);

void move_tcursor(int x, int y);

void vga_clear(u8 color);

void putentryat(char c, u8 color, size_t x, size_t y);

#endif
