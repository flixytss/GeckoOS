/*
    i will remake all the files that are related to usb, i will make this file
   based of what i understood of the linux uhci-hcd.c file. some GPL3-0 licence
   and 2026 by Pumpkicks
*/

#include "drivers/usb/host/uhci-hcd.h"
#include "drivers/interfaces/uhci.h"
#include "drivers/pci.h"
#include "drivers/pit.h"
#include "mem.h"
#include "ports.h"
#include "sys/types.h"
#include "terminal/printf.h"
#include "terminal/terminal.h"

#define uhci_

static u8 uhci_readb(const struct uhci_hcd *hcd, u16 offset) {
    return inb(hcd->io_port + offset);
}
static u16 uhci_readw(const struct uhci_hcd *hcd, u16 offset) {
    return inw(hcd->io_port + offset);
}
static u32 uhci_readl(const struct uhci_hcd *hcd, u16 offset) {
    return inl(hcd->io_port + offset);
}
static void uhci_writeb(const struct uhci_hcd *hcd, u16 offset, u8 data) {
    outb(hcd->io_port + offset, data);
}
static void uhci_writew(const struct uhci_hcd *hcd, u16 offset, u16 data) {
    outw(hcd->io_port + offset, data);
}
static void uhci_writel(const struct uhci_hcd *hcd, u16 offset, u32 data) {
    outl(hcd->io_port + offset, data);
}

void uhci_generic_reset_hc(struct uhci_hcd *uhci) {
    uhci_writew(uhci, USBCMD, CMD_HCRESET);
    pit_timer_wait_ms(15);

    if (uhci_readw(uhci, USBCMD) & CMD_HCRESET)
        pretty_printf("dev didn't restarted\n")

    uhci_writew(uhci, USBCMD, 0);
    uhci_writew(uhci, USBINTR, 0);
}

static void configure_hc(struct uhci_hcd* hcd) {
    if (!hcd->frame_list_dma)
        printf("configure_hc: frame_list_dma is null\n");

    uhci_writeb(hcd, SOFMOD, 0x40); // 0x40 as the default length
    uhci_writel(hcd, FRBASE, (u32)(size_t)hcd->frame_list_dma); // setting the framelist dma into the controller
    uhci_writew(hcd, FRNUM, hcd->frame_number & UHCI_MAX_SOF_NUM);
}

static void uhci_get_frame_number(struct uhci_hcd* hcd) {
    if (!hcd->is_stopped)
        hcd->frame_number += (uhci_readw(hcd, FRNUM) - hcd->frame_number) & (UHCI_NUMFRAMES - 1);
}

static void start_rh(struct uhci_hcd* hcd) {
    hcd->is_stopped = 0;

    // Something about Aspeed, TODO: add later (or just copy)
    uhci_writew(hcd, USBCMD, CMD_RS | CMD_CF | CMD_MAXP);
    uhci_writew(hcd, USBINTR, INTR_IOCE | INTR_TIE | INTR_RIE | INTR_SPIE);
    uhci_writew(hcd, USBSTS, 0x00FF);
}

static void suspend_rh(struct uhci_hcd* hcd, enum uhci_rh_state new_state) {
    int int_en = INTR_RIE, egsm_en = CMD_EGSM, wakeup_available = 1;

    uhci_writew(hcd, USBINTR, int_en);
    uhci_writew(hcd, USBCMD, egsm_en | CMD_CF);

    pit_timer_wait_ms(25);

    if (!(uhci_readw(hcd, USBSTS) & STS_HCHALT)) {
        hcd->rh_state = UHCI_RH_SUSPENDED;
        return;
    }
    hcd->rh_state = new_state;
}

static void wakeup_rh(struct uhci_hcd* hcd) {
    if (hcd->rh_state == UHCI_RH_SUSPENDED) {
        u16 egsm = uhci_readw(hcd, USBCMD) & CMD_EGSM;

        hcd->rh_state = UHCI_RH_RESUMING;
        uhci_writew(hcd, USBCMD, CMD_FGR | CMD_CF | egsm);
        pit_timer_wait_ms(20);

        uhci_writew(hcd, USBCMD, CMD_CF);
        pit_timer_wait_ms(4);

        if (uhci_readw(hcd, USBCMD) & CMD_FGR)
            pretty_printf("the uhci controller dosen't want to sleep yet")
    }

    start_rh(hcd);
    hcd->rh_state = UHCI_RH_RUNNING;
}

int uhci_start(struct uhci_hcd* hcd) {
    uhci_generic_reset_hc(hcd);
    configure_hc(hcd);
    hcd->is_initialized = 1;
    start_rh(hcd);

    return 0;
}

struct uhci_hcd* uhci_create(struct pci_dev* dev) {
    struct uhci_hcd* hcd = memset(kmalloc(sizeof(struct uhci_hcd)), 0, sizeof(struct uhci_hcd)); // TODO: Add out of memory

    hcd->dev = dev;
    hcd->io_port = pci_get_bar(dev->bus->primary, PCI_DEV(dev->devfn), PCI_FN(dev->devfn), 4) & ~0xF;

    // Hardware variables
    hcd->frame_list_dma = dma_create_pool(1, 1024, 4096);
    hcd->td_pool = dma_create_pool(sizeof(struct uhci_td*), 16, 4);
    hcd->qh_pool = dma_create_pool(sizeof(struct uhci_qh*), 8, 4);

    if (!hcd->frame_list_dma || !hcd->td_pool || !hcd->qh_pool)
        printf("failed to create dma pools\n");
    return hcd;
}
