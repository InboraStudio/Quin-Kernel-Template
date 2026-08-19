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

    /* EOI'd before the handler runs, not after: every vector >= 32
     * arrives via the LAPIC (see ioapic.c), and an unhandled IRQ still
     * has to be acknowledged or the LAPIC withholds further interrupts
     * of that vector. That alone would be just as correct done after
     * the handler returns -- what forces EOI first is the scheduler's
     * timer tick (kernel/sched/sched.c), whose handler can context-switch
     * away entirely instead of returning normally. A thread scheduled
     * in for the first time re-enables interrupts in thread_trampoline
     * (kernel/arch/x86_64/cpu/context_switch.S) before this vector's
     * in-service bit would otherwise have been cleared, and the LAPIC
     * silently withholds every subsequent tick of the same vector until
     * it is -- which, since nothing switches back to the thread whose
     * stack the deferred EOI call is buried in without a working timer,
     * is a permanent deadlock, not a delay. Discovered by hitting it: an
     * earlier version EOI'd after the handler and a preemption demo hung
     * indefinitely. */
    lapic_send_eoi();

    if (handler != NULL) {
        handler(frame);
    }
}
