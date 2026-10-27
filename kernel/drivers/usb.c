#include "mem.h"
#include "sys/errno.h"
#include <drivers/usb.h>
#include <stdbool.h>
#include <terminal/printf.h>
#include "sys/types.h"

struct usb_dev usb_devices[16];
u8 usb_devices_count;

int usb_is_hid(struct usb_dev* device) {
    if (!device) return -EINVAL;
    if ((device->data.interface[0].bInterfaceClass != 3) && (device->data.device_descriptor.bDeviceclass != 0) && (device->data.device_descriptor.bSubdeviceclass != 0)) return 0;
    return device->data.interface[0].bInterfaceSubClass;
}

int usb_get_string(struct usb_dev* device, struct usb_string_descriptor* string, u8 index, u16 langid) {
    if (!device || !string) return -EINVAL;

    device->set_control_tranfer(device, device->data.pipes[0], (struct usb_setup_packet){
        .requesttype = 0x80,
        .request = REQUEST_GET_DESCRIPTOR,
        .value = (DESCRIPTOR_TYPE_STRING << 8) | index,
        .index = langid,
        .lenght = sizeof(string->header.bLength)
    }, string, false);

    device->set_control_tranfer(device, device->data.pipes[0], (struct usb_setup_packet){
        .requesttype = 0x80,
        .request = REQUEST_GET_DESCRIPTOR,
        .value = (DESCRIPTOR_TYPE_STRING << 8) | index,
        .index = langid,
        .lenght = string->header.bLength
    }, string, false);
}

int usb_get_configuration(struct usb_dev* device, u8 configuration_index) { // Put the configuration descriptor and all of its interfaces and endpoints inside the usb device struct
    int status;
    if ((status = device->set_control_tranfer(device, device->data.pipes[0], (struct usb_setup_packet){
            .requesttype = 0x80,
            .request = REQUEST_GET_DESCRIPTOR,
            .value = DESCRIPTOR_TYPE_CONFIGURATION << 8 | configuration_index,
            .index = 0,
            .lenght = (sizeof(device->data.config_descriptor[configuration_index].header) + sizeof(device->data.config_descriptor[configuration_index].wTotalLength))
        }, &device->data.config_descriptor[configuration_index].header, false)) != 0) {
        return status;
    }

    char* buffer = kmalloc(device->data.config_descriptor[0].wTotalLength);
    if (!buffer) return -ENOMEM;

    if ((status = device->set_control_tranfer(device, device->data.pipes[0], (struct usb_setup_packet){
            .requesttype = 0x80,
            .request = REQUEST_GET_DESCRIPTOR,
            .value = (DESCRIPTOR_TYPE_CONFIGURATION << 8) | configuration_index,
            .index = 0,
            .lenght = device->data.config_descriptor[configuration_index].wTotalLength
        }, buffer, false)) != 0) {
        return status;
    }
    device->data.config_descriptor[configuration_index] = *(struct usb_configuration_descriptor*)buffer; // TODO: add a limiter for this

    for (int i = sizeof(struct usb_configuration_descriptor); i < device->data.config_descriptor[configuration_index].wTotalLength; i += ((struct usb_descriptor_head*)&buffer[i])->bLength) {
        const struct usb_descriptor_head* header = (struct usb_descriptor_head*)&buffer[i];

        switch (header->bDescriptortype) {
            case DESCRIPTOR_TYPE_INTERFACE:
                device->data.interface[device->data.interfaces_count++] = *(struct usb_interface_descriptor*)header;
                // usb_get_interface(device, ((struct usb_interface_descriptor*)header)->bInterfaceNumber);
            case DESCRIPTOR_TYPE_ENDPOINT: {
                struct usb_pipe* pipe = (device->data.pipes[device->data.pipes_count++] = kmalloc(sizeof(struct usb_pipe*)));

                pipe->device = device;
                pipe->descriptor = *(struct usb_endpoint_descriptor*)header;

                if (PIPE_IS_READ(pipe)) device->data.pipe_in = pipe;
                else if (PIPE_IS_WRITE(pipe)) device->data.pipe_in = pipe;
            }
        }
    } kfree(buffer);

    return 0;
}

int usb_get_interface(struct usb_dev* device, u8 interface_index) {
    int stat;

    struct usb_descriptor_head buffer;
    if (device->set_control_tranfer(device, device->data.pipes[0], (struct usb_setup_packet){
        .requesttype = 0x80,
        .request = REQUEST_GET_DESCRIPTOR,
        .value = 0,
        .index = interface_index,
        .lenght = sizeof(struct usb_descriptor_head)
    }, &buffer, false) != 0) {
        printf("err");
    }

    printf("hid; %x\n", buffer.bDescriptortype);

    return 0;
}

int usb_pipe_talk(struct usb_pipe* pipe, struct usb_setup_packet packet) {
    if (!pipe) return -EINVAL;


}

int usb_set_address(struct usb_dev* dev, u8 address) {
    if (!dev || !address) return -EINVAL;

    return dev->set_control_tranfer(dev, dev->data.pipes[0], (struct usb_setup_packet){
        .requesttype = 0x00,
        .request = REQUEST_SET_ADDRESS,
        .value = address,
        .index = 0x00,
        .lenght = 0x00
    }, NULL, true);
}
