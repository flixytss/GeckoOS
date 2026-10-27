#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "sys/types.h"

void fb_init(u64 addr, u32 width, u32 height, u32 pitch, u8 bpp);
void fb_draw_pixel(u32 x, u32 y, u32 color);
void fb_draw_char(u32 x, u32 y, char c, u32 fg, u32 bg);
void fb_scroll(void);
void fb_clear(u32 color);

extern u64 fb_base_addr;
extern u32 fb_width;
extern u32 fb_height;
extern u32 fb_pitch;
extern u8  fb_bpp;

extern u64 g_fb_addr;
extern u32 g_fb_width;
extern u32 g_fb_height;
extern u32 g_fb_pitch;
extern u8  g_fb_bpp;

#endif
