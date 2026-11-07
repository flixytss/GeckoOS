#include "drivers/pci.h"
#include <drivers/mass_storage_controller/ahci.h>
#include "sys/types.h"
#include <terminal/printf.h>

#define ABAR_OFF 0x24

void ahci_init(struct pci_dev* device) {
    u32 Abar = pci_get_bar(device, 5);
    if (!Abar) goto err;

    u8* registers_address = (u8*)((u64)Abar & ~0xF); // 16-byte aligned address
    // vmm_map(vmm_get_pml4(), (u64)registers_address, (u64)registers_address, PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE);

    printf("%x AHCI Address\n", *registers_address);

err:
    return;
}