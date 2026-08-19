/* void context_switch(uint64_t **old_sp, uint64_t *new_sp);
 *
 * A context switch is just a very unusual function call: it saves the
 * SysV-ABI callee-saved registers (the caller-saved ones are already on
 * the stack or dead, same as any normal call) onto the outgoing thread's
 * stack, stashes the resulting RSP where the scheduler can find it again,
 * then loads the incoming thread's previously-stashed RSP and pops its
 * registers back -- at which point `ret` returns not to this function's
 * caller, but to wherever the *incoming* thread was when it was last
 * switched away from. First-ever runs land in thread_trampoline instead,
 * via the fake initial stack kernel/sched/thread.c builds.
 *
 * Deliberately does not touch RFLAGS/IF: whether interrupts end up
 * enabled after this returns depends entirely on what's on the stack
 * being switched to. A thread resuming through its own suspended
 * interrupt frame gets IF restored by that frame's eventual iretq;
 * thread_trampoline does its own `sti` for a thread that's never run
 * before, since it has no such frame. See docs/ARCHITECTURE.md,
 * "Scheduling", for why this is correct rather than an oversight.
 */

.text
.global context_switch
.type context_switch, @function
context_switch:
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15

    mov %rsp, (%rdi)
    mov %rsi, %rsp

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    ret

/* Entry point baked into every new thread's initial stack (thread_create,
 * kernel/sched/thread.c): r12 = entry function, r13 = its argument,
 * placed there by the fake frame at the same offsets context_switch's
 * epilogue pops them from. */
.global thread_trampoline
.type thread_trampoline, @function
.extern thread_exited
thread_trampoline:
    sti
    mov %r13, %rdi
    call *%r12
    call thread_exited
1:
    hlt
    jmp 1b
