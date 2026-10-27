#pragma once

#include "drivers/pci.h"
#include "sys/types.h"

enum uhci_rh_state {
    UHCI_RH_RUNNING = 1,
    UHCI_RH_SUSPENDED,
    UHCI_RH_STOP,
    UHCI_RH_RESUMING,
};

struct uhci_td {
    u32 next;
    u32 status;
    u32 packet_header;
    u32 buffer;

    // we have 128 bits free, but i dont know what to do with them
} __attribute__((packed));
struct uhci_qh {
    u32 horizontal_pointer;
    u32 vertical_pointer;
} __attribute__((packed));

struct uhci_hcd {
    u16 io_port;
    u16 rh_numports;
    struct pci_dev* dev;

    // frame list
    void* frame_list_dma; // this is just an aligned address by dma_create_pool
    u32 frame_number;

    struct uhci_td** td_pool;
    struct uhci_qh** qh_pool;

    unsigned int is_stopped     : 1;
    unsigned int is_initialized : 1;
    enum uhci_rh_state rh_state;
};

#define UHCI_NUMFRAMES      1024
#define UHCI_MAX_SOF_NUM    2047

int uhci_start(struct uhci_hcd* hcd);
struct uhci_hcd* uhci_create(struct pci_dev* dev);
