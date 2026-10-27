#include "drivers/hid/keyboard.h"
#include "drivers/hid/hid.h"
#include "drivers/input.h"
#include "drivers/ps2keyboard.h"
#include "drivers/usb.h"
#include "layouts/kb_layouts.h"
#include "mem.h"
#include "sys/errno.h"
#include <drivers/interfaces/uhci.h>
#include "sys/types.h"
#include <terminal/printf.h>
#include <drivers/pit.h>

// This is only the boot protocol, report protocol will be for later

struct KeyboardReport* hid_kbd_init(struct usb_dev* usb) {
    int stat;
    printf("HERE\n");
    if ((stat = hid_dev_init(usb)) != 0)
        return (void*)(size_t)stat;

    if (!usb->data.user_data) {
        usb->data.user_data = kmalloc(sizeof(struct KeyboardReport));
        if (!usb->data.user_data) return (void*)ENOMEM;
        // TODO: Add a checker here
        memset(usb->data.user_data, 0, sizeof(struct KeyboardReport));
    }

    printf("pipe: %p\n", usb->data.pipe_in);
    usb->set_interrupt_tranfer(usb, usb->data.pipe_in, usb->data.user_data, sizeof(struct KeyboardReport));
    usb->handler = hid_kbd_report_manage;

    actual_input = 1; // set to hid output

    return usb->data.user_data;
}

volatile bool hid_kbs_ready[16];

void hid_kbd_report_manage(struct usb_dev* device) {
    struct KeyboardReport* report = device->data.user_data;

    KEYSTATE.CtrlL = report->modifier_key_status & (1 << 0);
    KEYSTATE.ShiftL = report->modifier_key_status & (1 << 1);
    KEYSTATE.AltL = report->modifier_key_status & (1 << 2);
    KEYSTATE.CtrlR = report->modifier_key_status & (1 << 4);
    KEYSTATE.ShiftR = report->modifier_key_status & (1 << 5);
    KEYSTATE.AltR = report->modifier_key_status & (1 << 6);

    for (int i = 0; i < sizeof(report->keypresses); i++) {
        if (!report->keypresses[i]) continue;
        else {
            #ifdef DEBUG
                if (report->keypresses[i] <= 0x4) {
                    switch (report->keypresses[i]) {
                        case 0x1: printf("\nhid_kbd: phantom scancode\n");
                        case 0x2: printf("\nhid_kbd: self-test failed\n");
                        case 0x3: printf("\nhid_kbd: undefined error\n");
                    }
                }
            #endif

            set_layout(HID_LAYOUTS[0]);
            actual_input = 1; // usb keyboard
            hid_kbs_ready[device->index] = 1;
            last_scancode = report->keypresses[i];
            break;
        }
    }
}
scancode_t hid_wfi() {
    while (1) {
        for (int i = 0; i < 16; i++) {
            if (hid_kbs_ready[i]) {
                hid_kbs_ready[i] = 0;
                return last_scancode;
            }

            pit_timer_wait_ms(10);
        }
    }
    return 0;
}
