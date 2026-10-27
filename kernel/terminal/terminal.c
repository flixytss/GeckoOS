#include "drivers/vga.h"
#include <drivers/framebuffer.h>
#include <drivers/ps2keyboard.h>
#include <drivers/ps2mouse.h>
#include <gk/gk.h>
#include <mem.h>
#include "sys/types.h"
#include <terminal/terminal.h>
#include <drivers/input.h>

extern u64 g_fb_addr;
extern u32 g_fb_width;
extern u32 g_fb_height;
extern u32 g_fb_pitch;
extern u8  g_fb_bpp;

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

static u32 cursor_x = 0;

static u32 fb_cols;
static u32 fb_rows;

static void fb_update_dims(void)
{
    fb_cols = g_fb_width / FONT_WIDTH;
    fb_rows = g_fb_height / FONT_HEIGHT;
}

static const u32 vga_to_rgb[16] = {
    0x00000000,
    0x000000AA,
    0x0000AA00,
    0x0000AAAA,
    0x00AA0000,
    0x00AA00AA,
    0x00AA5500,
    0x00AAAAAA,
    0x00555555,
    0x005555FF,
    0x0055FF55,
    0x0055FFFF,
    0x00FF5555,
    0x00FF55FF,
    0x00FFFF55,
    0x00FFFFFF,
};

static u32 vga_fg_to_rgb(u8 color)
{
    return vga_to_rgb[color & 0xF];
}

static u32 vga_bg_to_rgb(u8 color)
{
    return vga_to_rgb[(color >> 4) & 0xF];
}

static void fb_draw_cursor_block(u32 cx, u32 cy, u32 bg)
{
    for (u32 row = 0; row < FONT_HEIGHT; row++) {
        for (u32 col = 0; col < FONT_WIDTH; col++) {
            u32 px = cx * FONT_WIDTH + col;
            u32 py = cy * FONT_HEIGHT + row;
            if (px >= g_fb_width || py >= g_fb_height) {
                continue;
            }
            u32 color = (row >= FONT_HEIGHT - 2) ? 0x00FFFFFF : bg;
            volatile u32 *pixel = (volatile u32 *)(g_fb_addr + (u64)py * g_fb_pitch);
            pixel[px] = color;
        }
    }
}

u16 terminal_column = 0;
u16 terminal_row    = 0;
static int history_count = 0;
static int history_head  = 0;

static bool follow_output = true;

void putchar(char c, u8 color)
{
    if (fb_cols == 0) {
        fb_update_dims();
    }

    u32 fg = vga_fg_to_rgb(color);
    u32 bg = vga_bg_to_rgb(color);

    if (c == 0) {
        return;
    }

    if (c == '\n') {
        /* Blank the rest of this history row so render_viewport is clean */
        for (u32 k = cursor_x; k < fb_cols && k < VGA_TEXT_WIDTH; k++) {
            console_history[total_lines][k] = vga_entry(' ', color);
        }
        cursor_x = 0;
        total_lines++;
        if (total_lines >= SCROLLBACK_LINES)
            total_lines = SCROLLBACK_LINES - 1;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (total_lines > 0) {
            total_lines--;
            cursor_x = (fb_cols < VGA_TEXT_WIDTH ? fb_cols : VGA_TEXT_WIDTH) - 1;
        }
        console_history[total_lines][cursor_x] = vga_entry(' ', color);
    } else if (c == '\t') {
        for (int j = 0; j < 4; j++) {
            putchar(' ', color);
        }
        return;
    } else {
        if (cursor_x < VGA_TEXT_WIDTH) {
            console_history[total_lines][cursor_x] = vga_entry(c, color);
        }
        cursor_x++;
    }

    /* Word-wrap */
    if (cursor_x >= fb_cols) {
        cursor_x = 0;
        total_lines++;
        if (total_lines >= SCROLLBACK_LINES)
            total_lines = SCROLLBACK_LINES - 1;
    }

    /* Compute the screen row this character belongs to */
    int screen_row = (int)total_lines - viewport_top;

    /* Update viewport to follow output; full redraw only when it scrolls */
    if (follow_output) {
        int new_top = (int)total_lines - (int)fb_rows + 1;
        if (new_top < 0) new_top = 0;
        if (new_top != viewport_top) {
            viewport_top = new_top;
            screen_row   = (int)total_lines - viewport_top;
            render_viewport();
            terminal_row    = (u16)(total_lines - viewport_top);
            terminal_column = cursor_x;
            return;
        }
    }

    /* Fast path: draw only the character that just changed */
    if (screen_row >= 0 && screen_row < (int)fb_rows) {
        if (c == '\b') {
            fb_draw_char(cursor_x * FONT_WIDTH,
                         (u32)screen_row * FONT_HEIGHT,
                         ' ', fg, bg);
        } else if (c != '\n') {
            u32 draw_col = cursor_x > 0 ? cursor_x - 1 : 0;
            fb_draw_char(draw_col * FONT_WIDTH,
                         (u32)screen_row * FONT_HEIGHT,
                         c, fg, bg);
        }
    }

    terminal_row    = (u16)(total_lines - viewport_top);
    terminal_column = cursor_x;
}

void write(char *data, size_t size, u8 COLOR)
{
    for (int i = 0; i < size; i++) {
        putchar(data[i], COLOR);
    }
}

void printc(char *data, u8 COLOR)
{
    for (size_t i = 0; data[i]; i++) {
        putchar(data[i], COLOR);
    }
}

void print(char *data)
{
    for (size_t i = 0; data[i]; i++) {
        putchar(data[i], VGA_COLOR_WHITE);
    }
}

void print_int(int n)
{
    char buff[32];
    int_to_str(n, buff);
    print(buff);
}

void render_viewport()
{
    for (size_t y = 0; y < fb_rows && (size_t)(viewport_top + y) < SCROLLBACK_LINES; y++) {
        for (size_t x = 0; x < fb_cols; x++) {
            u16 entry = console_history[viewport_top + y][x];
            char c = (char)(entry & 0xFF);
            u8 color = (u8)(entry >> 8);
            u32 fg = vga_fg_to_rgb(color);
            u32 bg = vga_bg_to_rgb(color);
            fb_draw_char(x * FONT_WIDTH, y * FONT_HEIGHT, c, fg, bg);
        }
    }
}

void scroll_up()
{
    if (viewport_top >= total_lines - (int)fb_rows + 1) {
        follow_output = false;
    }
    if (viewport_top == 0)
        return;
    if (viewport_top >= 0)
        viewport_top--;
}

void scroll_down()
{
    if (viewport_top >= total_lines - (int)fb_rows + 1) {
        follow_output = true;
    }
    if (viewport_top + (int)fb_rows <= total_lines)
        viewport_top++;
}

void terminal_clear(u8 color)
{
    u32 bg = vga_bg_to_rgb(color);
    fb_clear(bg);

    u16 blank = vga_entry(' ', color);
    for (size_t y = 0; y < SCROLLBACK_LINES; y++) {
        for (size_t x = 0; x < VGA_TEXT_WIDTH; x++) {
            console_history[y][x] = blank;
        }
    }
    total_lines     = 0;
    cursor_x        = 0;
    terminal_column = 0;
    terminal_row    = 0;
    viewport_top    = 0;
}

static void history_push(unsigned char *buf)
{
    if (buf[0] == '\0')
        return;
    size_t i;
    for (i = 0; buf[i] && i < 511; i++) {
        history_entries[history_head][i] = buf[i];
    }
    history_entries[history_head][i] = '\0';
    history_head                     = (history_head + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE)
        history_count++;
}

void input(unsigned char *buff, size_t buffer_size, u8 color)
{
    size_t buff_count  = 0;
    size_t start_x     = cursor_x;
    size_t start_line  = total_lines;  /* absolute history line at prompt */

    int browse_idx = 0;
    unsigned char saved_input[512];

    while (true) {
        scancode_t sc = get_scancode();

        if (sc & 0x80)
            continue;

        if (sc == KEY_UP || sc == KEY_DOWN) {
            if (sc == KEY_UP && browse_idx == 0) {
                size_t k;
                for (k = 0; k < buff_count; k++) {
                    saved_input[k] = buff[k];
                }
                saved_input[k] = '\0';
            }

            if (sc == KEY_UP && browse_idx < history_count)
                browse_idx++;
            if (sc == KEY_DOWN && browse_idx > 0)
                browse_idx--;

            unsigned char *src;
            if (browse_idx == 0) {
                src = saved_input;
            } else {
                int slot =
                    (history_head - browse_idx + HISTORY_SIZE) % HISTORY_SIZE;
                src = history_entries[slot];
            }

            /* Erase current input from history buffer, reset cursor */
            for (size_t k = 0; k < buff_count; k++) {
                size_t abs_col   = start_x + k;
                size_t hist_line = start_line + abs_col / fb_cols;
                size_t hist_col  = abs_col % fb_cols;
                if (hist_col < VGA_TEXT_WIDTH)
                    console_history[hist_line][hist_col] = vga_entry(' ', color);
            }
            cursor_x    = start_x;
            total_lines = start_line;
            render_viewport();

            size_t k;
            for (k = 0; src[k] && k < buffer_size - 1; k++) {
                buff[k] = src[k];
                putchar(src[k], color);
            }

            buff_count       = k;
            buff[buff_count] = '\0';
            continue;
        }

        unsigned char ascii = scancode_to_ascii(last_scancode);

        if (ascii == '\n')
            break;

        if (ascii == '\b') {
            if (buff_count > 0) {
                putchar('\b', color);
                buff_count--;
                buff[buff_count] = 0;
            }
            continue;
        }

        if (buff_count < buffer_size - 1 && ascii >= 0x20) {
            buff[buff_count] = ascii;
            putchar(ascii, color);
            buff_count++;
        }
    }

    buff[buff_count] = '\0';
    history_push(buff);
}

void terminal_init()
{
    fb_cols = g_fb_width / FONT_WIDTH;
    fb_rows = g_fb_height / FONT_HEIGHT;
    register_mouse_callback(get_mouse_event);
}

void get_mouse_event(mouse_event_data_t md)
{
    int scroll = md.scroll == 0 ? 0 : md.scroll - 8;
    if (scroll) {
        if (scroll < 0) {
            scroll_down();
        }
        if (scroll > 0) {
            scroll_up();
        }
    }
    render_viewport();
    draw_cursor(md.x, md.y, 1);
}

void draw_cursor(int x, int y, int visible)
{
    if (visible) {
        if (cursor.is_drawn) {
            u32 old_cx = cursor.x;
            u32 old_cy = cursor.y;
            u16 old_entry = console_history[viewport_top + old_cy][old_cx];
            char old_c = (char)(old_entry & 0xFF);
            u8 old_color = (u8)(old_entry >> 8);
            u32 fg = vga_fg_to_rgb(old_color);
            u32 bg = vga_bg_to_rgb(old_color);
            fb_draw_char(old_cx * FONT_WIDTH, old_cy * FONT_HEIGHT, old_c, fg, bg);
            cursor.is_drawn = 0;
        }

        u32 cx = x;
        u32 cy = y;
        u16 entry = console_history[viewport_top + cy][cx];
        u8 old_color = (u8)(entry >> 8);
        u32 bg = vga_bg_to_rgb(old_color);

        fb_draw_cursor_block(cx, cy, bg);

        cursor.x = cx;
        cursor.y = cy;
        cursor.is_drawn = 1;
    } else {
        if (cursor.is_drawn) {
            u32 cx = cursor.x;
            u32 cy = cursor.y;
            u16 entry = console_history[viewport_top + cy][cx];
            char c = (char)(entry & 0xFF);
            u8 color = (u8)(entry >> 8);
            u32 fg = vga_fg_to_rgb(color);
            u32 bg = vga_bg_to_rgb(color);
            fb_draw_char(cx * FONT_WIDTH, cy * FONT_HEIGHT, c, fg, bg);
            cursor.is_drawn = 0;
        }
    }
}

static int console_history_tail = 0;

void add_to_history(void)
{
    if (console_history_tail >= SCROLLBACK_LINES) {
        console_history_tail = 0;
    }

    for (size_t x = 0; x < fb_cols && x < VGA_TEXT_WIDTH; x++) {
        u32 px = x * FONT_WIDTH;
        u32 py = 0;
        unsigned char glyph_line = 0;

        u16 entry = 0;
        for (u32 col = 0; col < FONT_WIDTH; col++) {
            if (px + col >= g_fb_width || py >= g_fb_height) {
                continue;
            }
            volatile u32 *pixel = (volatile u32 *)(g_fb_addr + (u64)py * g_fb_pitch);
            if (pixel[px + col] != 0) {
                glyph_line |= (0x80 >> col);
            }
        }
        // (void)glyph_line;
        console_history[console_history_tail][x] = entry;
    }

    console_history_tail++;
}