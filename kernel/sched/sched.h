#ifndef SCHED_SCHED_H
#define SCHED_SCHED_H

#include "thread.h"

/* Wraps the kernel's current execution (kmain's continuation) as thread
 * 0 and makes it `current`, so there's always a valid thread to save
 * into when the first real switch happens. Call once, before any
 * thread_create. */
void sched_init(void);

/* Inserts `thread` into the ready ring just after the currently running
 * thread. Called by thread_create (kernel/sched/thread.c) -- not
 * meant to be called directly with a thread that isn't fresh out of
 * thread_create. */
void sched_add_thread(struct thread *thread);

/* Called from the timer ISR (kernel/drivers/timer/timer.c) on every
 * tick. Preempts to the next ready thread once a full time slice
 * (SCHED_TIME_SLICE_TICKS in sched.c) has elapsed since the last
 * switch. */
void sched_tick(void);

/* Voluntary preemption: switches to the next ready thread immediately,
 * resetting the time-slice counter, regardless of how much of the
 * current slice remains. */
void sched_yield(void);

#endif
