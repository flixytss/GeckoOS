#ifndef PCI_H
#define PCI_H

#include "sys/types.h"

#define PCI_DEV(devfn) ((devfn) >> 3)
#define PCI_FN(devfn)  ((devfn) & 7)

#define PCI_CLASS_NETWORK   0x02
#define PCI_CLASS_SBC       0x0C

#define BAR0_OFF 0x10
#define BAR_SIZE 0x04

#define PCI_HDR_DEV_ILINE_OFFSET 0x3C

struct pci_bus {
    struct pci_bus *parent;
    struct pci_bus *children;
    struct pci_bus *next;

    struct pci_dev *self;
    struct pci_dev *devices;

    void *sysdata;

    u8 bus;
    u8 primary;
    u8 secondary;
    u8 subordinate;
};

struct pci_dev {
    struct pci_bus *bus;
    struct pci_dev *sibling;
    struct pci_dev *next;

    void *sysdata;

    u32 devfn;
    u16 vendor;
    u16 device;
    u32 class;
    u32 master : 1;

    u8 irq;
};

struct pci_common_hdr {
    u16 vendid;
    u16 devid;
    u16 cmd;
    u16 status;
    u8  revid;
    u8  prog_if;
    u8  subclass;
    u8  clas; // clas :O
    u8  clz;
    u8  lt;
    u8  htype;
    u8  bist;
} __attribute__((packed));

struct pci_hdr_dev {
    u32 bar[6];
    u32 cardptr;
    u16 subvendid;
    u16 subid;
    u32 erombs;
    u8  capptr;
    u8  res[7];
    u8  iline;
    u8  ipin;
    u8  mingrant;
    u8  maxlat;
} __attribute__((packed));

struct pci_hdr_brd {
    u32 bar[2];
    u8  p_busnum;
    u8  s_busnum;
    u8  ss_busnum;
    u8  s_lt;
    u8  iobase;
    u8  iolim;
    u16 s_stat;
    u16 membase;
    u16 memlim;
    u16 pmembase;
    u16 pmemlim;
    u32 pupperbase;
    u32 pupperlim;
    u16 ioupperbase;
    u16 ioupperlim;
    u8  capptr;
    u8  res[3];
    u32 erombs;
    u8  iline;
    u8  ipin;
    u16 bctrl;
} __attribute__((packed));

struct pci_hdr_crd {
    u32 cardbus_addr;
    u8  cl_offset;
    u8  res;
    u16 s_stat;
    u8  pcibusnum;
    u8  cbusnum;
    u8  ss_busnum;
    u8  cbus_lt;
    u32 membase0;
    u32 memlim0;
    u32 membase1;
    u32 memlim1;
    u32 iobase0;
    u32 iolim0;
    u32 iobase1;
    u32 iolim1;
    u8  iline;
    u8  ipin;
    u16 bctrl;
} __attribute__((packed));

struct pci_hdr {
    struct pci_common_hdr common;
    union {
        struct pci_hdr_dev dev;
        struct pci_hdr_brd bridge;
        struct pci_hdr_crd card_bridge;
    };
} __attribute__((packed));

struct pci_header0 {
    struct pci_common_hdr common;
    u32 bar0;
    u32 bar1;
    u8 primary_bus;
    u8 secondary_bus;
    u8 subordinate_bus;
    u8 secondary_lt;
    u8 I0base;
    u8 IOlimit;
    u16 secondary_status;
    u16 memory_base;
    u16 memory_limit;
    u16 prefetchable_membase;
    u16 prefetchable_memlimit;
    u32 prefetchable_membase_upper;
    u32 prefetchable_memlimit_upper;
    u16 IObase_upper;
    u16 IOlimit_upper;
    u8 capability_pointer;
    char reserved[3];
    u32 expansion_ROM;
    u8 interrupt_line;
    u8 interrupt_pin;
    u16 bridge_control;
} __attribute__((packed));

struct PCIBar {
    union {
        void* address;
        u16 port;
    } u;
    u64 size;
    u32 flags;
};

struct pci_device_shortcut {
    u32 bus;
    u32 slot;
    u32 func;
};

u32 pci_readl(u32 bus, u32 slot, u32 func, u8 off);
u16 pci_readw(u32 bus, u32 slot, u32 func, u8 off);
u8  pci_readb(u32 bus, u32 slot, u32 func, u8 off);
void     pci_writel(u32 bus, u32 slot, u32 func, u32 off, u32 data);
void     pci_writew(u32 bus, u32 slot, u32 func, u32 off, u16 data);
void     pci_writeb(u32 bus, u32 slot, u32 func, u32 off, u8 data);

u8 enumerate_pcidev(struct pci_bus *parent, u8 dev, u8 func);
void    enumerate_pcibus(struct pci_bus *bus);
void    enumerate_pci();

void pci_lspci();
void pci_detect_controllers();

const char *nic_vendor_str(u16 vendor);

u32 pci_get_bar(u32 bus, u32 slot, u32 func, u8 index);

#endif