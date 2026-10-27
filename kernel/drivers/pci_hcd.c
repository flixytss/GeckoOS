#include "drivers/pci.h"

#define PARAMETERS(dev) dev->bus->primary, PCI_DEV(dev->devfn), PCI_FN(dev->devfn)

void pci_dev_disable_bios(struct pci_dev* dev)
    { pci_writew(PARAMETERS(dev), 0xC0, 0x2000); }
void pci_dev_enable_busmastering(struct pci_dev* dev)
    { pci_writew(PARAMETERS(dev), 0x4, 0x5); }