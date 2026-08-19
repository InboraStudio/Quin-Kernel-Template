#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <stdint.h>

/* Plain, protocol-agnostic descriptions of what the bootloader handed us.
 * kernel/arch/<arch>/boot/ is responsible for filling these in from
 * whatever boot protocol that architecture uses (Limine on x86_64 today);
 * everything else in the kernel -- drivers, mm, acpi -- reads only these
 * structs and never includes a bootloader-specific header. That boundary
 * is the seam docs/ROADMAP.md points to for porting to another arch or
 * boot protocol later. */

struct boot_framebuffer {
    void *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
};

#define BOOT_MEMMAP_USABLE 0
#define BOOT_MEMMAP_RESERVED 1
#define BOOT_MEMMAP_ACPI_RECLAIMABLE 2
#define BOOT_MEMMAP_ACPI_NVS 3
#define BOOT_MEMMAP_BAD_MEMORY 4
#define BOOT_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define BOOT_MEMMAP_EXECUTABLE_AND_MODULES 6
#define BOOT_MEMMAP_FRAMEBUFFER 7
#define BOOT_MEMMAP_RESERVED_MAPPED 8

struct boot_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct boot_memmap {
    const struct boot_memmap_entry *entries;
    uint64_t count;
};

/* Kernel physical/virtual load addresses: the ELF is linked at a fixed
 * virtual base (KERNEL_VMA in linker.ld) but its physical placement is
 * chosen by the bootloader, so any code translating between a kernel .c
 * global's virtual address and its physical address (early page-table
 * bootstrapping, before the real VMM exists) needs both. */
struct boot_kernel_address {
    uint64_t physical_base;
    uint64_t virtual_base;
};

#endif
