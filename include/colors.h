// Added by MorganPG1 to centralise colors
#ifndef COLORS_H
#define COLORS_H

#include <drivers/vga.h>
#include "sys/types.h"

static const u8 BG_COLOR = VGA_COLOR_BLACK;
static const u8 TERM_COLOR = VGA_COLOR_WHITE | BG_COLOR << 4;
static const u8 PROMPT_COLOR = VGA_COLOR_LIGHT_GREEN | BG_COLOR << 4;
static const u8 BOLD_COLOR = VGA_COLOR_LIGHT_GREY | BG_COLOR << 4;

#endif
