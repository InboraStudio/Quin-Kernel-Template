#ifndef LIB_PRINTF_H
#define LIB_PRINTF_H

#include <stdarg.h>
#include <stddef.h>

typedef void (*kputc_fn)(void *ctx, char c);

/* Formatted output core. Every other formatting entry point in the kernel
 * (serial, framebuffer console, the leveled logger in kernel/lib/log.c)
 * funnels through this so there is exactly one format-string parser to
 * trust. Supports: %d %i %u %x %X %p %s %c %%, the l/ll length modifiers,
 * a '0' zero-pad flag, and a decimal width. Nothing else — no floats, no
 * '*' dynamic width, no POSIX positional args. Extend here if a future
 * phase needs more, rather than adding a second formatter. */
void kvprintf(kputc_fn emit, void *ctx, const char *fmt, va_list ap);

/* Writes into a fixed buffer, always NUL-terminating if size > 0.
 * Returns the number of characters that would have been written had the
 * buffer been unbounded (snprintf convention), so callers can detect
 * truncation. */
int ksnprintf(char *buf, size_t size, const char *fmt, ...);

#endif
