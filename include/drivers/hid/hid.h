#pragma once

#include "drivers/usb.h"
#include "sys/types.h"

#define BOOT_PROTOCOL 0
#define REPORT_PROTOCOL 1

#define KEYBOARD_DEFAULT_INTERVAL INTERRUPT_TRANSFER_INTERVAL_8MS

// Exclusive for HID devices
struct usb_hid_descriptor {
    struct usb_descriptor_head header;
    u16 bcdHID;
    u8 bCountryCode;
    u8 bNumDescriptors;

};

int hid_set_protocol(struct usb_dev* usb, u16 protocol);
int hid_get_report(struct usb_dev* usb, void* buffer, u16 len);
int hid_dev_init(struct usb_dev* dev);
