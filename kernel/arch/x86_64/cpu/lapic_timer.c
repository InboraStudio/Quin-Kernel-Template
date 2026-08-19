#include "lapic_timer.h"

#include "io.h"
#include "lapic.h"

#define PIT_FREQUENCY_HZ 1193182
#define PIT_CHANNEL2_PORT 0x42
#define PIT_COMMAND_PORT 0x43
#define PIT_GATE_SPEAKER_PORT 0x61

#define CALIBRATION_MS 10

/* OSDev Wiki: https://wiki.osdev.org/APIC_timer, "Determining the APIC
 * timer frequency". PIT channel 2 (not channel 0) specifically: its
 * output is directly pollable via bit 5 of port 0x61 (the legacy
 * "speaker gate" port), so calibration never needs IRQ0 wired up at
 * all -- channel 0's output only reaches the PIC/IOAPIC, which would
 * mean routing and unmasking an interrupt just to throw it away. */
uint32_t lapic_timer_calibrate_and_start(uint8_t vector, uint32_t frequency_hz) {
    lapic_timer_set_divide_16();
    lapic_timer_set_lvt(vector, true, true); /* periodic, masked while calibrating */

    uint16_t pit_count = (uint16_t)((PIT_FREQUENCY_HZ / 1000) * CALIBRATION_MS);

    outb(PIT_GATE_SPEAKER_PORT, (uint8_t)((inb(PIT_GATE_SPEAKER_PORT) & 0xfc) | 0x01));
    outb(PIT_COMMAND_PORT, 0xb0); /* channel 2, lobyte/hibyte access, mode 0, binary */
    outb(PIT_CHANNEL2_PORT, (uint8_t)(pit_count & 0xff));
    outb(PIT_CHANNEL2_PORT, (uint8_t)(pit_count >> 8));

    lapic_timer_set_initial_count(0xffffffff);

    while (!(inb(PIT_GATE_SPEAKER_PORT) & 0x20)) {}

    uint32_t elapsed_ticks = 0xffffffff - lapic_timer_get_current_count();
    uint32_t lapic_frequency_hz = elapsed_ticks * (1000 / CALIBRATION_MS);

    uint32_t periodic_count = lapic_frequency_hz / frequency_hz;
    lapic_timer_set_initial_count(periodic_count);
    lapic_timer_set_lvt(vector, true, false); /* periodic, unmasked */

    return lapic_frequency_hz;
}
