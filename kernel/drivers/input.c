// Globalize the inputs (from ps2 or usbs)

#include "layouts/kb_layouts.h"
#include "sys/types.h"

/*
    actual_input:
    0 = from ps2 keyboard
    1 = from usb keyboard
*/

volatile scancode_t last_scancode;
volatile u8 actual_input = 0;