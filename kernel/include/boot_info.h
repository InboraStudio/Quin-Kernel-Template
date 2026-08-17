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

#endif
