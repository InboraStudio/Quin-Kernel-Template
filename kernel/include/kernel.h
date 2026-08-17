#ifndef KERNEL_H
#define KERNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NORETURN __attribute__((noreturn))
#define PACKED __attribute__((packed))
#define ALIGNED(n) __attribute__((aligned(n)))
#define UNUSED __attribute__((unused))

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define CONTAINER_OF(ptr, type, member) \
    ((type *)((uint8_t *)(ptr) - offsetof(type, member)))

static inline uint64_t align_up(uint64_t value, uint64_t align) {
    return (value + (align - 1)) & ~(align - 1);
}

static inline uint64_t align_down(uint64_t value, uint64_t align) {
    return value & ~(align - 1);
}

NORETURN void kernel_halt(void);

/* Arch-independent kernel entry, called once by kernel/arch/<arch>/boot/
 * after the bootloader hand-off is verified. Never returns. */
void kmain(void);

#endif
