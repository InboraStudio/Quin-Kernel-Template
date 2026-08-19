#include "keyboard.h"

#include "arch/x86_64/cpu/io.h"
#include "arch/x86_64/cpu/isr.h"
#include "kernel.h"

#define PS2_DATA_PORT 0x60

#define SCANCODE_LEFT_SHIFT 0x2a
#define SCANCODE_RIGHT_SHIFT 0x36
#define SCANCODE_RELEASE_BIT 0x80

/* Scan Code Set 1 (the PS/2 keyboard controller's power-on default --
 * OSDev Wiki: https://wiki.osdev.org/Keyboard#Scan_Code_Set_1), make
 * codes 0x00-0x39 only: the alphanumeric block, punctuation, space,
 * enter, tab, and backspace. F-keys, the numpad, arrow keys, and the
 * E0-prefixed extended codes (right Ctrl/Alt, cursor cluster, ...) are
 * not decoded -- KEYBOARD_VECTOR's handler just drops any scancode this
 * table maps to 0. A fork wanting those needs to track the 0xE0 prefix
 * byte as a second piece of state alongside `shift_held` below. */
static const char unshifted_table[0x3a] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0a] = '9', [0x0b] = '0',
    [0x0c] = '-', [0x0d] = '=', [0x0e] = '\b', [0x0f] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1a] = '[', [0x1b] = ']', [0x1c] = '\n',
    [0x1e] = 'a', [0x1f] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    [0x27] = ';', [0x28] = '\'', [0x29] = '`', [0x2b] = '\\',
    [0x2c] = 'z', [0x2d] = 'x', [0x2e] = 'c', [0x2f] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x39] = ' ',
};

static const char shifted_table[0x3a] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0a] = '(', [0x0b] = ')',
    [0x0c] = '_', [0x0d] = '+', [0x0e] = '\b', [0x0f] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1a] = '{', [0x1b] = '}', [0x1c] = '\n',
    [0x1e] = 'A', [0x1f] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L',
    [0x27] = ':', [0x28] = '"', [0x29] = '~', [0x2b] = '|',
    [0x2c] = 'Z', [0x2d] = 'X', [0x2e] = 'C', [0x2f] = 'V', [0x30] = 'B',
    [0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>', [0x35] = '?',
    [0x39] = ' ',
};

#define BUFFER_SIZE 256

static char buffer[BUFFER_SIZE];
static uint32_t buffer_head;
static uint32_t buffer_tail;
static bool shift_held;

static void buffer_push(char c) {
    uint32_t next = (buffer_head + 1) % BUFFER_SIZE;
    if (next == buffer_tail) {
        return; /* full: drop the newest keystroke rather than overwrite an unread one */
    }
    buffer[buffer_head] = c;
    buffer_head = next;
}

static void keyboard_isr(struct interrupt_frame *frame) {
    (void)frame;

    uint8_t scancode = inb(PS2_DATA_PORT);
    bool released = (scancode & SCANCODE_RELEASE_BIT) != 0;
    uint8_t code = scancode & (uint8_t)~SCANCODE_RELEASE_BIT;

    if (code == SCANCODE_LEFT_SHIFT || code == SCANCODE_RIGHT_SHIFT) {
        shift_held = !released;
        return;
    }
    if (released || code >= ARRAY_LEN(unshifted_table)) {
        return;
    }

    char c = shift_held ? shifted_table[code] : unshifted_table[code];
    if (c != '\0') {
        buffer_push(c);
    }
}

void keyboard_init(void) {
    interrupt_register_handler(KEYBOARD_VECTOR, keyboard_isr);
}

char keyboard_read_char(void) {
    if (buffer_tail == buffer_head) {
        return '\0';
    }
    char c = buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
    return c;
}