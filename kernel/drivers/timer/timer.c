#include "timer.h"

#include "arch/x86_64/cpu/isr.h"
#include "arch/x86_64/cpu/lapic_timer.h"
#include "lib/log.h"

static volatile uint64_t ticks;

static void timer_tick_handler(struct interrupt_frame *frame) {
    (void)frame;
    ticks++;
}

void timer_init(void) {
    interrupt_register_handler(TIMER_VECTOR, timer_tick_handler);
    uint32_t measured_hz = lapic_timer_calibrate_and_start(TIMER_VECTOR, TIMER_FREQUENCY_HZ);
    log_info("timer: lapic calibrated at %u Hz, ticking at %u Hz", measured_hz,
             TIMER_FREQUENCY_HZ);
}

uint64_t timer_get_ticks(void) {
    return ticks;
}
