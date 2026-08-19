#include "isr.h"

#include "lapic.h"
#include "panic.h"

static interrupt_handler_fn handlers[256];

void interrupt_register_handler(uint8_t vector, interrupt_handler_fn handler) {
    handlers[vector] = handler;
}

void isr_dispatch(struct interrupt_frame *frame) {
    interrupt_handler_fn handler = handlers[frame->vector];

    if (frame->vector < 32) {
        if (handler != NULL) {
            handler(frame);
        } else {
            panic_exception(frame);
        }
        return;
    }

    if (handler != NULL) {
        handler(frame);
    }

    /* Every vector >= 32 in this kernel arrives via the LAPIC (see
     * ioapic.c), whether or not a handler claimed it -- an unhandled IRQ
     * still has to be acknowledged or the LAPIC withholds further
     * interrupts at or below its priority. */
    lapic_send_eoi();
}
