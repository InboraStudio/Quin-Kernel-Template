#ifndef ARCH_X86_64_MM_VMM_H
#define ARCH_X86_64_MM_VMM_H

#include <stdbool.h>
#include <stdint.h>

/* Abstract mapping flags -- vmm.c translates these to the actual x86_64
 * PTE bits. Callers never write a raw PTE; that keeps the encoding (and
 * the EFER.NXE dependency for VMM_NO_EXECUTE) in exactly one place. */
#define VMM_WRITABLE (1U << 0)
#define VMM_USER (1U << 1)
#define VMM_NO_CACHE (1U << 2)
#define VMM_NO_EXECUTE (1U << 3)

/* Requires pmm_init() to have already run: new page-table pages come
 * from pmm_alloc_frame(). Continues extending the page tables Limine
 * already built (walked via CR3 + HHDM) rather than switching to a
 * fresh address space -- see docs/ARCHITECTURE.md, "Virtual memory", for
 * why that's a deliberate choice and not a shortcut. */
void vmm_init(void);

/* Maps a single 4KiB page. Returns false if a new page-table page was
 * needed and the PMM is out of frames. */
bool vmm_map(uint64_t virt, uint64_t phys, uint32_t flags);

void vmm_unmap(uint64_t virt);

/* Registers [start, end) (both page-aligned) as backed lazily: a
 * not-present page fault landing inside it allocates a fresh frame and
 * maps it with `flags` instead of panicking. Returns false if the fixed
 * lazy-region table (vmm.c) is full. */
bool vmm_register_lazy_region(uint64_t start, uint64_t end, uint32_t flags);

/* Called by the page fault ISR (vmm.c registers vector 14 itself in
 * vmm_init). Returns true if `fault_addr` fell inside a registered lazy
 * region and was resolved; false means it's a real fault and the caller
 * should panic. */
bool vmm_handle_page_fault(uint64_t fault_addr, uint64_t error_code);

/* Maps `page_count` contiguous pages starting at a fresh, kernel-owned
 * virtual address, with one unmapped guard page immediately before and
 * after the range so an off-by-one read/write faults instead of
 * silently corrupting a neighboring allocation. Returns NULL on
 * failure. Physical backing is allocated from the PMM, not lazy. */
void *vmm_alloc_guarded(uint64_t page_count, uint32_t flags);

/* Maps one fixed-purpose MMIO page (uncacheable, never demand-paged) and
 * returns its virtual address. Used by LAPIC/IOAPIC bring-up. */
void *vmm_map_mmio(uint64_t phys_addr);

#endif
