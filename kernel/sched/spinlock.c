#include "spinlock.h"

void spinlock_init(struct spinlock *lock) {
    lock->locked = 0;
}

void spinlock_acquire(struct spinlock *lock) {
    /* Test-and-test-and-set: the inner loop only reads (no atomic RMW),
     * so a CPU spinning on a held lock doesn't keep forcing the cache
     * line into shared/invalid on every iteration -- it only attempts
     * the actual atomic exchange once the plain read suggests the lock
     * is free. `pause` hints the CPU this is a spin-wait, reducing power
     * draw and the penalty of a mispredicted exit from the loop. */
    while (__atomic_exchange_n(&lock->locked, 1, __ATOMIC_ACQUIRE)) {
        while (lock->locked) {
            __asm__ volatile("pause");
        }
    }
}

void spinlock_release(struct spinlock *lock) {
    __atomic_store_n(&lock->locked, 0, __ATOMIC_RELEASE);
}

static inline uint64_t read_and_disable_interrupts(void) {
    uint64_t flags;
    __asm__ volatile("pushfq\n\t"
                      "pop %0\n\t"
                      "cli"
                      : "=r"(flags)
                      :
                      : "memory");
    return flags;
}

static inline void restore_interrupt_state(uint64_t flags) {
    __asm__ volatile("push %0\n\t"
                      "popfq"
                      :
                      : "r"(flags)
                      : "memory", "cc");
}

uint64_t spinlock_acquire_irqsave(struct spinlock *lock) {
    uint64_t flags = read_and_disable_interrupts();
    spinlock_acquire(lock);
    return flags;
}

void spinlock_release_irqrestore(struct spinlock *lock, uint64_t flags) {
    spinlock_release(lock);
    restore_interrupt_state(flags);
}
