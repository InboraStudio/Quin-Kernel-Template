#include "syscall.h"

#include "arch/x86_64/mm/vmm.h"
#include "gdt.h"
#include "io.h"
#include "lib/log.h"
#include "mm/pmm.h"
#include "panic.h"

/* SDM Vol. 2B, "SYSCALL"/"SYSRET". SYSCALL loads CS from STAR[47:32] and
 * SS from STAR[47:32]+8; SYSRET (64-bit) loads CS from STAR[63:48]+16
 * and SS from STAR[63:48]+8, RPL forced to 3. That arithmetic is exactly
 * why kernel/arch/x86_64/cpu/gdt.c orders the GDT user-data-then-user-code
 * and puts kernel data right after kernel code: STAR[47:32] = kernel
 * code selector and STAR[63:48] = kernel data selector makes both
 * formulas land on the right descriptors. */
#define IA32_EFER 0xc0000080
#define IA32_STAR 0xc0000081
#define IA32_LSTAR 0xc0000082
#define IA32_FMASK 0xc0000084

#define EFER_SCE (1ULL << 0)

/* RFLAGS bits cleared on SYSCALL entry, before the kernel has had any
 * chance to run: IF, so an interrupt can't land while syscall_entry
 * (syscall_entry.S) is mid-way through swapping onto the kernel stack;
 * TF, so a debugger single-stepping user code can't trap into a kernel
 * entry point it has no business stopping inside. */
#define FMASK_IF_TF 0x300

#define SYSCALL_KERNEL_STACK_PAGES 4

extern void syscall_entry(void);
extern uint64_t kernel_syscall_rsp; /* defined in syscall_entry.S; set once, below */

#define ENOSYS 38 /* matches the standard POSIX/Linux errno value, though this kernel has no broader errno story yet */

int64_t syscall_dispatch(uint64_t syscall_number) {
    log_warn("syscall: number %lu is not implemented (ENOSYS)", syscall_number);
    return -ENOSYS;
}

void syscall_init(void) {
    uint8_t *stack = vmm_alloc_guarded(SYSCALL_KERNEL_STACK_PAGES, VMM_WRITABLE);
    if (stack == NULL) {
        panic("syscall: failed to allocate the kernel-entry stack");
    }
    uint64_t stack_top = (uint64_t)(uintptr_t)(stack + SYSCALL_KERNEL_STACK_PAGES * PAGE_SIZE);

    /* Doubles as the ring3->ring0 stack for ordinary interrupts/exceptions
     * (SDM Vol. 3A, 8.5: TSS.RSP0 is what the CPU loads on any
     * privilege-raising interrupt, not just page faults from user code)
     * and the one syscall_entry.S manually swaps to for SYSCALL, which
     * doesn't consult the TSS at all. One shared stack for both paths is
     * correct as long as at most one ring-3 excursion is ever in flight
     * at a time -- true for this template's single demo thread, not
     * something to rely on with more than one. */
    gdt_set_kernel_stack((void *)(uintptr_t)stack_top);
    kernel_syscall_rsp = stack_top;

    uint64_t efer = rdmsr(IA32_EFER);
    wrmsr(IA32_EFER, efer | EFER_SCE);

    uint64_t star = ((uint64_t)GDT_KERNEL_DATA << 48) | ((uint64_t)GDT_KERNEL_CODE << 32);
    wrmsr(IA32_STAR, star);
    wrmsr(IA32_LSTAR, (uint64_t)(uintptr_t)syscall_entry);
    wrmsr(IA32_FMASK, FMASK_IF_TF);

    log_info("syscall: SYSCALL/SYSRET enabled, entry=%p", (void *)(uintptr_t)syscall_entry);
}
