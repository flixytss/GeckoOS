#include <drivers/interfaces/uhci.h>
#include "drivers/mass_storage_controller/ahci.h"
#include "drivers/ide.h"
#include "drivers/usb/host/uhci-hcd.h"
#include <drivers/pci.h>
#include <drivers/e1000.h>
#include <mem.h>
#include <ports.h>
#include <stddef.h>
#include "sys/types.h"
#include <terminal/printf.h>
#include "drivers/pci_hcd.h"

#define PCI_CFG_ADDR 0xCF8
#define PCI_CFG_DATA 0xCFC

struct pci_bus pci_root_bus;

u32 pci_readl(u32 bus, u32 slot, u32 func, u8 off) {
    u32 addr = (bus << 16) | (slot << 11) | (func << 8) | (off & 0xFC) | 0x80000000;
    outl(PCI_CFG_ADDR, addr);
    return inl(PCI_CFG_DATA);
}

u16 pci_readw(u32 bus, u32 slot, u32 func, u8 off) {
    u32 addr = (bus << 16) | (slot << 11) | (func << 8) | (off & 0xFC) | 0x80000000;
    outl(PCI_CFG_ADDR, addr);
    return (u16)((inl(PCI_CFG_DATA) >> ((off & 2) * 8)) & 0xffff);
}

u8 pci_readb(u32 bus, u32 slot, u32 func, u8 off) {
    u32 v = pci_readl(bus, slot, func, off & ~3);
    return (v >> ((off & 3) * 8)) & 0xff;
}

void pci_writel(u32 bus, u32 slot, u32 func, u32 off, u32 data) {
    u32 addr = (bus << 16) | (slot << 11) | (func << 8) | (off & 0xFC) | 0x80000000;
    outl(PCI_CFG_ADDR, addr);
    outl(PCI_CFG_DATA, data);
}

void pci_writew(u32 bus, u32 slot, u32 func, u32 off, u16 data) {
    u32 align = off & ~3;
    u32 o     = pci_readl(bus, slot, func, align);
    u8  shift = (off & 2) * 8;
    pci_writel(bus, slot, func, align, (o & ~(0xffff << shift)) | ((u32)data << shift));
}

void pci_writeb(u32 bus, u32 slot, u32 func, u32 off, u8 data) {
    u32 align = off & ~3;
    u32 o     = pci_readl(bus, slot, func, align);
    u8  shift = (off & 3) * 8;
    pci_writel(bus, slot, func, align, (o & ~(0xff << shift)) | ((u32)data << shift));
}

#define PCI_CLASSES_NUM 120
const char* pci_classes[PCI_CLASSES_NUM] = {
    "Unclassified",
    "Mass Storage Controller",
    "Network Controller",
    "Display Controller",
    "Multimedia Controller",
    "Memory Controller",
    "Bridge",
    "Simple Communication Controller",
    "Base System Peripheral",
    "Input Device Controller",
    "Docking Station",
    "Processor",
    "Serial Bus Controller",
    "Wireless Controller",
    "Intelligent Controller",
    "Satellite Communication Controller",
    "Encryption Controller",
    "Signal Processing Controller",
    "Processing Accelerator",
    "Non-Essential Instrumentation"
};

static const char *class2str(u8 class) {
    if (class > PCI_CLASSES_NUM || class < 0) return "Unknown";
    return pci_classes[class];
}

#ifdef DEBUG
    static const char *progif2usbcontroller(u8 progif) {
        switch (progif) {
            case 0x00: return "UHCI Controller";
            case 0x10: return "OHCI Controller";
            case 0x20: return "EHCI Controller";
            case 0x30: return "xHCI Controller";
            case 0x80: return "Unspecified Controller";
            case 0xFE: return "USB Device (Not a host controller)";
        }
        return "Unknown";
    }
#endif

#ifdef DEBUG
    const char *nic_vendor_str(u16 vendor) {
        switch (vendor) {
            case 0x8086: return "Intel";
            case 0x10EC: return "Realtek";
            case 0x14E4: return "Broadcom";
            case 0x1022: return "AMD";
            case 0x15AD: return "VMware";
        } return NULL;
    }

    static const char *nic_model_str(u16 vendor, u16 device) {
        switch (vendor) {
            case 0x8086:
                switch (device) {
                    case 0x100E: return "82540EM";
                    case 0x100F: return "82545EM";
                    case 0x10D3: return "82574L";
                    case 0x1533: return "I210";
                    case 0x15B8: return "I219-V";
                } break;
            case 0x10EC:
                switch (device) {
                    case 0x8139: return "RTL8139";
                    case 0x8168: return "RTL8111/8168B";
                    case 0x8125: return "RTL8125";
                } break;
            case 0x15AD:
                switch (device) {
                    case 0x0720: return "VMXNET3";
                    case 0x0740: return "PCnet32";
                } break;
        } return NULL;
    }
#endif

static void alloc_new_bus(struct pci_bus *parent, struct pci_dev *self) {
    struct pci_bus *child = kmalloc(sizeof(*child));

    child->next     = NULL;
    child->parent   = parent;
    child->self     = self;
    child->devices  = NULL;
    child->children = NULL;

    child->secondary = pci_readb(
        parent->bus, PCI_DEV(self->devfn), PCI_FN(self->devfn),
        offsetof(struct pci_hdr, bridge.ss_busnum));

    child->bus     = child->secondary;
    child->primary = child->secondary;

    if (child->secondary == parent->bus)
        parent->subordinate++;

    if (parent->children == NULL) {
        parent->children = child;
        enumerate_pcibus(child);
        return;
    }

    struct pci_bus *end = parent->children;
    while (end->next && end->next != end)
        end = end->next;

    end->next = child;
    enumerate_pcibus(child);
}

u8 enumerate_pcidev(struct pci_bus *parent, u8 dev, u8 func) {
    u32 tmp = pci_readl(parent->primary, dev, func,
                             offsetof(struct pci_common_hdr, vendid));

    if (tmp == 0xffffffff)
        return 0;

    struct pci_dev *pci_dev = kmalloc(sizeof(*pci_dev));

    u8 htype = pci_readb(parent->primary, dev, func,
                              offsetof(struct pci_hdr, common.htype));

    pci_dev->bus     = parent;
    pci_dev->devfn   = (dev << 3) | (func & 7);
    pci_dev->class   = pci_readb(parent->primary, dev, func,
                                 offsetof(struct pci_hdr, common.clas));
    pci_dev->device  = tmp >> 16;
    pci_dev->vendor  = tmp & 0xffff;
    pci_dev->sibling = NULL;
    pci_dev->next    = NULL;

    if ((htype & 0x7f) == 1)
        alloc_new_bus(parent, pci_dev);

    if (parent->devices == NULL) {
        parent->devices = pci_dev;
        return 0;
    }

    struct pci_dev *endpoint = parent->devices;
    while (endpoint->sibling)
        endpoint = endpoint->sibling;

    endpoint->sibling = pci_dev;
    return 0;
}

void enumerate_pcibus(struct pci_bus *bus) {
    for (u8 device = 0; device < 32; device++) {
        for (u8 fn = 0; fn < 8; fn++) {
            u8 htype = pci_readb(bus->primary, device, fn,
                                      offsetof(struct pci_hdr, common.htype));
            if (htype == 0xff)
                break;

            enumerate_pcidev(bus, device, fn);

            if (!(htype & 0x80))
                break;
        }
    }
}

void enumerate_pci() {
    pci_root_bus.bus         = 0;
    pci_root_bus.primary     = 0;
    pci_root_bus.children    = NULL;
    pci_root_bus.devices     = NULL;
    pci_root_bus.parent      = NULL;
    pci_root_bus.self        = NULL;
    pci_root_bus.subordinate = 0;

    enumerate_pcibus(&pci_root_bus);
}

static void walk_buses(struct pci_bus *start, void (*cb)(struct pci_dev *)) {
    struct pci_bus *cur = start;
    struct pci_bus *visited[64];
    int nvisited = 0;

    while (cur) {
        int already_seen = 0;
        for (int i = 0; i < nvisited; i++) {
            if (visited[i] == cur) { already_seen = 1; break; }
        }
        if (already_seen) break;
        if (nvisited < 64) visited[nvisited++] = cur;

        for (struct pci_dev *dev = cur->devices; dev; dev = dev->sibling)
            cb(dev);

        if (cur->children) { cur = cur->children; continue; }
        cur = cur->next;
    }
}

// Helpers
u32 PciRead(struct pci_device_shortcut device, u8 off) { // A wrapper for pci_readl
    return pci_readl(device.bus, device.slot, device.func, off);
}
void PciWrite(struct pci_device_shortcut device, u8 off, u32 data) { // A wrapper for pci_writel
    pci_writel(device.bus, device.slot, device.func, off, data);
}

static void lspci_cb(struct pci_dev *dev) {
    #ifdef DEBUG
        u8 subclass = pci_readb(dev->bus->primary, PCI_DEV(dev->devfn), PCI_FN(dev->devfn), offsetof(struct pci_hdr, common.subclass));
        u8 progif = pci_readb(dev->bus->primary, PCI_DEV(dev->devfn), PCI_FN(dev->devfn), offsetof(struct pci_hdr, common.prog_if));

        if (subclass == 0x3) {
            printf("%02x:%02x.%d %s (%s) (Subclass: 0x%X, ProgIF: 0x%X)\n",
                dev->bus->primary,
                PCI_DEV(dev->devfn),
                PCI_FN(dev->devfn),
                class2str((u8)dev->class),
                progif2usbcontroller(progif),
                subclass,
                progif);
        } else
            printf("%02x:%02x.%d %s (Subclass: 0x%X, ProgIF: 0x%X)\n",
                dev->bus->primary,
                PCI_DEV(dev->devfn),
                PCI_FN(dev->devfn),
                class2str((u8)dev->class),
                subclass,
                progif);
    #else
        printf("%02x:%02x.%d %s\n",
           dev->bus->primary,
           PCI_DEV(dev->devfn),
           PCI_FN(dev->devfn),
           class2str(dev->class & 0xff));
    #endif
}

void pci_lspci() {
    walk_buses(&pci_root_bus, lspci_cb);
}

static void pci_filter(struct pci_dev *dev) {
    u8 bus  = dev->bus->primary;
    u8 slot = PCI_DEV(dev->devfn);
    u8 fn   = PCI_FN(dev->devfn);
    u8 subclass = pci_readb(bus, slot, fn,
                            offsetof(struct pci_hdr, common.subclass));
    u8 progif = pci_readb(bus, slot, fn, offsetof(struct pci_hdr, common.prog_if));

    switch (dev->class & 0xFF) {
        case PCI_CLASS_NETWORK:
            #ifdef DEBUG
                const char *vendor = nic_vendor_str(dev->vendor);
                const char *model  = nic_model_str(dev->vendor, dev->device);

                if (vendor && model)
                    printf("  [%02x:%02x.%d] %s %s\n", bus, slot, fn, vendor, model);
                else if (vendor)
                    printf("  [%02x:%02x.%d] %s (device %04x, subclass %02x)\n",
                        bus, slot, fn, vendor, dev->device, subclass);
                else
                    printf("  [%02x:%02x.%d] Unknown NIC (%04x:%04x, subclass %02x)\n",
                        bus, slot, fn, dev->vendor, dev->device, subclass);
            #endif
            if (dev->vendor == 0x8086 && dev->device == 0x100E)
                e1000_init(bus, slot, fn);
            break;
        case PCI_CLASS_SBC:
            if (subclass != 0x3) break; // Support only for USB controllers
            // dev->irq = pci_readb(bus, slot, fn, offsetof(struct pci_hdr, common.prog_if));

            pci_dev_disable_bios(dev);
            pci_dev_enable_busmastering(dev);

            switch (progif) {
                case 0x0: // UHCI
                    #ifdef DEBUG
                        printf("  [%02x:%02x.%d] UHCI Controller\n",
                            bus, slot, fn);
                    #endif
                    uhci_start(uhci_create(dev));
                    printf("\n");
                    break;
                case 0x10: // OHCI
                    #ifdef DEBUG
                        printf("  [%02x:%02x.%d] OHCI Controller\n",
                            bus, slot, fn);
                    #endif
                    break;
                case 0x20: // EHCI
                    #ifdef DEBUG
                        printf("  [%02x:%02x.%d] EHCI Controller\n",
                            bus, slot, fn);
                    #endif
                    break;
                default: break;
            } break;
        case 0x01: // Mass storage controller
            if (subclass == 0x06) {
                #ifdef DEBUG
                    printf("  [%02x:%02x.%d] AHCI Controller\n",
                        bus, slot, fn);
                #endif
                ahci_init((struct pci_device_shortcut){bus, slot, fn});
            } else if (subclass == 0x01) {
                #ifdef DEBUG
                    printf("  [%02x:%02x.%d] IDE Controller\n",
                        bus, slot, fn);
                #endif

                ide_init(0x1F0, 0x3F6, 0x170, 0x376, 0x000);
            }
            break;
    }
}

void pci_detect_controllers() {
    #ifdef DEBUG
        printf("Available controllers:\n");
    #endif
    walk_buses(&pci_root_bus, pci_filter);
}

u32 pci_get_bar(u32 bus, u32 slot, u32 func, u8 index) {
    return pci_readl(bus, slot, func, BAR0_OFF + (BAR_SIZE * index));
}
