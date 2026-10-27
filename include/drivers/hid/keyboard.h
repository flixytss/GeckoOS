#pragma once

#include <stdbool.h>
#include "drivers/usb.h"
#include "layouts/kb_layouts.h"

struct KeyboardReport {
    u8 modifier_key_status;
    u8 reserved;
    u8 keypresses[6];
} __attribute__((packed));

struct KeyboardReport* hid_kbd_init(struct usb_dev* usb);
void hid_kbd_report_manage(struct usb_dev*);
scancode_t hid_wfi();