#ifndef ACPI_MADT_H
#define ACPI_MADT_H

#include <stdbool.h>
#include <stdint.h>

struct madt_info {
    uint64_t local_apic_address; /* physical */
    uint32_t enabled_cpu_count;  /* Local APIC entries with the Enabled flag set */

    bool have_ioapic;
    uint64_t ioapic_address; /* physical */
    uint32_t ioapic_gsi_base;
};

/* Requires acpi_init() to have already run. Returns false if no MADT
 * ("APIC") table is present -- callers fall back to the hardcoded
 * defaults in lapic.c/ioapic.c in that case. */
bool madt_parse(struct madt_info *out);

#endif
