#include "thread.h"

#include "arch/x86_64/cpu/panic.h"
#include "arch/x86_64/mm/vmm.h"
#include "kernel.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "sched.h"

#define THREAD_STACK_PAGES 4 /* 16KiB -- plenty for the demo threads this template ships with */

extern void thread_trampoline(void);

struct thread *thread_create(thread_entry_fn entry, void *arg) {
    struct thread *t = kmalloc(sizeof(struct thread));
    if (t == NULL) {
        return NULL;
    }

    uint8_t *stack = vmm_alloc_guarded(THREAD_STACK_PAGES, VMM_WRITABLE);
    if (stack == NULL) {
        kfree(t);
        return NULL;
    }

    /* Builds the fake initial stack frame context_switch's epilogue
     * expects: six register slots it will `pop` (contents irrelevant
     * except r12/r13, which land in the real r12/r13 by the time
     * thread_trampoline runs) followed by a return address. Pushed
     * high-to-low so the *last* write here ends up at the lowest
     * address -- i.e. where stack_pointer must point, since that's what
     * context_switch loads into %rsp before its first `pop`. */
    uint64_t *sp = (uint64_t *)(stack + THREAD_STACK_PAGES * PAGE_SIZE);
    *(--sp) = (uint64_t)(uintptr_t)thread_trampoline; /* return address */
    *(--sp) = 0;                                      /* rbp */
    *(--sp) = 0;                                      /* rbx */
    *(--sp) = (uint64_t)(uintptr_t)entry;             /* r12 */
    *(--sp) = (uint64_t)(uintptr_t)arg;               /* r13 */
    *(--sp) = 0;                                      /* r14 */
    *(--sp) = 0;                                      /* r15 */

    t->stack_pointer = sp;
    t->stack_base = stack;

    sched_add_thread(t);
    return t;
}

/* Called from thread_trampoline (context_switch.S) if a thread's entry
 * function ever returns. There's no thread-exit/reaping mechanism yet
 * (see docs/ROADMAP.md) -- a thread is expected to run an unbounded
 * loop, not fall off the end -- so this is a bug backstop, not a
 * supported exit path. */
NORETURN void thread_exited(void) {
    panic("thread: entry function returned (thread exit is not implemented)");
}
