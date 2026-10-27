#include <drivers/framebuffer.h>
#include <fonts/font8x16.h>
#include <mem.h>
#include "sys/types.h"

u64 fb_base_addr;
u32 fb_width;
u32 fb_height;
u32 fb_pitch;
u8  fb_bpp;

void fb_init(u64 addr, u32 width, u32 height, u32 pitch, u8 bpp)
{
    fb_base_addr = addr;
    fb_width     = width;
    fb_height    = height;
    fb_pitch     = pitch;
    fb_bpp       = bpp;
}

void fb_draw_pixel(u32 x, u32 y, u32 color)
{
    if (x >= fb_width || y >= fb_height) {
        return;
    }
    volatile u32 *pixel = (volatile u32 *)(fb_base_addr + (u64)y * fb_pitch);
    pixel[x] = color;
}

void fb_draw_char(u32 x, u32 y, char c, u32 fg, u32 bg)
{
    unsigned char uc = (unsigned char)c;
    const unsigned char *glyph = &font8x16[uc][0];

    for (u32 row = 0; row < 16; row++) {
        unsigned char line = glyph[row];
        for (u32 col = 0; col < 8; col++) {
            u32 px = x + col;
            u32 py = y + row;
            if (px >= fb_width || py >= fb_height) {
                continue;
            }
            u32 color = (line & (0x80 >> col)) ? fg : bg;
            volatile u32 *pixel = (volatile u32 *)(fb_base_addr + (u64)py * fb_pitch);
            pixel[px] = color;
        }
    }
}

void fb_scroll(void)
{
    u64 bytes_per_row = fb_pitch;
    u64 scroll_bytes  = (u64)16 * bytes_per_row;
    u64 total_bytes   = (u64)fb_height * bytes_per_row;

    memmove((void *)fb_base_addr,
            (void *)(fb_base_addr + scroll_bytes),
            total_bytes - scroll_bytes);

    u64 clear_start = total_bytes - scroll_bytes;
    memset((void *)(fb_base_addr + clear_start), 0, scroll_bytes);
}

void fb_clear(u32 color)
{
    u64 total_pixels = (u64)fb_height * (fb_pitch / 4);
    volatile u32 *buf = (volatile u32 *)fb_base_addr;
    for (u64 i = 0; i < total_pixels; i++) {
        buf[i] = color;
    }
}
