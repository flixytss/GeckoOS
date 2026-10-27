#pragma once

#include "pci.h"

void pci_dev_disable_bios(struct pci_dev* dev);
void pci_dev_enable_busmastering(struct pci_dev* dev);