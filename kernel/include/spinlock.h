#pragma once

#include <stdatomic.h>
#include <stdbool.h>

struct spinlock {
    atomic_flag value;
};

struct intrlock {
    struct spinlock lock;
    bool intr_flag;
};

void spinlock_init(struct spinlock* lock);
void spinlock_acquire(struct spinlock* lock);
void spinlock_release(struct spinlock* lock);

void intrlock_init(struct intrlock* lock);
void intrlock_acquire(struct intrlock* lock);
void intrlock_release(struct intrlock* lock);
