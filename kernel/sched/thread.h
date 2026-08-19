#ifndef SCHED_THREAD_H
#define SCHED_THREAD_H

#include <stdint.h>

struct thread {
    /* Valid only while this thread isn't the one running -- context_switch
     * (kernel/arch/x86_64/cpu/context_switch.S) writes it on the way out
     * and reads it on the way back in. */
    uint64_t *stack_pointer;
    void *stack_base;   /* for the guard-paged allocation this came from */
    uint64_t id;
    struct thread *next; /* circular ready-queue link -- see kernel/sched/sched.h */
};

typedef void (*thread_entry_fn)(void *arg);

/* Allocates a thread with its own guard-paged stack (kernel/arch/x86_64/mm/vmm.h's
 * vmm_alloc_guarded) and adds it to the scheduler's ready queue. Returns
 * NULL on allocation failure. `entry` is expected to never return --
 * see thread_exited's doc comment in thread.c for what happens if it
 * does. */
struct thread *thread_create(thread_entry_fn entry, void *arg);

#endif
