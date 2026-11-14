#include <drivers/interfaces/uhci.h>
#include "drivers/mass_storage_controller/ahci.h"
#include "drivers/ide.h"
#include "drivers/usb/host/uhci-hcd.h"
#include <drivers/pci.h>
#include <drivers/e1000.h>
#include <mem/mmu.h>
#include <ports.h>
#include <stddef.h>
#include "sys/types.h"
#include <terminal/printf.h>

#define PCI_CFG_ADDR 0xCF8
#define PCI_CFG_DATA 0xCFC

struct pci_bus pci_root_bus;

u32 pci_readl(struct pci_dev* dev, u32 off) {
    u32 addr = (dev->_bus << 16) | (dev->_slot << 11) | (dev->_fn << 8) | (off & 0xFC) | 0x80000000;
    outl(PCI_CFG_ADDR, addr);
    return inl(PCI_CFG_DATA);
}

u16 pci_readw(struct pci_dev* dev, u32 off) {
    u32 addr = (dev->_bus << 16) | (dev->_slot << 11) | (dev->_fn << 8) | (off & 0xFC) | 0x80000000;
    outl(PCI_CFG_ADDR, addr);
    return (u16)((inl(PCI_CFG_DATA) >> ((off & 2) * 8)) & 0xffff);
}

u8 pci_readb(struct pci_dev* dev, u32 off) {
    u32 v = pci_readl(dev, off & ~3);
    return (v >> ((off & 3) * 8)) & 0xff;
}

void pci_writel(struct pci_dev* dev, u32 off, u32 data) {
    u32 addr = (dev->_bus << 16) | (dev->_slot << 11) | (dev->_fn << 8) | (off & 0xFC) | 0x80000000;
    outl(PCI_CFG_ADDR, addr);
    outl(PCI_CFG_DATA, data);
}

void pci_writew(struct pci_dev* dev, u32 off, u16 data) {
    u32 align = off & ~3;
    u32 o     = pci_readl(dev, align);
    u8  shift = (off & 2) * 8;
    pci_writel(dev, align, (o & ~(0xffff << shift)) | ((u32)data << shift));
}

void pci_writeb(struct pci_dev* dev, u32 off, u8 data) {
    u32 align = off & ~3;
    u32 o     = pci_readl(dev, align);
    u8  shift = (off & 3) * 8;
    pci_writel(dev, align, (o & ~(0xff << shift)) | ((u32)data << shift));
}

static void pci_dev_disable_bios(struct pci_dev* dev)
    { pci_writew(dev, 0xC0, 0x2000); }
static void pci_dev_enable_memory_space(struct pci_dev* dev)
    { pci_writew(dev, 0x4, pci_readw(dev, 0x4) | (1 << 1)); }
static void pci_dev_enable_busmastering(struct pci_dev* dev)
    { pci_writew(dev, 0x4, pci_readw(dev, 0x4) | (1 << 2)); }
static u32 pci_get_bar(struct pci_dev* dev, u8 index)
    { return dev->readl(dev, BAR0_OFF + (BAR_SIZE * index)); }
static u8 pci_dev_get_intline(struct pci_dev* dev)
    { return pci_readb(dev, 0x3C); }
static u64 pci_get_bar_size(struct pci_dev* dev, u8 index) {
    u32 reg;
    u32 bar, mask;

    reg = BAR0_OFF + (BAR_SIZE * index);
    bar = dev->readl(dev, reg);

    dev->writel(dev, reg, 0xFFFFFFFF);
    mask = dev->readl(dev, reg);
    dev->writel(dev, reg, bar);

    return ~(mask & ~0xF) + 1;
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

    child->secondary = self->readb(self, offsetof(struct pci_hdr, bridge.ss_busnum));

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
    int ret = 0;
    struct pci_dev *pci_dev = kmalloc(sizeof(*pci_dev));
    if (!pci_dev) {
        ret = 1;
        goto go_and_free;
    }

    pci_dev->_bus = parent->primary;
    pci_dev->_slot = dev;
    pci_dev->_fn = func;

    pci_dev->readb = pci_readb;
    pci_dev->readw = pci_readw;
    pci_dev->readl = pci_readl;
    pci_dev->writeb = pci_writeb;
    pci_dev->writew = pci_writew;
    pci_dev->writel = pci_writel;

    pci_dev->disable_bios_handle = pci_dev_disable_bios;
    pci_dev->enable_busmastering = pci_dev_enable_busmastering;
    pci_dev->enable_memory_space = pci_dev_enable_memory_space;
    pci_dev->get_intline = pci_dev_get_intline;
    pci_dev->get_base_memory = pci_get_bar;
    pci_dev->get_base_memory_size = pci_get_bar_size;

    u32 tmp = pci_dev->readl(pci_dev, offsetof(struct pci_common_hdr, vendid));

    if (tmp == 0xffffffff) {
        ret = 2;
        goto go_and_free;
    }

    u8 htype = pci_dev->header_type = pci_dev->readb(pci_dev, offsetof(struct pci_hdr, common.htype));

    pci_dev->bus     = parent;
    pci_dev->devfn   = (dev << 3) | (func & 7);

    pci_dev->class   = pci_dev->readb(pci_dev, offsetof(struct pci_hdr, common.clas));
    pci_dev->device  = tmp >> 16;
    pci_dev->vendor  = tmp & 0xffff;
    pci_dev->sibling = NULL;
    pci_dev->next    = NULL;

    if ((htype & 0x7f) == 1)
        alloc_new_bus(parent, pci_dev);

    if (parent->devices == NULL) {
        parent->devices = pci_dev;
        return ret;
    }

    struct pci_dev *endpoint = parent->devices;
    while (endpoint->sibling)
        endpoint = endpoint->sibling;

    endpoint->sibling = pci_dev;
    return ret;

go_and_free:
    kfree(pci_dev);
    return ret;
}

void enumerate_pcibus(struct pci_bus *bus) {
    struct pci_dev pci_dev;
    pci_dev._bus = bus->primary;

    for (u8 device = 0; device < 32; device++) {
        pci_dev._slot = device;
        for (u8 fn = 0; fn < 8; fn++) {
            pci_dev._fn = fn;

            u8 htype = pci_readb(&pci_dev, offsetof(struct pci_hdr, common.htype));
            if (htype == 0xff)
                break;

            if (enumerate_pcidev(bus, device, fn) == 1) {
                printf("Failed to allocate memory for PCI device: %02x:%02x.%d", bus->primary, device, fn);
            }

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

static void lspci_cb(struct pci_dev *dev) {
    #ifdef DEBUG
        u8 subclass = dev->readb(dev, offsetof(struct pci_hdr, common.subclass));
        u8 progif = dev->readb(dev, offsetof(struct pci_hdr, common.prog_if));

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

    u8 subclass = dev->readb(dev, offsetof(struct pci_hdr, common.subclass));
    u8 progif = dev->readb(dev, offsetof(struct pci_hdr, common.prog_if));

    const char *vendor;
    const char *model;

    switch (dev->class & 0xFF) {
        case PCI_CLASS_NETWORK:
            #ifdef DEBUG
                vendor = nic_vendor_str(dev->vendor);
                model  = nic_model_str(dev->vendor, dev->device);

                if (vendor && model)
                    printf("  [%02x:%02x.%d] %s %s\n", bus, slot, fn, vendor, model);
                else if (vendor)
                    printf("  [%02x:%02x.%d] %s (device %04x, subclass %02x)\n",
                        bus, slot, fn, vendor, dev->device, subclass);
                else
                    printf("  [%02x:%02x.%d] Unknown NIC (%04x:%04x, subclass %02x)\n",
                        bus, slot, fn, dev->vendor, dev->device, subclass);
            #endif
            if (dev->vendor == 0x8086 && dev->device == 0x100E) {
                dev->enable_busmastering(dev);
                dev->enable_memory_space(dev);

                e1000_init(dev);
            }
            break;
        case PCI_CLASS_SBC:
            if (subclass != 0x3) break;

            dev->enable_busmastering(dev);
            dev->disable_bios_handle(dev);

            switch (progif) {
                case 0x0: // UHCI
                    #ifdef DEBUG
                        printf("  [%02x:%02x.%d] UHCI Controller\n",
                            bus, slot, fn);
                    #endif
                    uhci_start(uhci_create(dev)); // There isn't a working driver for uhci right now
                    printf("\n");
                    break;
            } break;
        case 0x01: // Mass storage controller
            if (subclass == 0x06) {
                #ifdef DEBUG
                    printf("  [%02x:%02x.%d] AHCI Controller\n",
                        bus, slot, fn);
                #endif
                ahci_init(dev); // This is also in... lets say development
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
