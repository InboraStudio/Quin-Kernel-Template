#include "printf.h"

#include <stdbool.h>
#include <stdint.h>

#include "string.h"

static void emit_padded(kputc_fn emit, void *ctx, const char *digits, size_t len, int width,
                         bool zero_pad) {
    int pad = width - (int)len;
    for (int i = 0; i < pad; i++) {
        emit(ctx, zero_pad ? '0' : ' ');
    }
    for (size_t i = 0; i < len; i++) {
        emit(ctx, digits[i]);
    }
}

static void emit_unsigned(kputc_fn emit, void *ctx, uint64_t value, unsigned base, bool upper,
                           int width, bool zero_pad) {
    static const char lower[] = "0123456789abcdef";
    static const char upper_digits[] = "0123456789ABCDEF";
    const char *table = upper ? upper_digits : lower;

    char digits[32];
    size_t len = 0;
    do {
        digits[len++] = table[value % base];
        value /= base;
    } while (value != 0 && len < sizeof(digits));

    char reversed[32];
    for (size_t i = 0; i < len; i++) {
        reversed[i] = digits[len - 1 - i];
    }

    emit_padded(emit, ctx, reversed, len, width, zero_pad);
}

static void emit_signed(kputc_fn emit, void *ctx, int64_t value, int width, bool zero_pad) {
    if (value < 0) {
        emit(ctx, '-');
        uint64_t magnitude = (uint64_t)(-(value + 1)) + 1;
        emit_unsigned(emit, ctx, magnitude, 10, false, width > 0 ? width - 1 : 0, zero_pad);
    } else {
        emit_unsigned(emit, ctx, (uint64_t)value, 10, false, width, zero_pad);
    }
}

void kvprintf(kputc_fn emit, void *ctx, const char *fmt, va_list ap) {
    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            emit(ctx, *p);
            continue;
        }

        p++;
        if (*p == '\0') {
            break;
        }

        bool zero_pad = false;
        if (*p == '0') {
            zero_pad = true;
            p++;
        }

        int width = 0;
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        int length = 0;
        while (*p == 'l') {
            length++;
            p++;
        }

        switch (*p) {
        case 'd':
        case 'i': {
            int64_t value = length > 0 ? va_arg(ap, long) : va_arg(ap, int);
            emit_signed(emit, ctx, value, width, zero_pad);
            break;
        }
        case 'u': {
            uint64_t value = length > 0 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            emit_unsigned(emit, ctx, value, 10, false, width, zero_pad);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t value = length > 0 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            emit_unsigned(emit, ctx, value, 16, *p == 'X', width, zero_pad);
            break;
        }
        case 'p': {
            void *ptr = va_arg(ap, void *);
            emit(ctx, '0');
            emit(ctx, 'x');
            emit_unsigned(emit, ctx, (uint64_t)(uintptr_t)ptr, 16, false, 16, true);
            break;
        }
        case 's': {
            const char *str = va_arg(ap, const char *);
            if (str == NULL) {
                str = "(null)";
            }
            emit_padded(emit, ctx, str, strlen(str), width, false);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            emit(ctx, c);
            break;
        }
        case '%':
            emit(ctx, '%');
            break;
        default:
            emit(ctx, '%');
            emit(ctx, *p);
            break;
        }
    }
}

struct snprintf_ctx {
    char *buf;
    size_t size;
    size_t written;
};

static void snprintf_emit(void *raw_ctx, char c) {
    struct snprintf_ctx *ctx = raw_ctx;
    if (ctx->written + 1 < ctx->size) {
        ctx->buf[ctx->written] = c;
    }
    ctx->written++;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
    struct snprintf_ctx ctx = {.buf = buf, .size = size, .written = 0};

    va_list ap;
    va_start(ap, fmt);
    kvprintf(snprintf_emit, &ctx, fmt, ap);
    va_end(ap);

    if (size > 0) {
        size_t term = ctx.written < size - 1 ? ctx.written : size - 1;
        buf[term] = '\0';
    }

    return (int)ctx.written;
}
