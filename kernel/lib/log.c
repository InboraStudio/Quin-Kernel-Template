#include "log.h"

#include "drivers/framebuffer/framebuffer.h"
#include "drivers/serial/serial.h"
#include "printf.h"

static enum log_level min_level = LOG_LEVEL_DEBUG;

struct level_style {
    const char *tag;
    uint32_t fb_color;
};

static const struct level_style styles[] = {
    [LOG_LEVEL_DEBUG] = {"DEBUG", FB_GRAY},
    [LOG_LEVEL_INFO] = {"INFO", FB_WHITE},
    [LOG_LEVEL_WARN] = {"WARN", FB_YELLOW},
};

static uint32_t current_fb_color;

static void log_emit(void *ctx, char c) {
    (void)ctx;
    serial_putc(c);
    if (fb_is_available()) {
        fb_console_putc(c, current_fb_color, FB_BLACK);
    }
}

void log_set_min_level(enum log_level level) {
    min_level = level;
}

void log_write(enum log_level level, const char *fmt, ...) {
    if (level < min_level) {
        return;
    }

    const struct level_style *style = &styles[level];
    current_fb_color = style->fb_color;

    log_emit(NULL, '[');
    for (const char *p = style->tag; *p != '\0'; p++) {
        log_emit(NULL, *p);
    }
    log_emit(NULL, ']');
    log_emit(NULL, ' ');

    va_list ap;
    va_start(ap, fmt);
    kvprintf(log_emit, NULL, fmt, ap);
    va_end(ap);

    log_emit(NULL, '\n');
}
