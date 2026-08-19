#ifndef ARCH_X86_64_CPU_GDT_H
#define ARCH_X86_64_CPU_GDT_H

/* Selector values (index << 3 | RPL). Fixed order matters: SYSCALL/SYSRET
 * (Phase 5) derive CS/SS from the STAR MSR using offsets from a single
 * base selector, which only works if kernel code/data and user data/code
 * sit in exactly this order -- see gdt.c. */
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA 0x18
#define GDT_USER_CODE 0x20
#define GDT_TSS 0x28

void gdt_init(void);

/* Updates the stack pointer the CPU loads on a ring3->ring0 transition
 * (interrupt or syscall while running userspace code). Unused until
 * Phase 5, but the TSS has to exist -- and TR has to be loaded -- before
 * then regardless, so it's set up now. */
void gdt_set_kernel_stack(void *rsp0);

#endif
