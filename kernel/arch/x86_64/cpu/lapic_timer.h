#ifndef ARCH_X86_64_CPU_LAPIC_TIMER_H
#define ARCH_X86_64_CPU_LAPIC_TIMER_H

#include <stdint.h>

/* Calibrates the LAPIC timer's actual tick frequency against the PIT
 * (channel 2, polled -- see lapic_timer.c), then programs it for
 * periodic interrupts at `frequency_hz` on `vector`. Requires lapic_init()
 * to have already run. Returns the measured LAPIC timer frequency in Hz,
 * mostly useful for logging. */
uint32_t lapic_timer_calibrate_and_start(uint8_t vector, uint32_t frequency_hz);

#endif
