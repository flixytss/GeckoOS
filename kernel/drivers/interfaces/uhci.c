/*
    All the professional repos like the linux always have a comment at the start
    of the file with like: "GPL3 licence 2010 - 2012" or something like that so:

    License Identifier: GPL-3.0

    (C) Copyright 2026 Pumpkicks
*/

//going to hate my life here we go

// damn like fr

#include <drivers/interfaces/uhci.h>
#include "drivers/hid/keyboard.h"
#include "drivers/pci.h"
#include "drivers/pit.h"
#include "arch/x86_64/irq.h"
#include "arch/x86_64/isr.h"
#include "drivers/usb/host/uhci-hcd.h"
#include "drivers/usb.h"
#include "gk/gk.h"
#include "mem.h"
#include "ports.h"
#include <stdbool.h>
#include <stddef.h>
#include <terminal/printf.h>
#include "drivers/vga.h"
#include "sys/errno.h"
#include <mem/paging.h>

// TODO: Remake all of this code, i think that a uhci dosen't create usb devices inside of its code

#define UHCI_MAX_CHILDRENS 7

#define uhci_readw(controller, reg) inw((controller)->ioport + reg)
// #define uhci_readl(controller, reg) inl(controller.ioport + reg)
#define uhci_writew(controller, reg, data) outw((controller)->ioport + reg, data)
#define uhci_writel(controller, reg, data) outl((controller)->ioport + reg, data)

static void uhci_init_framelist(struct usb_dev* device);
static bool uhci_is_connected(struct usb_dev* device);
static int uhci_set_control_tranfer(struct usb_dev* device, struct usb_pipe* pipe, struct usb_setup_packet setup, void* buffer, bool no_response);
static int uhci_set_interrupt_transfer_r(struct usb_dev* device, struct usb_pipe* pipe, void* buffer, u16 size);

#define uhci_get_entry(addr, disable, type) (((addr) & ~0xF) | ((disable) << 0) | ((type) << 1))
#define uhci_set_frame_entry(controller, index, entry) (controller)->framelist[index] = entry
void uhci_set_td_on(struct uhci_dev* dev, u32 index, struct uhci_td td) { dev->tdpool[index] = td; }
void uhci_set_qh_on(struct uhci_dev* dev, u32 index, struct uhci_qh qh) { dev->qhpool[index] = qh; }

static int uhci_count_ports(struct uhci_dev uhci) {
    int port;
    for (port = 0; port < (uhci.ioport - PORTSC1) / 2; port++) {
        unsigned portsts = uhci_readw(&uhci, PORTSC1 + (port * 2));
        if (!(portsts & 0x0080) || portsts == 0xFFFF) break;
    }
    if (port > UHCI_MAX_CHILDRENS) {
        printf("uhci controller has more ports than normal");
        return UHCI_MAX_CHILDRENS;
    }
    return port;
}
static bool uhci_reset_port(struct uhci_dev* controller, int i) {
    u8 reg = PORTSC1 + (i * 2);

    u16 port = uhci_readw(controller, reg);
    if (port & 1 && port != 0xFFFF) {
        // Enable reset
        u16 cmd = port & ~0xA;
        uhci_writew(controller, reg, cmd);
        pit_timer_wait_ms(50);

        // Disable reset
        cmd &= ~0x200;
        uhci_writew(controller, reg, cmd);
        pit_timer_wait_ms(10);

        uhci_writew(controller, reg, (uhci_readw(controller, reg) & ~0x13) | 0xA);

        // Enable port
        uhci_writew(controller, reg, 0xE);

        int timeout = 100;
        while (!(uhci_readw(controller, reg) & (1 << 2)) && --timeout) pit_timer_wait_ms(1);
        if (!timeout) {
            set_printf_color(VGA_COLOR_LIGHT_RED);
                printf("Failed to enable UHCI Controller's port %d\n", i + 1);
            set_printf_color(VGA_COLOR_WHITE);
            return false;
        }
    }
    return true;
}
static int uhci_suspend(struct uhci_dev* controller) {
    if (!controller) return EINVAL;

    if (controller->dead) return 0;

    // pci_writew(controller->device.bus, controller->device.slot, controller->device.func, 0xC0, 0); // disable irqs

    // TODO: Complete this

    return 0;
}

void uhci_interrupt(registers_t* _) {
    for (int i = 0; i < usb_devices_count; i++) {
        if (usb_devices[i].type != UHCICONTROLLER) continue;

        struct uhci_dev* controller = (struct uhci_dev*)usb_devices[i].data.controller;

        u16 status = uhci_readw(controller, 0x2);
        uhci_writew(controller, 0x2, status | 1);

        if (status & 1) {
            printf("HEREa\n");
            // if (!usb_devices[i].interrupt_tranfers_enabled[INTERRUPT_TRANFERS_DEFAULT_INTERVAL]) { printf("suspicious behavior from a uhci hosted usb\n"); continue; }
            if (!usb_devices[i].is_connected(&usb_devices[i])) {
                controller->qhpool[usb_devices[i].interrupt_tranfers_enabled[INTERRUPT_TRANFERS_DEFAULT_INTERVAL]].vertical_pointer = 1;
                usb_devices[i].connected = 0;
                continue;
            }
            if ((controller->tdpool[0].status & ~0x800000)) {
                printf("%d\n", usb_devices[i].interrupt_tranfers_enabled[INTERRUPT_TRANFERS_DEFAULT_INTERVAL]);
                controller->qhpool[usb_devices[i].interrupt_tranfers_enabled[INTERRUPT_TRANFERS_DEFAULT_INTERVAL]].vertical_pointer = 1;

                if (usb_devices[i].handler) usb_devices[i].handler(&usb_devices[i]);

                controller->tdpool[0].status = 0x1800000;
                controller->tdpool[0].packet_header ^= 0x80000;

                controller->qhpool[usb_devices[i].interrupt_tranfers_enabled[INTERRUPT_TRANFERS_DEFAULT_INTERVAL]].vertical_pointer = uhci_get_entry((u32)(size_t)&controller->tdpool[0], 0, FRAME_TYPE_TD);
            }
        }
    }
}

struct usb_dev* uhci_init(struct pci_dev* dev) {
    u8 bus  = dev->bus->primary;
    u8 slot = PCI_DEV(dev->devfn);
    u8 fn   = PCI_FN(dev->devfn);

    u32 base_addr = pci_get_bar(bus, slot, fn, 4);

    struct uhci_dev* controller = kmalloc(sizeof(struct uhci_dev));
    controller->framelist = (frame_entry*)ALIGN((u64)kmalloc_align(1024 * sizeof(frame_entry), 4096), 4096);
    controller->tdpool = (struct uhci_td*)ALIGN((u64)kmalloc_align(32 * sizeof(struct uhci_td), 16), 16);
    controller->qhpool = (struct uhci_qh*)ALIGN((u64)kmalloc_align(16 * sizeof(struct uhci_qh), 16), 16);
    if (!controller || !controller->framelist || !controller->tdpool || !controller->qhpool) return (void*)ENOMEM;

    controller->header.pci_dev = dev;
    controller->ioport = base_addr & (~0xF);

    memset(controller->framelist, 0, 1024 * sizeof(frame_entry));

    if (!base_addr || !(base_addr & 1)) {
        printc("  is uhci controller a valid device?\n", VGA_COLOR_LIGHT_RED);
        goto err;
    }

    // Take control from BIOS
    // Enable I/O Bus mastering
    pci_writew(bus, slot, fn, 0xC0, 0x2000);
    pci_writew(bus, slot, fn, 0x4, pci_readw(bus, slot, fn, 0x4) | 0x5);

    // Host controller reset
    uhci_writew(controller, USBCMD, uhci_readw(controller, USBCMD) | CMD_HCRESET);
    pit_timer_wait_ms(25);

    int timeout = 1000;
    while ((uhci_readw(controller, USBCMD) & CMD_HCRESET) && --timeout) pit_timer_wait_ms(1);
    if (!timeout) {
        printc("failed to restart the uhci controller\n", VGA_COLOR_LIGHT_RED);
        goto err;
    }

    // Set Frame List Base Address
    uhci_writel(controller, FRBASE, (u32)(size_t)(controller->framelist) & ~0xFFF);
    uhci_writew(controller, FRNUM, 0x0);
    uhci_writew(controller, SOFMOD, 0x40);

    // Clean the status register
    uhci_writew(controller, USBSTS, 0xFFFF);

    // Enable it
    uhci_writew(controller, USBCMD, CMD_RS | CMD_MAXP);
    pit_timer_wait_ms(5);

    if (uhci_readw(controller, USBSTS) & STS_HCHALT) {
        printf("uhci error when enabling it!");
        goto err;
    }

    // Enable interrupts (iOC)
    uhci_writew(controller, USBINTR, INTR_IOCE);

    u8 irq = pci_readb(bus, slot, fn, PCI_HDR_DEV_ILINE_OFFSET);
    if (irq == 0xFF)
        printf("uhci dosen't has a interrupt line, it will not work\n");
    else
        irq_install_handler(irq, uhci_interrupt, 0);

    // Counts the available port of the uhci controller
    int max = uhci_count_ports(*controller);

    struct usb_dev* devices = kmalloc((sizeof(struct usb_dev) * max) + sizeof(int)) + sizeof(int);
    if (!devices) return (void*)ENOMEM;
    *(int*)((u64)devices - sizeof(int)) = max; // set how many devices the usb abtraction layer will check
    memset(devices, 0, sizeof(struct usb_dev) * max);
    // printf("%d\n", *(int*)((u64)devices - sizeof(int)));

    int id = usb_devices_count;
    for (int i = 0; i < max; i++) {
        struct usb_dev* dev = &devices[i];

        u16 sts = uhci_readw(controller, PORTSC1 + (i * 2));
        if (!(sts & 1) || !uhci_reset_port(controller, i)) {
            dev->data.controller = (void*)UHCI_PORT_NODEV; // This will mean there is a port but there isn't a device connected to that port
            continue;
        }

        dev->data.controller = (struct dev_host_head*)controller;
        dev->data.controller_reserved = kmalloc(sizeof(u8) * 2);
        if (!dev->data.controller_reserved) {
            dev->data.controller = (void*)ENOMEM;
            continue;
        }
        /*
            An array of two u8, one for the device address of that device (for the uhci functions), and the second one is for the speed (is lowspeed?)
        */
        (((u8*)dev->data.controller_reserved)[1]) = sts & (1 << 8);
        ((u8*)dev->data.controller_reserved)[0] = 0;
        dev->type = UHCICONTROLLER;
        dev->connected = 1;

        dev->init_framelist = uhci_init_framelist;
        dev->is_connected = uhci_is_connected;
        dev->set_control_tranfer = uhci_set_control_tranfer;
        dev->set_interrupt_tranfer = uhci_set_interrupt_transfer_r;

        struct usb_pipe* default_pipe = (dev->data.pipes[dev->data.pipes_count++] = kmalloc(sizeof(struct usb_pipe*)));
        if (!default_pipe) {
            dev->data.controller = (void*)ENOMEM;
            dev->data.pipes_count--;
            continue;
        }

        default_pipe->device = dev;
        default_pipe->descriptor.bEndpointAddress = 0;

        dev->init_framelist(dev);

        memset(&dev->data.device_descriptor, 0, sizeof(dev->data.device_descriptor));
        if (dev->set_control_tranfer(&devices[i], default_pipe, (struct usb_setup_packet){
                .requesttype = 0x80,
                .request = REQUEST_GET_DESCRIPTOR,
                .value = DESCRIPTOR_TYPE_DEVICE << 8,
                .index = 0,
                .lenght = sizeof(struct usb_device_descriptor) - 1
            }, &dev->data.device_descriptor, false) != 0) {
                printf("uhci controller port %d: failed to get device descriptor\n", i);
                dev->data.controller = (void*)ENODEV;
                continue;
            }
        if (usb_set_address(dev, ++id) != 0) {
            printf("uhci controller port %d: failed to set address %d\n", i, id);
            dev->data.controller = (void*)ENODEV;
            continue;
        }

        ((u8*)dev->data.controller_reserved)[0] = id;
    }

    return devices;
err:
    kfree(controller->framelist);
    kfree(controller->tdpool);
    kfree(controller->qhpool);
    kfree(controller);

    return NULL;
}

#ifdef DEBUG
    void DumpTd(struct uhci_td td) {
        printf("TD Dump:\n");
        printf("  TD Buffer: %x\n", td.buffer);
        printf("  TD Next:\n");
        printf("    Physical address: %x\n", td.next & ~0xF);
        printf("    Depth: %x\n", td.next & (1 << 2));
        printf("    Memory structure type: %s\n", td.next & (1 << 1) ? "Queue head" : "Transfer descriptor");
        printf("    Terminate: %x\n", td.next & (1 << 0));
        printf("  TD Status:\n");
        if (td.status & (1 << 17)) printf("    Bit stuff error\n");
        if (td.status & (1 << 18)) printf("    Timeout CRC\n");
        if (td.status & (1 << 19)) printf("    Non-Acknowledged\n");
        if (td.status & (1 << 20)) printf("    Babble Detected\n");
        if (td.status & (1 << 21)) printf("    Data Buffer Error\n");
        if (td.status & (1 << 22)) printf("    Stalled\n");
        if (td.status & (1 << 23)) printf("    Active\n");
        if (td.status & (1 << 24)) printf("    Interrupt On Complete\n");
        if (td.status & (1 << 25)) printf("    Is Isochronous\n");
        if (td.status & (1 << 26)) printf("    Low speed\n");
        if (td.status & (1 << 29)) printf("    Short Packet Detect\n");
        printf("    Error counter: %x\n", (td.status >> 17) & 0x1F);
        printf("    Actual length: %x\n", td.status & 0x7FF);
        printf("  TD Packet header:\n");
        printf("    Maximum length: %x\n", td.packet_header >> 21);
        printf("    Data toggle: %x\n", td.packet_header & (1 << 19));
        printf("    Packet type: %x\n", td.packet_header & 0xFF);
        printf("    Endpoint: %x\n", (td.packet_header >> 15) & 0x7);
        printf("    Device: %x\n", (td.packet_header >> 8) & 0x3F);
    }
#endif

static int uhci_set_control_tranfer(struct usb_dev* uhci, struct usb_pipe* pipe, struct usb_setup_packet setup, void* buffer, bool no_response) {
    if (!uhci || !pipe) return -EINVAL;

    struct uhci_dev* controller = (struct uhci_dev*)uhci->data.controller;
    int packets = 1;

    bool speed = ((u8*)uhci->data.controller_reserved)[1];
    uhci_set_td_on(controller, 0, (struct uhci_td){
        .buffer = (u32)(size_t)&setup,
        .next = ((u32)(size_t)(&controller->tdpool[1]) & ~0xF) | (1 << 2),
        .packet_header = (HEADER_TYPE_SETUP & 0xFF) | ((*(u8*)uhci->data.controller_reserved) << 8) | ((0x7 & 0x7ff) << 21),
        .status = (1 << 23) | (speed << 26) | (3 << 27)
    }); // Setup packet

    // Put more data packets if the setup packet wants more than the max limit
    if (((setup.lenght > uhci->data.device_descriptor.bMaxpacketsize) && uhci->data.device_descriptor.bMaxpacketsize) && !no_response) {
        int max = (int)(setup.lenght / uhci->data.device_descriptor.bMaxpacketsize) + 1;
        int n = 0;

        for (int i = 1; i <= max; i++) {
            u32 size = (i == max ? (setup.lenght - (uhci->data.device_descriptor.bMaxpacketsize * (i - 1))) : uhci->data.device_descriptor.bMaxpacketsize);

            uhci_set_td_on(controller, packets, (struct uhci_td){
                .buffer = (u32)(size_t)buffer + n,
                .next = ((u32)(size_t)(&controller->tdpool[packets + 1]) & ~0xF) | (1 << 2),
                .packet_header = (HEADER_TYPE_IN & 0xFF) | ((*(u8*)uhci->data.controller_reserved) << 8) | ((bool)(i % 2) << 19) | (((size - 1) & 0x7ff) << 21),
                .status = (1 << 23) | (speed << 26) | (3 << 27) | (1 << 29)
            }); // Data packet
            n += size;
            packets++;
        }
    } else {
        uhci_set_td_on(controller, 1, (struct uhci_td){
            .buffer = (u32)(size_t)buffer,
            .next = no_response ? 1 : ((u32)(size_t)(&controller->tdpool[packets + 1]) & ~0xF) | (1 << 2),
            .packet_header = (HEADER_TYPE_IN & 0xFF) | ((*(u8*)uhci->data.controller_reserved) << 8) | (1 << 19) | (((setup.lenght - 1) & 0x7ff) << 21) | ((pipe->descriptor.bEndpointAddress & 0xF) << 15),
            .status = (1 << 23) | (speed << 26) | (3 << 27) | (1 << 29)
        }); // Data packet
        packets++;
    }

    if (!no_response) { // For requests like "set_address" that are just two packets (SETUP and STATUS)
        uhci_set_td_on(controller, packets, (struct uhci_td){
            .buffer = 0,
            .next = 1,
            .packet_header = (HEADER_TYPE_OUT & 0xFF) | ((*(u8*)uhci->data.controller_reserved) << 8) | (1 << 19) | ((0 & 0x7ff) << 21),
            .status = (1 << 23) | (speed << 26) | (3 << 27)
        }); // Status packet
        packets++;
    }

    uhci_set_qh_on(controller, 0, (struct uhci_qh){
        .vertical_pointer = uhci_get_entry((u32)(size_t)&controller->tdpool[0], 0, FRAME_TYPE_TD),
        .horizontal_pointer = 1
    });
    uhci_set_frame_entry(controller, 0, uhci_get_entry((u32)(size_t)&controller->qhpool[0], 0, FRAME_TYPE_QH));

    int timeout = 10000;
    for (int i = 0; i < packets; i++) {
        #ifdef DEBUG
            printf(".");
        #endif
        timeout = 10000;
        while (controller->tdpool[i].status & (1 << 23) && --timeout) pit_timer_wait_ms(1); // Wait for the TD active bit to turn off

        if (!timeout) {
            #ifdef DEBUG
                printf("td%d timeout\n", i + 1);
            #endif
            return -ETIME;
        }

        // Check for errors
        if (controller->tdpool[i].status & 0x7E0000) {
            #ifdef DEBUG
                printf("td%d failed\n", i + 1);
                DumpTd(controller->tdpool[i]);
            #endif
            return -2;
        }
    }
    uhci_writew(controller, USBSTS, 0xFFFF);

    return 0;
}

static void uhci_init_framelist(struct usb_dev* uhci) {
    struct uhci_dev* controller = (struct uhci_dev*)uhci->data.controller;
    if (!uhci || !controller) return;

    for (int i = 5; i > 0; i--) {
        controller->qhpool[i * 2].horizontal_pointer = uhci_get_entry((u32)(size_t)&controller->qhpool[(i * 2) - 2], 0, FRAME_TYPE_QH);
        controller->qhpool[i * 2].vertical_pointer = 1;   // T=1 → (grok redesigned this part of code) empty
    }
    controller->qhpool[0].horizontal_pointer = 1;
    controller->qhpool[0].vertical_pointer   = 1;

    for (int i = 0; i < 1024; i++) {
        if ((i & 31) == 0) // Every 32ms
            uhci_set_frame_entry(controller, i, uhci_get_entry((u32)(size_t)&controller->qhpool[INTERRUPT_TRANSFER_INTERVAL_32MS], 0, FRAME_TYPE_QH));
        else if ((i & 15) == 0) // Every 16ms
            uhci_set_frame_entry(controller, i, uhci_get_entry((u32)(size_t)&controller->qhpool[INTERRUPT_TRANSFER_INTERVAL_16MS], 0, FRAME_TYPE_QH));
        else if ((i & 7) == 0) // Every 8ms
            uhci_set_frame_entry(controller, i, uhci_get_entry((u32)(size_t)&controller->qhpool[INTERRUPT_TRANSFER_INTERVAL_8MS], 0, FRAME_TYPE_QH));
        else if ((i & 3) == 0) // Every 4ms
            uhci_set_frame_entry(controller, i, uhci_get_entry((u32)(size_t)&controller->qhpool[INTERRUPT_TRANSFER_INTERVAL_4MS], 0, FRAME_TYPE_QH));
        else if ((i & 1) == 0) // Every 2ms
            uhci_set_frame_entry(controller, i, uhci_get_entry((u32)(size_t)&controller->qhpool[INTERRUPT_TRANSFER_INTERVAL_2MS], 0, FRAME_TYPE_QH));
        else // Every 1ms
            uhci_set_frame_entry(controller, i, uhci_get_entry((u32)(size_t)&controller->qhpool[INTERRUPT_TRANSFER_INTERVAL_1MS], 0, FRAME_TYPE_QH));
    }
}
static int uhci_set_interrupt_transfer_r(struct usb_dev* device, struct usb_pipe* pipe, void* buffer, u16 size) {
    struct uhci_dev* controller = (struct uhci_dev*)device->data.controller;
    if (!device || !controller || !pipe) return -EINVAL;

    bool speed = ((u8*)device->data.controller_reserved)[1];
    if (PIPE_IS_WRITE(pipe)) return -EINVAL; // return an err if that pipe is only for writing

    uhci_set_td_on(controller, 0, (struct uhci_td){
        .buffer = (u32)(size_t)buffer,
        .next = 1,
        .packet_header = (HEADER_TYPE_IN & 0xFF) | ((((u8*)device->data.controller_reserved)[0]) << 8) | (1 << 19) | ((pipe->descriptor.bEndpointAddress & 0xF) << 15) | (((size - 1) & 0x7ff) << 21),
        .status = (1 << 23) | (speed << 26) | (3 << 27) | (1 << 24)
    });

    device->interrupt_tranfers_enabled[INTERRUPT_TRANFERS_DEFAULT_INTERVAL] = INTERRUPT_TRANFERS_DEFAULT_INTERVAL;
    controller->qhpool[INTERRUPT_TRANFERS_DEFAULT_INTERVAL].vertical_pointer = uhci_get_entry((u32)(size_t)&controller->tdpool[0], 0, FRAME_TYPE_TD);

    return 0;
}

static bool uhci_is_connected(struct usb_dev* uhci) { // TODO: Check this
    if (!uhci || !uhci->data.controller) return false;
    return uhci_readw((struct uhci_dev*)uhci->data.controller, 0x10 + (2 * ((((u8*)uhci->data.controller_reserved)[0])))) & 1;
}

#if 0

void uhci_free(struct UHCIDevice* controller) {
    kfree(controller->framelist);
    kfree(controller->tdpool);
    kfree(controller->qhpool);
    kfree(controller);
}

#endif

int uhci_pci_driver_init(struct pci_dev* dev) {
    struct usb_dev* devices = uhci_init(dev);

    u8 progif = pci_readb(dev->bus->primary, PCI_DEV(dev->devfn), PCI_FN(dev->devfn),
                                            offsetof(struct pci_hdr, common.prog_if));

    if (PTR_FAILED((u64)devices)) {
        printc("uhci controller initialization failed!\n", VGA_COLOR_RED);
        #ifdef DEBUG
            perror(-((i64)devices));
        #endif
        return -((i64)devices);
    }

    for (int i = 0; i < *(int*)((u64)devices - sizeof(int)); i++) { // That array has the size of 2 usbdevices, because uhci have 2 ports
        if (PTR_FAILED((u64)devices[i].data.controller)) {
            // This device failed
            printf("device %d from uhci controller failed to initialize\n", i);
            #ifdef DEBUG
                perror(-(i64)devices[i].data.controller);
            #endif
            continue;
        } else if ((u64)devices[i].data.controller == UHCI_PORT_NODEV) continue; // No device connected
        devices[i].index = usb_devices_count;

        usb_devices[usb_devices_count++] = devices[i];
        struct usb_dev* device = &usb_devices[usb_devices_count - 1];

        device->init_framelist(device);

        int status;
        for (int x = 0; x < device->data.device_descriptor.bNumConfigurations + 1; x++)
            if ((status = usb_get_configuration(&usb_devices[usb_devices_count - 1], x)) != 0) {
                printf("\nfailed to get usb's %d configuration\n", x);
                #ifdef DEBUG
                    perror(status);
                #endif
                continue;
            }

        device->hid_protocol = usb_is_hid(&usb_devices[usb_devices_count - 1]);
        if (device->hid_protocol) {
            // hid_set_protocol(*device, REPORT_PROTOCOL);

            // Detect if the device is a mouse or a keyboard by its protocol value
            #define KEYBOARD_PROTOCOL 1
            #define MOUSE_PROTOCOL 2

            if (device->data.interface[0].bInterfaceProtocol == KEYBOARD_PROTOCOL) {
                int stat;
                if (PTR_FAILED((stat = (u64)hid_kbd_init(device)))) {
                    printf("failed to initialize hid keyboard with errno %d\n", stat);
                    #ifdef DEBUG
                        perror(stat);
                    #endif
                }
            } else if (device->data.interface[0].bInterfaceProtocol == MOUSE_PROTOCOL) {
                // Nothing...
            }
        }
    }
    kfree(devices);

    return 0;
}
