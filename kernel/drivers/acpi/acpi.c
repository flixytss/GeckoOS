#include "drivers/acpi/acpi-driver.h"
#include "mem/paging.h"
#include <gk/io.h>
#include "terminal/printf.h"
#include "terminal/terminal.h"

const struct acpi_header* map_full_header(void* header) {
    const struct acpi_header* header_m = (const struct acpi_header*)mmio_map((u64)header, sizeof(struct acpi_header));
    header_m = (const struct acpi_header*)mmio_map((u64)header, header_m->length);

    return header_m;
}

void acpi_aml_init(struct acpi* acpi) {
    const struct acpi_header* dsdt = map_full_header((void*)(size_t)acpi->fadt->dsdt);

    #ifdef DEBUG
        pretty_printf("aml length: %zu bytes\n", dsdt->length - sizeof(struct acpi_header));
    #endif

    const u8* raw = (u8*)dsdt + sizeof(struct acpi_header);

    const u8 pkg_length = raw[0];
}