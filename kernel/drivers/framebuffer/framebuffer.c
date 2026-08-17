#include "framebuffer.h"

#include "font8x8_basic.h"
#include "lib/string.h"

#define CELL_W 8
#define CELL_H 8

static struct boot_framebuffer fb;
static uint32_t bytes_per_pixel;
static uint32_t console_cols;
static uint32_t console_rows;
static uint32_t cursor_col;
static uint32_t cursor_row;

bool fb_init(const struct boot_framebuffer *info) {
    if (info == NULL || info->address == NULL) {
        return false;
    }

    fb = *info;
    bytes_per_pixel = (uint32_t)(fb.bpp / 8);
    console_cols = (uint32_t)(fb.width / CELL_W);
    console_rows = (uint32_t)(fb.height / CELL_H);
    cursor_col = 0;
    cursor_row = 0;
    return true;
}

/* Scales an 8-bit RGB component to whatever width the framebuffer's mode
 * actually uses, then places it at the mode-reported bit shift. Most QEMU
 * GOP modes are 8/8/8, where this is a no-op scale, but the framebuffer
 * feature response never guarantees that. */
static uint32_t pack_color(uint32_t rgb) {
    uint8_t r = (uint8_t)(rgb >> 16);
    uint8_t g = (uint8_t)(rgb >> 8);
    uint8_t b = (uint8_t)rgb;

    uint32_t rv = fb.red_mask_size < 8 ? (uint32_t)(r >> (8 - fb.red_mask_size)) : r;
    uint32_t gv = fb.green_mask_size < 8 ? (uint32_t)(g >> (8 - fb.green_mask_size)) : g;
    uint32_t bv = fb.blue_mask_size < 8 ? (uint32_t)(b >> (8 - fb.blue_mask_size)) : b;

    return (rv << fb.red_mask_shift) | (gv << fb.green_mask_shift) | (bv << fb.blue_mask_shift);
}

static void put_pixel_raw(uint32_t x, uint32_t y, uint32_t native_color) {
    uint8_t *row = (uint8_t *)fb.address + (uint64_t)y * fb.pitch;
    uint8_t *pixel = row + (uint64_t)x * bytes_per_pixel;
    for (uint32_t i = 0; i < bytes_per_pixel; i++) {
        pixel[i] = (uint8_t)(native_color >> (8 * i));
    }
}

void fb_clear(uint32_t color_rgb) {
    uint32_t native = pack_color(color_rgb);
    for (uint32_t y = 0; y < fb.height; y++) {
        for (uint32_t x = 0; x < fb.width; x++) {
            put_pixel_raw(x, y, native);
        }
    }
}

static void draw_glyph(uint32_t col, uint32_t row, char c, uint32_t fg_rgb, uint32_t bg_rgb) {
    uint32_t fg = pack_color(fg_rgb);
    uint32_t bg = pack_color(bg_rgb);
    const uint8_t *glyph = font8x8_basic[(uint8_t)c & 0x7f];

    uint32_t origin_x = col * CELL_W;
    uint32_t origin_y = row * CELL_H;

    for (uint32_t gy = 0; gy < CELL_H; gy++) {
        uint8_t bits = glyph[gy];
        for (uint32_t gx = 0; gx < CELL_W; gx++) {
            bool set = (bits & (1u << gx)) != 0;
            put_pixel_raw(origin_x + gx, origin_y + gy, set ? fg : bg);
        }
    }
}

static void scroll_one_row(void) {
    uint64_t row_bytes = (uint64_t)CELL_H * fb.pitch;
    uint8_t *base = fb.address;
    uint64_t remaining_bytes = (uint64_t)(fb.height - CELL_H) * fb.pitch;

    memmove(base, base + row_bytes, remaining_bytes);
    memset(base + remaining_bytes, 0, row_bytes);
}

void fb_console_putc(char c, uint32_t fg_rgb, uint32_t bg_rgb) {
    if (fb.address == NULL) {
        return;
    }

    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else {
        draw_glyph(cursor_col, cursor_row, c, fg_rgb, bg_rgb);
        cursor_col++;
        if (cursor_col >= console_cols) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= console_rows) {
        scroll_one_row();
        cursor_row = console_rows - 1;
    }
}

void fb_console_write(const char *str, uint32_t fg_rgb, uint32_t bg_rgb) {
    while (*str != '\0') {
        fb_console_putc(*str, fg_rgb, bg_rgb);
        str++;
    }
}
