#include "serial.h"

#include <stdbool.h>
#include <stdint.h>

#include "arch/x86_64/cpu/io.h"

/* COM1. QEMU always wires this to -serial stdio; real hardware may not have
 * it, hence the loopback self-test below before we trust the port.
 * OSDev Wiki: https://wiki.osdev.org/Serial_Ports */
#define COM1 0x3f8

#define REG_DATA 0
#define REG_INT_ENABLE 1
#define REG_FIFO_CTRL 2
#define REG_LINE_CTRL 3
#define REG_MODEM_CTRL 4
#define REG_LINE_STATUS 5

static bool serial_present;

void serial_init(void) {
    outb(COM1 + REG_INT_ENABLE, 0x00);

    outb(COM1 + REG_LINE_CTRL, 0x80);
    outb(COM1 + REG_DATA, 0x03);
    outb(COM1 + REG_INT_ENABLE, 0x00);
    outb(COM1 + REG_LINE_CTRL, 0x03);
    outb(COM1 + REG_FIFO_CTRL, 0xc7);
    outb(COM1 + REG_MODEM_CTRL, 0x0b);

    outb(COM1 + REG_MODEM_CTRL, 0x1e);
    outb(COM1 + REG_DATA, 0xae);
    serial_present = inb(COM1 + REG_DATA) == 0xae;

    outb(COM1 + REG_MODEM_CTRL, 0x0f);
}

static bool transmit_empty(void) {
    return (inb(COM1 + REG_LINE_STATUS) & 0x20) != 0;
}

void serial_putc(char c) {
    if (!serial_present) {
        return;
    }
    if (c == '\n') {
        serial_putc('\r');
    }
    while (!transmit_empty()) {}
    outb(COM1 + REG_DATA, (uint8_t)c);
}

void serial_write(const char *str) {
    while (*str != '\0') {
        serial_putc(*str);
        str++;
    }
}

void serial_write_n(const char *str, unsigned long len) {
    for (unsigned long i = 0; i < len; i++) {
        serial_putc(str[i]);
    }
}
