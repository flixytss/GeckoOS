#include "drivers/apic/ioapic.h"
#include "drivers/acpi.h"
#include "drivers/apic/lapic.h"
#include "drivers/ps2keyboard.h"
#include "drivers/vga.h"
#include "mem/paging.h"
#include "terminal/terminal.h"
#include "terminal/printf.h"
#include "sys/types.h"

//the modified virtual adress by virtual mapping
volatile u32 *ioapic;
//the original physical base given by the madt table
extern uintptr_t acpi_ioapic_base;

//reading a register form the ioapic
u32 ioapic_read(u32 reg)
{
    ioapic[IOAPIC_IOREGSEL / 4] = reg;
    return ioapic[IOAPIC_IOWIN / 4];
}
//writing a register to the ioapic
void ioapic_write(u32 reg, u32 value)
{
    ioapic[IOAPIC_IOREGSEL / 4] = reg;
    ioapic[IOAPIC_IOWIN / 4]    = value;
}

void ioapic_set_entry(volatile u32 *base, u8 index, u64 data)
{
    ioapic_write(IOAPIC_REDTBL_BASE + index * 2, (u32)data);
    ioapic_write(IOAPIC_REDTBL_BASE + index * 2 + 1, (u32)(data >> 32));
}
//Debug function for outputing the data of an entry
#ifdef DEBUG
    void ioapic_dump_entry(int gsi) {
        set_printf_color(VGA_COLOR_MAGENTA);
            u64 entry = ioapic_read(IOAPIC_REDTBL_BASE + gsi * 2) |
                            ((u64)ioapic_read(IOAPIC_REDTBL_BASE + gsi * 2 + 1) << 32);
            printf("GSI %d: 0x%016lx | vector=%u | mask=%u | trigger=%u | polarity=%u | dest=%u\n",
                gsi, entry,
                (u32)(entry & 0xFF),            // Vector
                (u32)((entry >> 16) & 1),       // Mask (0 = unmasked)
                (u32)((entry >> 15) & 1),       // Trigger (0 = edge)
                (u32)((entry >> 13) & 1),       // Polarity (0 = high)
                (u32)(entry >> 56)              // Destination LAPIC ID
            );
        set_printf_color(VGA_COLOR_WHITE);
    }
#endif

void ioapic_init()
{

    // Map IOAPIC physical address to virtual memory
    if (!acpi_ioapic_base) {
        printc("No IOAPIC found in ACPI tables", VGA_COLOR_RED);
    }

    uintptr_t ioapic_virt = acpi_ioapic_base;

    vmm_map(vmm_get_pml4(), acpi_ioapic_base, ioapic_virt,
            PTE_WRITABLE | PTE_CACHE_DISABLE);

    ioapic = (volatile u32 *)ioapic_virt;

    // Get IOAPIC version and maximum redirection entries
    u32 version = ioapic_read(IOAPIC_REG_VER);
    u32 max_redir =(version >> 16) & 0xFF; // Max redirection entry index(pins on card)
    #ifdef DEBUG
        set_printf_color(VGA_COLOR_DARK_GREY);
            printf("IOAPIC: version=%x, max_redir=%u\n", version & 0xFF, max_redir);
        set_printf_color(VGA_COLOR_WHITE);
    #endif

    // Mask all redirection entries
    for (int i = 0; i <= max_redir; ++i) {
        ioapic_set_entry(ioapic, i, 1 << 16);
    }

    // u32 current_id = (ioapic_read(IOAPIC_REG_ID) >> 24) & 0xF;
    //printf("IOAPIC: current ID=%u\n", current_id);
    //printf("IOapci and lapic base:%x",ioapic_virt);
}

void ioapic_redirect_irq(int gsi, int vector, u16 flags)
{

    // Need to find which IOAPIC handles this GSI
    // For single IOAPIC, using GSI as pin
    u8 pin = gsi;

    u8 polarity = (flags & 0x3) == 3 ? 1 : 0;        // Active low if 3
    u8 trigger  = ((flags >> 2) & 0x3) == 3 ? 1 : 0; // Level if 3

    u64 entry =
        vector | (0 << 8) |                      // Delivery mode: Fixed
        (0 << 11) |                              // Destination mode: Physical
        ((u64)polarity << 13) | (0 << 14) | // Remote IRR (0 for now)
        ((u64)trigger << 15) | (0 << 16) |  // Masked? 0 = unmasked
        ((u64)lapic_get_id() << 56);                  
    
    ioapic_set_entry((u32 *)ioapic, pin, entry);
   
    // ioapic_dump_entry(gsi);
   
}
