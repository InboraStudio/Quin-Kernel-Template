#include "pmm.h"

#include "arch/x86_64/boot/limine_requests.h"
#include "arch/x86_64/cpu/panic.h"
#include "kernel.h"
#include "lib/bitmap.h"
#include "lib/string.h"

/* Bit set = frame in use (or simply untracked, e.g. reserved/MMIO). Bit
 * clear = free. Starting every frame as "in use" and only clearing the
 * ones inside a usable memmap region means anything the bootloader
 * didn't explicitly call out as usable -- reserved, ACPI, MMIO gaps, bad
 * memory -- is never handed out, with no separate exclusion list to
 * maintain. The bit-twiddling itself lives in kernel/lib/bitmap.c,
 * exercised directly by tests/unit/test_bitmap.c without needing QEMU;
 * everything here is the boot-time integration around it. */
static uint8_t *bitmap;
static uint64_t frame_count;
static uint64_t next_search_index;
static uint64_t hhdm_offset;
static uint64_t free_frames;

static void *phys_to_virt(uint64_t phys) {
    return (void *)(uintptr_t)(phys + hhdm_offset);
}

void pmm_init(void) {
    hhdm_offset = boot_get_hhdm_offset();

    struct boot_memmap memmap;
    if (!boot_get_memmap(&memmap)) {
        panic("pmm: bootloader did not answer the memory map request");
    }

    /* Sized off RAM-backed regions only (usable, bootloader-reclaimable,
     * the kernel's own executable-and-modules region) -- QEMU's memmap
     * also reports a large `Reserved` entry for the 64-bit PCIe MMIO
     * window far above any real RAM (tens of GiB on a 256MiB guest), and
     * including that would inflate the bitmap by three orders of
     * magnitude to track frames that can never be allocated anyway. */
    uint64_t highest_addr = 0;
    for (uint64_t i = 0; i < memmap.count; i++) {
        uint64_t type = memmap.entries[i].type;
        if (type != BOOT_MEMMAP_USABLE && type != BOOT_MEMMAP_BOOTLOADER_RECLAIMABLE &&
            type != BOOT_MEMMAP_EXECUTABLE_AND_MODULES) {
            continue;
        }
        uint64_t end = memmap.entries[i].base + memmap.entries[i].length;
        if (end > highest_addr) {
            highest_addr = end;
        }
    }
    frame_count = highest_addr / PAGE_SIZE;
    uint64_t bitmap_bytes = align_up(frame_count, 8) / 8;

    uint64_t bitmap_phys = 0;
    for (uint64_t i = 0; i < memmap.count; i++) {
        const struct boot_memmap_entry *entry = &memmap.entries[i];
        if (entry->type == BOOT_MEMMAP_USABLE && entry->length >= bitmap_bytes) {
            bitmap_phys = entry->base;
            break;
        }
    }
    if (bitmap_phys == 0) {
        panic("pmm: no usable region large enough for the frame bitmap (%lu bytes)", bitmap_bytes);
    }

    bitmap = phys_to_virt(bitmap_phys);
    memset(bitmap, 0xff, bitmap_bytes);

    for (uint64_t i = 0; i < memmap.count; i++) {
        if (memmap.entries[i].type != BOOT_MEMMAP_USABLE) {
            continue;
        }
        uint64_t start_frame = memmap.entries[i].base / PAGE_SIZE;
        uint64_t end_frame = (memmap.entries[i].base + memmap.entries[i].length) / PAGE_SIZE;
        for (uint64_t frame = start_frame; frame < end_frame; frame++) {
            bitmap_clear(bitmap, frame);
            free_frames++;
        }
    }

    /* The bitmap occupies part of a region it just marked free -- claim
     * those frames back. Physical page 0 is reserved permanently so a
     * frame's physical address is never mistakable for a null pointer. */
    uint64_t bitmap_start_frame = bitmap_phys / PAGE_SIZE;
    uint64_t bitmap_end_frame = bitmap_start_frame + align_up(bitmap_bytes, PAGE_SIZE) / PAGE_SIZE;
    for (uint64_t frame = bitmap_start_frame; frame < bitmap_end_frame; frame++) {
        if (!bitmap_test(bitmap, frame)) {
            bitmap_set(bitmap, frame);
            free_frames--;
        }
    }
    if (!bitmap_test(bitmap, 0)) {
        bitmap_set(bitmap, 0);
        free_frames--;
    }
}

uint64_t pmm_alloc_frame(void) {
    uint64_t frame = bitmap_find_first_clear(bitmap, frame_count, next_search_index);
    if (frame == frame_count) {
        return 0;
    }

    bitmap_set(bitmap, frame);
    free_frames--;
    next_search_index = frame + 1;

    uint64_t phys = frame * PAGE_SIZE;
    memset(phys_to_virt(phys), 0, PAGE_SIZE);
    return phys;
}

void pmm_free_frame(uint64_t phys_addr) {
    uint64_t frame = phys_addr / PAGE_SIZE;
    if (frame >= frame_count || !bitmap_test(bitmap, frame)) {
        panic("pmm_free_frame: double free or invalid address 0x%lx", phys_addr);
    }
    bitmap_clear(bitmap, frame);
    free_frames++;
}

uint64_t pmm_free_frame_count(void) {
    return free_frames;
}

uint64_t pmm_total_frame_count(void) {
    return frame_count;
}
