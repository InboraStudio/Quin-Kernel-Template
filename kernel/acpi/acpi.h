#ifndef ACPI_ACPI_H
#define ACPI_ACPI_H

#include <stdbool.h>
#include <stdint.h>

#include "kernel.h"

/* ACPI System Description Table Header -- every ACPI table (RSDT, XSDT,
 * MADT, ...) starts with exactly this. ACPI spec 6.4 sect. 5.2.6. */
struct PACKED acpi_table_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};

/* Validates the RSDP, walks to the XSDT (or RSDT on the ACPI 1.0
 * systems this template will never actually run on, but the fallback
 * costs little), and remembers it for acpi_find_table. Requires
 * boot_get_rsdp() to succeed and pmm/vmm to already be initialized (the
 * RSDP and every table it leads to are read through HHDM). Panics if
 * the RSDP or root table's checksum doesn't validate -- a corrupt ACPI
 * table is not something to silently continue past. */
void acpi_init(void);

/* Finds a table by its 4-character signature (e.g. "APIC" for the
 * MADT), returning NULL if not present. The header's own `length`
 * field bounds how much of the table is safe to read after it. */
struct acpi_table_header *acpi_find_table(const char *signature);

#endif
