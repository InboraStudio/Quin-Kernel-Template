#ifndef SCHED_SPINLOCK_H
#define SCHED_SPINLOCK_H

#include <stdint.h>

/* A real atomic spinlock, not a cli/sti stand-in -- this kernel only
 * ever runs one CPU today (see docs/ROADMAP.md, SMP), so a plain
 * disable-interrupts section would be equally correct and cheaper, but
 * it would also be a lie about what "spinlock" means once a second core
 * shows up. Written to already be SMP-correct. */
struct spinlock {
    volatile uint32_t locked;
};

#define SPINLOCK_INIT {.locked = 0}

void spinlock_init(struct spinlock *lock);

/* Plain acquire/release: correct for locks never touched from interrupt
 * context. Anything the scheduler's timer-tick path might also touch
 * (the ready queue) needs the _irqsave variants below instead, or a
 * same-CPU interrupt could fire while the lock is held and spin forever
 * trying to acquire a lock its own interrupted code already owns. */
void spinlock_acquire(struct spinlock *lock);
void spinlock_release(struct spinlock *lock);

/* Disables interrupts before acquiring and returns the prior RFLAGS (so
 * a nested acquire elsewhere doesn't clobber a caller's own saved
 * state); spinlock_release_irqrestore restores exactly that state
 * rather than unconditionally re-enabling interrupts, so nesting inside
 * an already-cli'd section stays cli'd. */
uint64_t spinlock_acquire_irqsave(struct spinlock *lock);
void spinlock_release_irqrestore(struct spinlock *lock, uint64_t flags);

#endif
