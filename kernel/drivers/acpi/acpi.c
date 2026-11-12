#include "drivers/acpi/acpi-driver.h"
#include "mem/paging.h"
#include "ports.h"
#include "terminal/printf.h"

const struct acpi_header* map_full_header(void* header) {
    const struct acpi_header* header_m = (const struct acpi_header*)mmio_map((u64)header, sizeof(struct acpi_header));
    header_m = (const struct acpi_header*)mmio_map((u64)header, header_m->length);
    return header;
}

void acpi_aml_init(struct acpi* acpi) {
    const struct acpi_header* dsdt = map_full_header((void*)(size_t)acpi->fadt->dsdt);

    

}