#ifndef ARCH_X86_64_CPU_IOAPIC_H
#define ARCH_X86_64_CPU_IOAPIC_H

#include <stdbool.h>
#include <stdint.h>

/* Requires vmm_init() to have already run. Masks every redirection
 * entry; nothing is routed until ioapic_set_irq is called. `phys_base`
 * should come from ACPI MADT parsing (kernel/acpi/madt.h) when
 * available; kmain falls back to the conventional 0xfec00000 default if
 * the MADT couldn't be parsed. */
void ioapic_init(uint64_t phys_base);

/* Routes legacy IRQ line `irq` (0-15) to interrupt vector `vector`,
 * targeting the given LAPIC ID, edge-triggered/active-high (the legacy
 * ISA default -- correct for the PIT and PS/2 controller this kernel
 * currently cares about; a driver for a level-triggered PCI IRQ would
 * need a variant of this, not added since nothing needs it yet). */
void ioapic_set_irq(uint8_t irq, uint8_t vector, uint8_t dest_apic_id, bool masked);

#endif
