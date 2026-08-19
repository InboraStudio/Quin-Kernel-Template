#ifndef ARCH_X86_64_CPU_SYSCALL_H
#define ARCH_X86_64_CPU_SYSCALL_H

#include <stdint.h>

#include "kernel.h"

/* This is where the template ends and your kernel begins. syscall_init
 * wires up SYSCALL/SYSRET and gives every call number -ENOSYS -- there
 * is no syscall table, no argument marshaling convention beyond what
 * the CPU itself defines, and no per-syscall permission model. Designing
 * that ABI is the actual work of writing a kernel on top of this
 * template, not something a template can decide on your behalf. See
 * docs/ROADMAP.md.
 *
 * Requires gdt_init() (for the TSS this allocates a kernel stack into)
 * and pmm_init()/vmm_init() (for that allocation) to have already run. */
void syscall_init(void);

/* One-way transition into ring 3: constructs an iretq frame by hand and
 * jumps. Never returns to its caller -- the only way back to ring 0
 * from here is whatever the user code at `entry` does (a syscall, or a
 * fault that lands in panic_exception). `user_stack` is the initial
 * RSP; since the stack grows down, pass the *top* of whatever range you
 * mapped, not its base. */
NORETURN void jump_to_ring3(uint64_t entry, uint64_t user_stack);

#endif
