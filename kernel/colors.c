// Added by MorganPG1 to centralise colors
#include <colors.h>
#include <drivers/vga.h>
#include "sys/types.h"

const u8 BG_COLOR = VGA_COLOR_BLACK;
const u8 TERM_COLOR = VGA_COLOR_WHITE | BG_COLOR << 4;
const u8 PROMPT_COLOR = VGA_COLOR_LIGHT_GREEN | BG_COLOR << 4;
const u8 BOLD_COLOR = VGA_COLOR_LIGHT_GREY | BG_COLOR << 4;