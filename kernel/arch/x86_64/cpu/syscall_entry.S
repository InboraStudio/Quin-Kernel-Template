/* SYSCALL leaves RSP completely untouched -- unlike an interrupt gate,
 * there's no TSS.RSP0-driven automatic stack switch, so landing here
 * means %rsp still points into whatever user stack was live when the
 * user program executed `syscall`. The very first thing this does is
 * get off of it, onto a known-good kernel stack, before touching memory
 * for anything else.
 *
 * On entry: %rcx = user RIP to resume at, %r11 = user RFLAGS (both
 * loaded automatically by the CPU -- see SYSCALL in the SDM), %rax =
 * syscall number by this kernel's own convention (there's only one
 * caller of this convention right now: kernel/arch/x86_64/cpu/syscall.c's
 * eventual real syscall table, whenever a fork adds one).
 */

.text
.global syscall_entry
.type syscall_entry, @function
syscall_entry:
    mov %rsp, saved_user_rsp(%rip)
    mov kernel_syscall_rsp(%rip), %rsp

    push %rcx /* user RIP -- needed by sysretq */
    push %r11 /* user RFLAGS -- needed by sysretq */

    /* rbx/rbp/r12-r15 are untouched from here on, so whatever the user
     * program had in them survives the round trip for free: the SysV C
     * ABI call below preserves them the same way it would for any
     * ordinary function call, since neither this stub nor syscall_dispatch
     * has any reason to touch them. rdi/rsi/rdx/r10/r8/r9 (and rax) are
     * NOT preserved -- exactly the standard convention (Linux's x86_64
     * syscall ABI treats those as clobbered too), and irrelevant anyway
     * until a real syscall table needs to read arguments out of them. */
    mov %rax, %rdi
    call syscall_dispatch

    pop %r11
    pop %rcx

    mov saved_user_rsp(%rip), %rsp
    sysretq

/* void jump_to_ring3(uint64_t entry, uint64_t user_stack);
 * rdi = entry, rsi = initial user RSP (System V: first two integer args).
 *
 * The one-way trip into ring 3: builds the same five-item frame any
 * privilege-raising interrupt return needs (SS, RSP, RFLAGS, CS, RIP,
 * in that push order so iretq pops them in the order it expects) and
 * iretqs. Selector values are GDT_USER_DATA|3 (0x18|3=0x1b) and
 * GDT_USER_CODE|3 (0x20|3=0x23) -- kernel/arch/x86_64/cpu/gdt.h owns
 * those base values; hardcoded here rather than computed since this is
 * the one place outside gdt.c itself that needs to know the RPL-3
 * variants specifically. RFLAGS = 0x202: bit 1 is the always-1 reserved
 * bit, bit 9 is IF (interrupts enabled once user code starts running).
 */
.global jump_to_ring3
.type jump_to_ring3, @function
jump_to_ring3:
    mov $0x1b, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    pushq $0x1b   /* SS */
    pushq %rsi    /* RSP */
    pushq $0x202  /* RFLAGS */
    pushq $0x23   /* CS */
    pushq %rdi    /* RIP */
    iretq

.bss
.align 16
.global kernel_syscall_rsp
kernel_syscall_rsp:
    .quad 0
saved_user_rsp:
    .quad 0
