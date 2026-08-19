#ifndef MM_PMM_H
#define MM_PMM_H

#include <stdint.h>

#define PAGE_SIZE 4096ULL

/* Bitmap physical frame allocator, built once from the bootloader's
 * memory map (boot_get_memmap) and never consulting it again. Must run
 * after the boot_info accessors are available; nothing else needs to
 * happen first. */
void pmm_init(void);

/* Returns the physical address of a freshly allocated, zeroed 4KiB
 * frame, or 0 if none remain. 0 is never a valid frame address to
 * return on success -- physical page 0 is deliberately never handed out
 * (see pmm.c) so callers can use it as a real failure sentinel. */
uint64_t pmm_alloc_frame(void);

void pmm_free_frame(uint64_t phys_addr);

uint64_t pmm_free_frame_count(void);
uint64_t pmm_total_frame_count(void);

#endif
