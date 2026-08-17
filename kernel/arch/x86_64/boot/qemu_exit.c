#include "qemu_exit.h"

#include "arch/x86_64/cpu/io.h"

void qemu_debug_exit(uint8_t code) {
    outb(0xf4, code);
}
