#ifndef LIB_LOG_H
#define LIB_LOG_H

/* Terse, factual, dmesg-style output -- no hype language, no exclamation
 * marks. "[INFO] apic: calibrated at 998244353 Hz", not "APIC ready!". */

enum log_level {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
};

/* Writes to serial always, and to the framebuffer console if one is
 * available (see fb_is_available in drivers/framebuffer/framebuffer.h).
 * Below the minimum level (log_set_min_level), a call is a no-op --
 * still safe to call from anywhere, including before the framebuffer or
 * even serial exists (serial_putc/fb_console_putc are themselves safe
 * no-ops until initialized). */
void log_write(enum log_level level, const char *fmt, ...);

void log_set_min_level(enum log_level level);

#define log_debug(...) log_write(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define log_info(...) log_write(LOG_LEVEL_INFO, __VA_ARGS__)
#define log_warn(...) log_write(LOG_LEVEL_WARN, __VA_ARGS__)

#endif
