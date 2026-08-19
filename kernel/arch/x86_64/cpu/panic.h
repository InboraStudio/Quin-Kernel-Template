#ifndef ARCH_X86_64_CPU_PANIC_H
#define ARCH_X86_64_CPU_PANIC_H

#include "isr.h"
#include "kernel.h"

/* Unhandled CPU exception: full register dump (from the trapped frame)
 * plus CR2 for page faults, then a stack trace. Called from isr_dispatch
 * (isr.c) -- not meant to be called directly. */
NORETURN void panic_exception(struct interrupt_frame *frame);

/* Fatal condition detected by kernel code itself (an assertion, an
 * allocator that's out of memory it has no way to recover from, ...).
 * No register frame to dump, but still prints a stack trace starting
 * from the call site. printf-style. */
NORETURN void panic(const char *fmt, ...);

#endif
