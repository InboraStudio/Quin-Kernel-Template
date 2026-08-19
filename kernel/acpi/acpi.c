#include "acpi.h"

#include "arch/x86_64/boot/limine_requests.h"
#include "arch/x86_64/cpu/panic.h"
#include "kernel.h"
#include "lib/string.h"

/* ACPI spec 6.4, sect. 5.2.5.3 "Root System Description Pointer (RSDP)
 * Structure". The first 20 bytes are the ACPI 1.0 layout (checksummed
 * separately); everything from `length` on was added in ACPI 2.0 and is
 * covered by `extended_checksum` instead, which sums the *entire*
 * 36-byte structure. */
struct PACKED acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
};

static uint64_t hhdm_offset;
static void *root_table;
static uint32_t root_entry_count;
static bool root_entries_are_64bit;

static void *phys_to_virt(uint64_t phys) {
    return (void *)(uintptr_t)(phys + hhdm_offset);
}

static bool checksum_ok(const void *data, uint32_t length) {
    const uint8_t *bytes = data;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum = (uint8_t)(sum + bytes[i]);
    }
    return sum == 0;
}

void acpi_init(void) {
    hhdm_offset = boot_get_hhdm_offset();

    void *rsdp_ptr;
    if (!boot_get_rsdp(&rsdp_ptr)) {
        panic("acpi: bootloader did not answer the RSDP request");
    }
    struct acpi_rsdp *rsdp = rsdp_ptr;

    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
        panic("acpi: RSDP signature mismatch");
    }
    if (!checksum_ok(rsdp, 20)) {
        panic("acpi: RSDP (v1) checksum mismatch");
    }

    if (rsdp->revision >= 2 && checksum_ok(rsdp, sizeof(struct acpi_rsdp))) {
        struct acpi_table_header *xsdt = phys_to_virt(rsdp->xsdt_address);
        if (memcmp(xsdt->signature, "XSDT", 4) != 0 || !checksum_ok(xsdt, xsdt->length)) {
            panic("acpi: XSDT signature or checksum mismatch");
        }
        root_table = xsdt;
        root_entry_count = (xsdt->length - (uint32_t)sizeof(struct acpi_table_header)) / 8;
        root_entries_are_64bit = true;
    } else {
        /* ACPI 1.0 fallback: every table pointer in the RSDT is 32-bit.
         * QEMU/OVMF always advertise ACPI 2.0+, so this path exists for
         * completeness rather than because it's exercised in this
         * template's own CI. */
        struct acpi_table_header *rsdt = phys_to_virt(rsdp->rsdt_address);
        if (memcmp(rsdt->signature, "RSDT", 4) != 0 || !checksum_ok(rsdt, rsdt->length)) {
            panic("acpi: RSDT signature or checksum mismatch");
        }
        root_table = rsdt;
        root_entry_count = (rsdt->length - (uint32_t)sizeof(struct acpi_table_header)) / 4;
        root_entries_are_64bit = false;
    }
}

struct acpi_table_header *acpi_find_table(const char *signature) {
    const uint8_t *entries = (const uint8_t *)root_table + sizeof(struct acpi_table_header);

    for (uint32_t i = 0; i < root_entry_count; i++) {
        uint64_t table_phys;
        if (root_entries_are_64bit) {
            uint64_t entry;
            memcpy(&entry, entries + i * 8, sizeof(entry));
            table_phys = entry;
        } else {
            uint32_t entry;
            memcpy(&entry, entries + i * 4, sizeof(entry));
            table_phys = entry;
        }

        struct acpi_table_header *table = phys_to_virt(table_phys);
        if (memcmp(table->signature, signature, 4) == 0) {
            if (!checksum_ok(table, table->length)) {
                panic("acpi: %s table checksum mismatch", signature);
            }
            return table;
        }
    }
    return NULL;
}