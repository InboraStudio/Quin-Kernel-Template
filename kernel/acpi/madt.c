#include "madt.h"

#include "acpi.h"
#include "kernel.h"
#include "lib/string.h"

/* ACPI spec 6.4, sect. 5.2.12 "Multiple APIC Description Table". Fixed
 * fields first, then a stream of variable-length sub-structures up to
 * header.length -- each starts with the same (type, length) pair, so
 * unrecognized types (interrupt source overrides, NMI sources, ...) can
 * always be skipped without understanding their contents. */
struct PACKED acpi_madt {
    struct acpi_table_header header;
    uint32_t local_apic_address;
    uint32_t flags;
};

struct PACKED madt_entry_header {
    uint8_t type;
    uint8_t length;
};

#define MADT_ENTRY_LOCAL_APIC 0
#define MADT_ENTRY_IOAPIC 1

#define LOCAL_APIC_FLAG_ENABLED (1U << 0)

struct PACKED madt_local_apic_entry {
    struct madt_entry_header header;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
};

struct PACKED madt_ioapic_entry {
    struct madt_entry_header header;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_address;
    uint32_t gsi_base;
};

bool madt_parse(struct madt_info *out) {
    memset(out, 0, sizeof(*out));

    struct acpi_table_header *table = acpi_find_table("APIC");
    if (table == NULL) {
        return false;
    }
    struct acpi_madt *madt = (struct acpi_madt *)table;

    out->local_apic_address = madt->local_apic_address;

    const uint8_t *cursor = (const uint8_t *)madt + sizeof(struct acpi_madt);
    const uint8_t *end = (const uint8_t *)madt + madt->header.length;

    while (cursor + sizeof(struct madt_entry_header) <= end) {
        const struct madt_entry_header *entry_header = (const struct madt_entry_header *)cursor;
        if (entry_header->length == 0 || cursor + entry_header->length > end) {
            break; /* malformed table -- stop rather than read past it */
        }

        if (entry_header->type == MADT_ENTRY_LOCAL_APIC) {
            const struct madt_local_apic_entry *lapic =
                (const struct madt_local_apic_entry *)cursor;
            if (lapic->flags & LOCAL_APIC_FLAG_ENABLED) {
                out->enabled_cpu_count++;
            }
        } else if (entry_header->type == MADT_ENTRY_IOAPIC && !out->have_ioapic) {
            const struct madt_ioapic_entry *ioapic = (const struct madt_ioapic_entry *)cursor;
            out->have_ioapic = true;
            out->ioapic_address = ioapic->ioapic_address;
            out->ioapic_gsi_base = ioapic->gsi_base;
        }

        cursor += entry_header->length;
    }

    return true;
}