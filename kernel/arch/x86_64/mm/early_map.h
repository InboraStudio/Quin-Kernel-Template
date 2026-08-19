#ifndef ARCH_X86_64_MM_EARLY_MAP_H
#define ARCH_X86_64_MM_EARLY_MAP_H

#include <stdint.h>

/* Minimal, Phase-1-only page-table editing, just enough to reach the two
 * fixed physical MMIO pages (LAPIC, IOAPIC) that CPU bring-up needs
 * before kernel/arch/x86_64/mm's real VMM exists (Phase 2). Firmware
 * memory maps generally don't list these addresses at all -- they're not
 * DRAM, so there's no HHDM alias to reach them through (verified against
 * this kernel's own QEMU/OVMF memmap: see docs/ARCHITECTURE.md).
 *
 * This is not a general-purpose mapping API. Phase 2's vmm_map replaces
 * it; nothing outside kernel/arch/x86_64/cpu (LAPIC/IOAPIC bring-up)
 * should call this. */

void early_map_init(void);

/* Maps one 4KiB physical page as uncacheable, supervisor, read+write at
 * an internally chosen virtual address and returns it. Panics if the
 * small static page-table-page pool (early_map.c) is exhausted --
 * expected to happen never, since only two pages are ever mapped this
 * way. */
void *early_map_mmio(uint64_t phys_addr);

#endif
