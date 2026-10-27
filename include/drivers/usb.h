#pragma once

#include "drivers/pci.h"
#include "sys/types.h"
#include <stdbool.h>

#define UHCICONTROLLER 1
#define OHCICONTROLLER 2
#define EHCICONTROLLER 3
#define xHCICONTROLLER 4

// Standards requests
#define REQUEST_GET_STATUS 0
#define REQUEST_CLEAR_FEATURE 1
#define REQUEST_SET_FEATURE 3
#define REQUEST_SET_ADDRESS 5
#define REQUEST_GET_DESCRIPTOR 6
#define REQUEST_SET_DESCRIPTOR 7
#define REQUEST_GET_CONFIGURATION 8
#define REQUEST_SET_CONFIGURATION 9
#define REQUEST_GET_INTERFACE 10
#define REQUEST_SET_INTERFACE 11
#define REQUEST_SYNC_FRAME 12

// HID requests
#define REQUEST_SET_PROTOCOL 0x0B
#define REQUEST_GET_REPORT 1
// HID request types
#define REQUEST_TYPE_SET_PROTOCOL 0x21
#define REQUEST_TYPE_GET_REPORT 0xA1

// Standards descriptors
#define DESCRIPTOR_TYPE_DEVICE 1
#define DESCRIPTOR_TYPE_CONFIGURATION 2
#define DESCRIPTOR_TYPE_STRING 3
#define DESCRIPTOR_TYPE_INTERFACE 4
#define DESCRIPTOR_TYPE_ENDPOINT 5
#define DESCRIPTOR_TYPE_DEVICE_QUALIFIER 6
#define DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION 7
#define DESCRIPTOR_TYPE_INTERFACE_POWER 8

// Descriptors
struct usb_setup_packet {
    u8 requesttype;
    u8 request;
    u16 value;
    u16 index;
    u16 lenght;
} __attribute__((packed));

struct usb_descriptor_head {
    u8 bLength; // Size of this descriptor in bytes
    u8 bDescriptortype; // Type of this descriptor
} __attribute__((packed));

// Device descriptor
struct usb_device_descriptor {
    struct usb_descriptor_head header;
    u16 bcdUSB;
    u8 bDeviceclass;
    u8 bSubdeviceclass;
    u8 bDeviceprotocol;
    u8 bMaxpacketsize;
    u16 idVendor;
    u16 idProduct;
    u16 bcdDevice;
    u8 iManufacter;
    u8 iProduct;
    u8 iSerialnumber;
    u8 bNumConfigurations;
} __attribute__((packed));

// String descriptor struct
struct usb_string_descriptor {
    struct usb_descriptor_head header;
    u16 string[0xFF];
} __attribute__((packed));

// Interface descriptor struct
struct usb_interface_descriptor {
    struct usb_descriptor_head header;
    u8 bInterfaceNumber;
    u8 bAlternateSetting;
    u8 bNumEndpoints;
    u8 bInterfaceClass;
    u8 bInterfaceSubClass;
    u8 bInterfaceProtocol;
    u8 iInterface;
} __attribute__((packed));

// Configuration descriptor struct
struct usb_configuration_descriptor {
    struct usb_descriptor_head header;
    u16 wTotalLength;
    u8 bNumInterfaces;
    u8 bConfigurationValue;
    u8 iConfiguration;
    u8 bmAttributes;
    u8 bMaxPower;
} __attribute__((packed));

// Endpoint descriptor struct
struct usb_endpoint_descriptor {
    struct usb_descriptor_head header;
    u8 bEndpointAddress;
    u8 bmAttributes;
    u16 wMaxPacketSize;
    u8 bInterval;
} __attribute__((packed));

// Usb system
struct usb_pipe {
    struct usb_dev* device;
    struct usb_endpoint_descriptor descriptor;
};

struct usb_header {
    struct usb_device_descriptor device_descriptor;

    // Arrays
    struct usb_configuration_descriptor config_descriptor[8];
    u8 config_count;

    struct usb_interface_descriptor interface[16];
    u8 interfaces_count;

    struct usb_pipe* pipes[10];
    struct usb_pipe* pipe_in;
    struct usb_pipe* pipe_out;
    u8 pipes_count;

    struct dev_host_head* controller;
    void* controller_reserved; // Used by the controller
    void* user_data; // Used by HID Devices and its data

    int state;
};

#define INTERRUPT_TRANSFER_INTERVAL_1MS 0
#define INTERRUPT_TRANSFER_INTERVAL_2MS 2
#define INTERRUPT_TRANSFER_INTERVAL_4MS 4
#define INTERRUPT_TRANSFER_INTERVAL_8MS 6
#define INTERRUPT_TRANSFER_INTERVAL_16MS 8
#define INTERRUPT_TRANSFER_INTERVAL_32MS 10

#define INTERRUPT_TRANFERS_DEFAULT_INTERVAL INTERRUPT_TRANSFER_INTERVAL_8MS
#define CONTROL_TRANFERS_DEFAULT_INTERVAL   INTERRUPT_TRANSFER_INTERVAL_16MS

struct usb_dev {
    struct usb_header data;

    u8 hid_protocol; // set to 0 if the device isn't hid
    u8 type;
    char interrupt_tranfers_enabled[10];
    u8 index;
    bool connected : 1;

    bool (*is_connected)(struct usb_dev* device);
    void (*init_framelist)(struct usb_dev* device);

    int  (*set_control_tranfer)(struct usb_dev* device, struct usb_pipe* pipe, struct usb_setup_packet setup, void* data, bool no_data);
    int  (*set_interrupt_tranfer)(struct usb_dev* device, struct usb_pipe* pipe, void* data, u16 size); // Only support IN packets

    void (*handler)(struct usb_dev* device);
};

struct dev_host_head {
    struct pci_dev* pci_dev;

    int  (*suspend)(struct usb_dev* device);
};

extern struct usb_dev usb_devices[16];
extern u8 usb_devices_count;

static const char* usb_types_table[] = {
    "No type",
    "UHCI",
    "OHCI",
    "EHCI",
    "xHCI",
    "PS2" // hufdehiugdfiuh
};

// usb pipe macros
#define PIPE_IS_READ(x) (x)->descriptor.bEndpointAddress & (1 << 7)
#define PIPE_IS_WRITE(x) !(PIPE_IS_READ(x))

int usb_is_hid(struct usb_dev* device);
int usb_get_string(struct usb_dev* device, struct usb_string_descriptor* string, u8 index, u16 langid);
int usb_get_configuration(struct usb_dev* device, u8 configuration_index);
int usb_get_interface(struct usb_dev* device, u8 interface_index);
int usb_set_address(struct usb_dev* dev, u8 address);
