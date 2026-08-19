#ifndef DRIVERS_TIMER_H
#define DRIVERS_TIMER_H

#include <stdint.h>

#define TIMER_VECTOR \
    32 /* IRQ0's vector -- delivered directly by the LAPIC, not through the IOAPIC */
#define TIMER_FREQUENCY_HZ 1000

/* Calibrates and starts the LAPIC timer (kernel/arch/x86_64/cpu/lapic_timer.h)
 * at TIMER_FREQUENCY_HZ and registers the tick-counting interrupt
 * handler. Requires lapic_init() to have already run. */
void timer_init(void);

/* Ticks since timer_init. At TIMER_FREQUENCY_HZ, one tick is 1ms. */
uint64_t timer_get_ticks(void);

#endif
