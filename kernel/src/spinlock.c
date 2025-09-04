#include "spinlock.h"
#include "arch/inst.h"

#include <freec/assert.h>

void spinlock_init(struct spinlock* lock) {
    atomic_flag_clear(&lock->value);
}

void spinlock_acquire(struct spinlock* lock) {
    while (atomic_flag_test_and_set(&lock->value)) {

        // in single-threaded context, spinlock must be always unlocked
        panic("spinlock_acquire(): deadlock detected");

        spinloop_hint();
    }
}

void spinlock_release(struct spinlock* lock) {
    atomic_flag_clear(&lock->value);
}

void intrlock_init(struct intrlock* lock) {
    spinlock_init(&lock->lock);
    lock->intr_flag = false;
}

void intrlock_acquire(struct intrlock* lock) {
    lock->intr_flag = interrupt_is_enabled();
    interrupt_disable();
    spinlock_acquire(&lock->lock);
}

void intrlock_release(struct intrlock* lock) {
    spinlock_release(&lock->lock);
    if (lock->intr_flag) {
        interrupt_enable();
    }
}
