#ifndef DRIVERS_FRAMEBUFFER_H
#define DRIVERS_FRAMEBUFFER_H

#include <stdbool.h>
#include <stdint.h>

#include "boot_info.h"

/* 0xRRGGBB, converted to the framebuffer's native pixel format internally.
 * Not the raw pixel value -- callers never need to know the mode's bit
 * layout. */
#define FB_COLOR(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

#define FB_WHITE FB_COLOR(0xff, 0xff, 0xff)
#define FB_BLACK FB_COLOR(0x00, 0x00, 0x00)
#define FB_RED FB_COLOR(0xff, 0x40, 0x40)
#define FB_YELLOW FB_COLOR(0xff, 0xd0, 0x40)
#define FB_GREEN FB_COLOR(0x40, 0xff, 0x80)
#define FB_GRAY FB_COLOR(0xa0, 0xa0, 0xa0)

/* Returns false if no framebuffer was reported by the bootloader; callers
 * must fall back to serial-only output in that case. */
bool fb_init(const struct boot_framebuffer *info);

void fb_clear(uint32_t color_rgb);

/* Text console over the raw framebuffer: tracks a cursor in character
 * cells, wraps at the right edge, and scrolls the whole framebuffer up by
 * one cell row when the bottom is reached. This is what the leveled
 * logger (kernel/lib/log.c, added in Phase 3) writes through. */
void fb_console_putc(char c, uint32_t fg_rgb, uint32_t bg_rgb);
void fb_console_write(const char *str, uint32_t fg_rgb, uint32_t bg_rgb);

#endif
