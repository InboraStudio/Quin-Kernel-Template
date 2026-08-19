#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include <stdint.h>

#define KEYBOARD_IRQ 1
#define KEYBOARD_VECTOR (32 + KEYBOARD_IRQ)

/* Registers the IRQ1 handler. Routing IRQ1 through the IOAPIC to
 * KEYBOARD_VECTOR (ioapic_set_irq) is kmain's job, same as every other
 * IOAPIC-routed device -- this only owns the PS/2 side. Assumes the
 * 8042 controller and its keyboard port are already enabled, which
 * every checked QEMU machine type does by firmware default; a real
 * driver aiming at unknown hardware would probe and initialize the
 * controller itself first. */
void keyboard_init(void);

/* Returns the next buffered character, or 0 if none is available.
 * Non-blocking. Only printable ASCII plus '\n', '\b', and '\t' are ever
 * produced -- modifier keys (Ctrl, Alt, Caps Lock) and non-alphanumeric
 * keys (F-keys, arrows, numpad) are tracked internally where needed
 * (Shift) or silently dropped otherwise. */
char keyboard_read_char(void);

#endif
