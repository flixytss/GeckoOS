#include "drivers/acpi/acpi-driver.h"
#include "boot/multiboot2.h"
#include "drivers/vga.h"
#include "mem/mmu.h"
#include "mem/paging.h"
#include "ports.h"
#include "terminal/terminal.h"
#include "sys/types.h"
#include <string.h>
#include <terminal/printf.h>
#include <arch/x86_64/irq.h>
#include <drivers/pit.h>

struct acpi* global_acpi;

// Helper
u8 verify_checksum(struct acpi_header* header) {
    u8 sum = 0;

    for (int i = 0; i < header->length; i++)
        sum += ((u8*)header)[i];

    return sum == 0;
}

void *find_rsdp()
{
    extern struct multiboot2_tag_acpi* acpi_info_grub;

    // That tag from grub can be NULL
    // If that is the case, this will find the RSDP manually in memory

    if (!acpi_info_grub) goto manual;

    return acpi_info_grub->rsdp;
manual:
    for (u64 p = 0x000E0000; p < 0x000FFFFF; p += 16)
        if (*(u64*)p == 0x5253442050545220) return (void*)p;
    // If nothing was found, keep searching from 0x40E to 0x40E + 1KIB (EBDA memory),
    for (u64 p = (0x40E * 0x10) & 0x000FFFFF; p < 0x40E + 1024; p += 0x10 / sizeof(p))
        if (*(u64*)p == 0x5253442050545220) return (void*)p;
nothing:
    set_printf_color(VGA_COLOR_RED);
        printf("No ACPI tag found!\n");
    set_printf_color(VGA_COLOR_WHITE);
    return NULL;
}

// Find MADT via XSDT (ACPI 2.0+)
static struct acpi_madt *find_madt_via_xsdt(u64 xsdt_phys)
{
    // Map just the header first to get length
    struct acpi_header *header =
        (struct acpi_header *)mmio_map(xsdt_phys, sizeof(struct acpi_header));
    if (!header || *(u32*)header->signature != 0x54445358) {
        return NULL;
    }

    u32 length = header->length;
    mmio_unmap((uintptr_t)header, sizeof(struct acpi_header));

    // Map the entire XSDT
    struct acpi_header *xsdt =
        (struct acpi_header *)mmio_map(xsdt_phys, length);
    if (!xsdt)
        return NULL;

    // Number of entries (each is 8 bytes for XSDT)
    int count = (xsdt->length - sizeof(struct acpi_header)) / 8;
    u64 *entries =
        (u64 *)((u8 *)xsdt + sizeof(struct acpi_header));

    struct acpi_madt *madt = NULL;

    for (int i = 0; i < count; i++) {
        // Map each table header to check signature
        struct acpi_header *hdr = (struct acpi_header *)mmio_map(
            entries[i], sizeof(struct acpi_header));
        if (!hdr)
            continue;

        if (*(u32*)hdr->signature == 0x43495041) {
            // Found MADT - map the entire table
            u32 madt_len = hdr->length;
            mmio_unmap((uintptr_t)hdr, sizeof(struct acpi_header));

            madt = (struct acpi_madt *)mmio_map(entries[i], madt_len);
            break;
        }

        mmio_unmap((uintptr_t)hdr, sizeof(struct acpi_header));
    }

    mmio_unmap((uintptr_t)xsdt, length);
    return madt;
}

// Find MADT via RSDT (ACPI 1.0)
static struct acpi_madt *find_madt_via_rsdt(u32 rsdt_phys)
{
    // Map just the header first
    struct acpi_header *header =
        (struct acpi_header *)mmio_map(rsdt_phys, sizeof(struct acpi_header));
    if (!header || *(u32*)header->signature != 0x54445352) {
        return NULL;
    }

    u32 length = header->length;
    mmio_unmap((uintptr_t)header, sizeof(struct acpi_header));

    // Map the entire RSDT
    struct acpi_header *rsdt =
        (struct acpi_header *)mmio_map(rsdt_phys, length);
    if (!rsdt)
        return NULL;

    // Number of entries (each is 4 bytes for RSDT)
    int count = (rsdt->length - sizeof(struct acpi_header)) / 4;
    u32 *entries =
        (u32 *)((u8 *)rsdt + sizeof(struct acpi_header));

    struct acpi_madt *madt = NULL;

    for (int i = 0; i < count; i++) {
        // Map each table header to check signature
        struct acpi_header *hdr = (struct acpi_header *)mmio_map(
            entries[i], sizeof(struct acpi_header));
        if (!hdr)
            continue;

        if (*(u32*)hdr->signature == 0x43495041) {
            // Found MADT - map the entire table
            u32 madt_len = hdr->length;
            mmio_unmap((uintptr_t)hdr, sizeof(struct acpi_header));

            madt = (struct acpi_madt *)mmio_map(entries[i], madt_len);
            break;
        }

        mmio_unmap((uintptr_t)hdr, sizeof(struct acpi_header));
    }

    mmio_unmap((uintptr_t)rsdt, length);
    return madt;
}

// Parse MADT entries
static int parse_madt_entries(struct acpi* acpi)
{
    int isos_count = 0;
    while (acpi->isos[isos_count++]);

    if (!acpi->madt)
        return 0;

    // Map LAPIC base
    acpi->acpi_lapic_base = (void*)(size_t)acpi->madt->lapic_addr;

    #ifdef DEBUG
        set_printf_color(VGA_COLOR_DARK_GREY);
            printf("LAPIC is at %p\n", acpi->acpi_lapic_base);
        set_printf_color(VGA_COLOR_WHITE);
    #endif

    // Parse entries after the MADT header
    u8 *ptr = (u8 *)acpi->madt + sizeof(struct acpi_madt);
    u8 *end = (u8 *)acpi->madt + acpi->madt->header.length;

    int found_ioapic = 0;

    while (ptr < end) {
        struct madt_entry_header *h = (struct madt_entry_header *)ptr;

        // Sanity check
        if (h->length == 0 || ptr + h->length > end)
            break;

        switch (h->type) {
            case 1: { // IOAPIC
                struct madt_ioapic *io = (struct madt_ioapic *)ptr;
                acpi->acpi_ioapic_base = (void*)(size_t)io->addr;
                #ifdef DEBUG
                    set_printf_color(VGA_COLOR_DARK_GREY);
                        printf("IOAPIC found: addr=%p, gsi_base=%u\n", acpi->acpi_ioapic_base,
                            io->gsi_base);
                    set_printf_color(VGA_COLOR_WHITE);
                #endif
                found_ioapic = 1;
                break;
            }

            case 2: { // ISO (Interrupt Source Override)
                acpi->isos[isos_count++] = (struct madt_iso *)ptr;
                break;
            }
            default:
                break;
        }

        ptr += h->length;
    }

    return found_ioapic;
}

// Function that does the init for acpi and other stuff that needs parsing from
// mbi_addr including lapic and ioapic
struct acpi* acpi_init()
{
    struct acpi_madt *madt = NULL;
    struct acpi_rsdp_v2 *rsdp_v2;
    struct acpi_header *rsdt_header;
    struct acpi* acpi = kmalloc(sizeof(*acpi));
    if (!acpi) return NULL;
    memset(acpi, 0, sizeof(*acpi));


    struct acpi_rsdp_v1 *rsdp = (struct acpi_rsdp_v1 *)find_rsdp();
    if (!rsdp) {
        set_printf_color(VGA_COLOR_RED);
            printf("FAIL: No RSDP found\n");
        set_printf_color(VGA_COLOR_WHITE);
        return NULL;
    }

    // Try XSDT first v2 if revision exists
    if (rsdp->revision >= 2)
        goto revision2;
    // Try RSDT fallback
    if (rsdp->rsdt_addr != 0)
        goto revision1;

revision2:
    rsdp_v2 = (struct acpi_rsdp_v2 *)rsdp;
    if (rsdp_v2->xsdt_addr != 0) {
        // Map XSDT header
        struct acpi_header *xsdt_header = (struct acpi_header *)mmio_map(
            rsdp_v2->xsdt_addr, sizeof(struct acpi_header));

        if (!xsdt_header || !verify_checksum(xsdt_header)) {
            set_printf_color(VGA_COLOR_RED);
                printf("FAIL: Could not map XSDT header\n");
            set_printf_color(VGA_COLOR_WHITE);
        } else {
            if (*(u32*)xsdt_header->signature == 0x54445358) {
                // Map full XSDT
                struct acpi_header *xsdt = (struct acpi_header *)mmio_map(
                    rsdp_v2->xsdt_addr, xsdt_header->length);
                if (xsdt) {
                    int count =
                        (xsdt->length - sizeof(struct acpi_header)) / 8;

                    u64 *entries =
                        (u64 *)((u8 *)xsdt +
                                    sizeof(struct acpi_header));
                    for (int i = 0; i < count; i++) {

                        // Map just the header of each table
                        struct acpi_header *hdr =
                            (struct acpi_header *)mmio_map(
                                entries[i], sizeof(struct acpi_header));
                        if (!hdr || !verify_checksum(hdr)) {
                            set_printf_color(VGA_COLOR_RED);
                                printf("FAIL: Could notmap header\n");
                            set_printf_color(VGA_COLOR_WHITE);
                            continue;
                        }
                        u32 length = hdr->length;
                        switch (*(u32*)hdr->signature) {
                            case 0x43495041: // MADT
                                mmio_unmap((uintptr_t)hdr,
                                    sizeof(struct acpi_header));
                                // Map full MADT
                                madt = (struct acpi_madt *)mmio_map(
                                    entries[i], length);
                                break;
                            case 0x50434141: // FADT
                                mmio_unmap((uintptr_t)hdr,
                                    sizeof(struct acpi_header));
                                // Map full FADT
                                acpi->fadt = (acpi_fadt_t *)mmio_map(
                                    entries[i], length);
                                break;
                            default:
                                break;
                        }

                        // Headers are only mapped not unmapped memory
                        // deallocation is for losers
                    }
                }
            } else {
                set_printf_color(VGA_COLOR_RED);
                    printf("Invalid XSDT signature\n");
                set_printf_color(VGA_COLOR_WHITE);
            }
        }
    }

    goto end;
revision1:
    rsdt_header = (struct acpi_header *)mmio_map(
        rsdp->rsdt_addr, sizeof(struct acpi_header));

    if (rsdt_header) {
        if (*(u32*)rsdt_header->signature == 0x54445352) {
            struct acpi_header *rsdt = (struct acpi_header *)mmio_map(
                rsdp->rsdt_addr, rsdt_header->length);
            if (rsdt) {
                int count = (rsdt->length - sizeof(struct acpi_header)) / 4;

                u32 *entries =
                    (u32 *)((u8 *)rsdt +
                                    sizeof(struct acpi_header));

                for (int i = 0; i < count; i++) {
                    struct acpi_header *hdr =
                        (struct acpi_header *)mmio_map(
                            entries[i], sizeof(struct acpi_header));
                    if (!hdr)
                        continue;
                    switch (*(u32*)hdr->signature) {
                        case 0x43495041: // MADT
                            madt = (struct acpi_madt *)mmio_map(
                                entries[i], hdr->length);
                            break;
                        case 0x50434146: // FADT
                            acpi->fadt = (acpi_fadt_t *)mmio_map(
                                entries[i], hdr->length);
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }
end:
    if (!madt) {
        set_printf_color(VGA_COLOR_LIGHT_RED);
            printf("\nFAIL: MADT not found in any table\n");
            printf("This means either:\n");
            printf("  1. ACPI tables are corrupted\n");
            printf("  2. Memory mapping is failing\n");
            printf("  3. Your MMIO mapping doesn't work for physical addresses > "
                "4GB\n");
            printf("  4. find_rsdp() returned a pointer to unmapped memory\n");
        set_printf_color(VGA_COLOR_WHITE);
        return NULL;
    }
    acpi->madt = madt;

    printc("MADT found successfully!\n", VGA_COLOR_LIGHT_GREY);

    parse_madt_entries(acpi);

    return acpi;
}

int acpi_enable(struct acpi* acpi) {
    // Check if ACPI is already enabled
    if (acpi->fadt->smiCommandPort == 0)
        if (acpi->fadt->acpiEnable == 0 && acpi->fadt->acpiDisable == 0)
            return 0;
    if (inw(acpi->fadt->pm1aControlBlk) & 1)
        return 0;

    // Activate ACPI
    outb(acpi->fadt->smiCommandPort, acpi->fadt->acpiEnable);

    pit_timer_wait_s(3);
    while (!(inw(acpi->fadt->pm1aControlBlk) & 1)) HALT();

    // irq_install_handler(acpi->fadt->sciInterrupt, acpi_irq_handler, 0);

    return 0;
}

#if 0
void acpi_irq_handler(registers_t *regs) {
    printf("REGS \n");
}
#endif