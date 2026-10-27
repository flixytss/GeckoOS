#pragma once

#include "drivers/hid/keyboard.h"
#include "drivers/ps2keyboard.h"
#include <terminal/printf.h>
#include "sys/types.h"

extern volatile u8 actual_input;
extern volatile u8 last_scancode;

static inline u8 get_scancode() {
    if (actual_input) return hid_wfi();
    else return ps2_kb_wfi();
}