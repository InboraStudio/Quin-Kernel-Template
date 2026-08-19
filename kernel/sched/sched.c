#include "sched.h"

#include "spinlock.h"

#define SCHED_TIME_SLICE_TICKS 10 /* 10ms at timer.h's 1000Hz tick rate */

extern void context_switch(uint64_t **old_sp, uint64_t *new_sp);

static struct spinlock ready_lock = SPINLOCK_INIT;

/* Represents "whatever kmain was doing" so there's always a valid
 * `current` to save into on the very first switch -- kmain never
 * constructs this through thread_create, since it already has a stack
 * and doesn't need thread_trampoline to get started. */
static struct thread bootstrap_thread;
static struct thread *current = &bootstrap_thread;
static uint64_t next_thread_id = 1;
static uint64_t ticks_since_switch;

void sched_init(void) {
    bootstrap_thread.id = 0;
    bootstrap_thread.next = &bootstrap_thread;
    current = &bootstrap_thread;
}

void sched_add_thread(struct thread *thread) {
    uint64_t flags = spinlock_acquire_irqsave(&ready_lock);
    thread->id = next_thread_id++;
    thread->next = current->next;
    current->next = thread;
    spinlock_release_irqrestore(&ready_lock, flags);
}

static void switch_to_next(void) {
    uint64_t flags = spinlock_acquire_irqsave(&ready_lock);
    struct thread *prev = current;
    struct thread *next = current->next;
    current = next;
    spinlock_release_irqrestore(&ready_lock, flags);

    ticks_since_switch = 0;
    if (next != prev) {
        context_switch(&prev->stack_pointer, next->stack_pointer);
    }
}

void sched_tick(void) {
    if (++ticks_since_switch >= SCHED_TIME_SLICE_TICKS) {
        switch_to_next();
    }
}

void sched_yield(void) {
    switch_to_next();
}
