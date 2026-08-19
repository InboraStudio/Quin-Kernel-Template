#ifndef ARCH_X86_64_CPU_ISR_H
#define ARCH_X86_64_CPU_ISR_H

#include "kernel.h"

/* Field order matches isr_common_stub's push order exactly (isr_stubs.S)
 * -- rsp points at r15 (the last register pushed) when isr_dispatch
 * receives this pointer, so r15 has to be the first field. rip/cs/rflags
 * are what the CPU itself pushed on entry; no rsp/ss, since nothing in
 * this kernel triggers a stack switch on interrupt yet (no ring 3, no
 * IST) -- see isr_stubs.S's header comment. */
struct interrupt_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
};

typedef void (*interrupt_handler_fn)(struct interrupt_frame *frame);

/* Registers a handler for one IDT vector (0-255).
 *
 * Vectors 0-31 are CPU exceptions. Leaving one unregistered means it
 * panics -- that's the correct default for Phase 1, where nothing
 * recovers from a fault yet. Phase 2 registers vector 14 (#PF) to
 * distinguish a real fault from a lazy-mapping opportunity instead of
 * always panicking.
 *
 * Vectors 32-47 are the legacy IRQ lines 0-15, routed through the IOAPIC
 * (see ioapic.c) rather than the 8259 PIC. An unregistered IRQ is EOI'd
 * and otherwise ignored -- see isr_dispatch in isr.c. */
void interrupt_register_handler(uint8_t vector, interrupt_handler_fn handler);

/* Entry point called by isr_common_stub (isr_stubs.S). Not meant to be
 * called from C; declared here only because the assembly needs the
 * mangled name to match. */
void isr_dispatch(struct interrupt_frame *frame);

#endif
