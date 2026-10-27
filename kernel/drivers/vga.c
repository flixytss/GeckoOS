/*
 * VGA text mode driver — superseded by the framebuffer driver.
 * Kept for fallback and for vga_entry/vga_entry_color helpers
 * still used by the terminal history buffer.
 * Functions that write to 0xB8000 or VGA hardware ports are stubbed out
 * or forwarded to the framebuffer driver.
 */

#include <drivers/framebuffer.h>
#include <drivers/vga.h>
#include <fonts/font8x16.h>
#include <ports.h>

u8 TERMINAL_COLOR;

static const u32 vga_to_rgb[16] = {
    0x00000000, 0x000000AA, 0x0000AA00, 0x0000AAAA,
    0x00AA0000, 0x00AA00AA, 0x00AA5500, 0x00AAAAAA,
    0x00555555, 0x005555FF, 0x0055FF55, 0x0055FFFF,
    0x00FF5555, 0x00FF55FF, 0x00FFFF55, 0x00FFFFFF,
};

extern u32 g_fb_width;
extern u32 g_fb_height;
extern u32 g_fb_pitch;
extern u64 g_fb_addr;

static void fb_putchar_raw(char c, u32 x, u32 y, u32 fg, u32 bg)
{
    unsigned char uc = (unsigned char)c;
    const unsigned char *glyph = &font8x16[uc][0];
    for (u32 row = 0; row < 16; row++) {
        unsigned char line = glyph[row];
        for (u32 col = 0; col < 8; col++) {
            u32 px = x + col;
            u32 py = y + row;
            if (px >= g_fb_width || py >= g_fb_height) {
                continue;
            }
            u32 color = (line & (0x80 >> col)) ? fg : bg;
            volatile u32 *pixel = (volatile u32 *)(g_fb_addr + (u64)py * g_fb_pitch);
            pixel[px] = color;
        }
    }
}

/* Forward to framebuffer character drawing. */
void putentryat(char c, u8 color, size_t x, size_t y) {
    u32 fg = vga_to_rgb[color & 0xF];
    u32 bg = vga_to_rgb[(color >> 4) & 0xF];
    fb_putchar_raw(c, x * 8, y * 16, fg, bg);
}

u8 vga_entry_color(enum VGA_COLOR fg, enum VGA_COLOR bg) {
	return fg | bg << 4;
}

u16 vga_entry(unsigned char c, u8 color) {
	return (u16) c | (u16) color << 8;
}

/* Forward to framebuffer clear using the background color from the VGA byte. */
void vga_clear(u8 color) {
	u32 bg = vga_to_rgb[(color >> 4) & 0xF];
	fb_clear(bg);
}

/* Stubbed — hardware VGA cursor is no longer used. */
void move_tcursor(int x, int y) {
	(void)x;
	(void)y;
}

void set_termcolor(enum VGA_COLOR FG, enum VGA_COLOR BG) {
	TERMINAL_COLOR = vga_entry_color(FG, BG);
}
