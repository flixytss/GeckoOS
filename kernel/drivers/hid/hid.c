#include "drivers/hid/hid.h"
#include "sys/errno.h"
#include "terminal/printf.h"
#include "sys/types.h"

int hid_set_protocol(struct usb_dev* usb, u16 protocol) {
    // if (protocol == REPORT_PROTOCOL) return; // Only boot protocol
    return usb->set_control_tranfer(usb, usb->data.pipes[0], (struct usb_setup_packet){
            .requesttype = REQUEST_TYPE_SET_PROTOCOL,
            .request = REQUEST_SET_PROTOCOL,
            .value = protocol,
            .index = 0,
            .lenght = 0
        }, NULL, true);
}
int hid_get_report(struct usb_dev* usb, void* buffer, u16 len) {
    return usb->set_control_tranfer(usb, usb->data.pipes[0], (struct usb_setup_packet){
            .requesttype = REQUEST_TYPE_GET_REPORT,
            .request = REQUEST_GET_REPORT,
            .value = 0x0100,
            .index = 0,
            .lenght = len
        }, buffer, false);
}
int hid_dev_init(struct usb_dev* dev) {
    if (!dev) return -EINVAL;

    return hid_set_protocol(dev, dev->hid_protocol == 1 ? BOOT_PROTOCOL : REPORT_PROTOCOL);
}
