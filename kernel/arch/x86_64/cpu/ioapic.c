#include "ioapic.h"

#include "arch/x86_64/mm/vmm.h"
#include "kernel.h"

/* OSDev Wiki: https://wiki.osdev.org/IOAPIC. The caller (kmain, via
 * kernel/acpi/madt.c) is responsible for supplying the actual base
 * address; this file no longer hardcodes the conventional 0xfec00000
 * default itself now that MADT parsing exists to find the real one --
 * see kmain's fallback logic if the MADT is unavailable. */

#define IOAPIC_REG_SELECT 0x00
#define IOAPIC_REG_WINDOW 0x10

#define IOAPIC_REG_VERSION 0x01
#define IOAPIC_REG_REDIRECTION(irq) (0x10 + 2 * (irq))

#define IOAPIC_REDIR_MASKED (1U << 16)

static volatile uint32_t *ioapic_base;

static uint32_t ioapic_read(uint32_t reg) {
    ioapic_base[IOAPIC_REG_SELECT / 4] = reg;
    return ioapic_base[IOAPIC_REG_WINDOW / 4];
}

static void ioapic_write(uint32_t reg, uint32_t value) {
    ioapic_base[IOAPIC_REG_SELECT / 4] = reg;
    ioapic_base[IOAPIC_REG_WINDOW / 4] = value;
}

void ioapic_init(uint64_t phys_base) {
    ioapic_base = (volatile uint32_t *)vmm_map_mmio(phys_base);

    uint32_t version = ioapic_read(IOAPIC_REG_VERSION);
    uint32_t max_entry = (version >> 16) & 0xff;

    for (uint32_t irq = 0; irq <= max_entry; irq++) {
        ioapic_write(IOAPIC_REG_REDIRECTION(irq), IOAPIC_REDIR_MASKED);
        ioapic_write(IOAPIC_REG_REDIRECTION(irq) + 1, 0);
    }
}

void ioapic_set_irq(uint8_t irq, uint8_t vector, uint8_t dest_apic_id, bool masked) {
    uint32_t low = vector;
    if (masked) {
        low |= IOAPIC_REDIR_MASKED;
    }
    uint32_t high = (uint32_t)dest_apic_id << 24;

    ioapic_write(IOAPIC_REG_REDIRECTION(irq) + 1, high);
    ioapic_write(IOAPIC_REG_REDIRECTION(irq), low);
}
